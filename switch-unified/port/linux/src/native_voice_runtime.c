#include "native_voice_runtime.h"

#include "native_voice_wire.h"
#include "p2p.h"
#include "port_config.h"
#include "native_voice_session.h"
#include "platform.h"

#include <SDL3/SDL.h>
#include <pthread.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#define VOICE_CAPTURE_CHUNK 2048
#define VOICE_PLAYBACK_STREAMS 16
#define VOICE_PLAYBACK_FRAMES_PER_STREAM 3
#define VOICE_PLAYER_LIMIT 128
#define VOICE_SAMPLE_GAIN 0.45f
#define VOICE_CAPTURE_RETRY_MS 1000

struct playback_frame {
    int16_t samples[NATIVE_VOICE_SAMPLES];
    float left, right;
    unsigned int position;
    Uint64 queued_at;
};

struct playback_stream {
    int used, speaker_slot, head, count;
    struct playback_frame frames[VOICE_PLAYBACK_FRAMES_PER_STREAM];
};

static pthread_mutex_t voice_lock = PTHREAD_MUTEX_INITIALIZER;
static SDL_AudioStream *capture_stream;
static struct playback_stream playback[VOICE_PLAYBACK_STREAMS];
/* ptt_active: the microphone is being read, by the key or by open mic. */
static int voice_enabled, voice_open, voice_menus, voice_lobby_mix, ptt_active, session_map_initialized;
static int session_connected_previous, local_allowed_previous;
static unsigned long session_map_token;
static int16_t capture_pending[NATIVE_VOICE_SAMPLES];
static unsigned int capture_pending_count;
static Uint64 capture_retry_time;
static uint32_t capture_sequence;
static unsigned char receive_sender[6], receive_speaker[6];
static unsigned char session_host[6];
static int session_host_initialized;
static unsigned char voice_wire[NATIVE_VOICE_FRAME_BYTES];
static int receive_slot, receive_relayed;
static unsigned long trace_capture, trace_sent, trace_received, trace_spatial, trace_mapping_drops;
static _Atomic unsigned long trace_mixed;
static Uint64 trace_time;
static int trace_enabled = -1;

/* Engine transport addresses are host-order integers. P2P/socket addresses
 * contain network-order bytes. Keep this ABI boundary explicit in one place. */
static unsigned long voice_game_address(unsigned long socket_address)
{
    uint32_t value = (uint32_t)socket_address;
    unsigned char bytes[4]; memcpy(bytes, &value, sizeof(bytes));
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
        ((uint32_t)bytes[2] << 8) | bytes[3];
}
static unsigned long voice_socket_address(unsigned long game_address)
{
    unsigned char bytes[4] = { (unsigned char)(game_address >> 24),
        (unsigned char)(game_address >> 16), (unsigned char)(game_address >> 8), (unsigned char)game_address };
    uint32_t value; memcpy(&value, bytes, sizeof(value)); return value;
}

static void playback_clear(void)
{
    pthread_mutex_lock(&voice_lock);
    memset(playback, 0, sizeof(playback));
    pthread_mutex_unlock(&voice_lock);
}

static void playback_push(const struct native_voice_frame *frame, int speaker_slot)
{
    float listener[3], right[3], speaker[3], left_gain, right_gain;
    float left, output_right;
    struct playback_stream *stream = NULL;
    int i, tail;
    if (speaker_slot < 0 || speaker_slot >= VOICE_PLAYER_LIMIT) { trace_mapping_drops++; return; }
    if (voice_lobby_mix) {
        /* A joined lobby has an authenticated roster, but no world positions. */
        left_gain = right_gain = 0.70710678f;
    } else {
        if (!network_game_port_voice_spatial(speaker_slot, listener, right, speaker)) { trace_mapping_drops++; return; }
        trace_spatial++;
        native_voice_spatial_gains(listener, right, speaker, 1.0f, 30.0f, &left_gain, &right_gain);
    }
    left = left_gain * VOICE_SAMPLE_GAIN;
    output_right = right_gain * VOICE_SAMPLE_GAIN;
    pthread_mutex_lock(&voice_lock);
    for (i = 0; i < VOICE_PLAYBACK_STREAMS; i++) {
        if (playback[i].used && playback[i].speaker_slot == speaker_slot) { stream = &playback[i]; break; }
        if (!stream && !playback[i].used) stream = &playback[i];
    }
    if (!stream) { pthread_mutex_unlock(&voice_lock); return; }
    stream->used = 1;
    stream->speaker_slot = speaker_slot;
    if (stream->count == VOICE_PLAYBACK_FRAMES_PER_STREAM) {
        memset(&stream->frames[stream->head], 0, sizeof(stream->frames[stream->head]));
        stream->head = (stream->head + 1) % VOICE_PLAYBACK_FRAMES_PER_STREAM;
        stream->count--;
    }
    tail = (stream->head + stream->count) % VOICE_PLAYBACK_FRAMES_PER_STREAM;
    memcpy(stream->frames[tail].samples, frame->samples, sizeof(frame->samples));
    stream->frames[tail].left = left;
    stream->frames[tail].right = output_right;
    stream->frames[tail].position = 0;
    stream->frames[tail].queued_at = SDL_GetTicks();
    stream->count++;
    pthread_mutex_unlock(&voice_lock);
}

static int capture_open(void)
{
    SDL_AudioSpec spec;
    if (capture_stream) return 1;
    if (!config_boolean("audio.voice_enabled")) return 0;
    /* Open mic asks on every update: do not hammer a missing or refused device. */
    if (SDL_GetTicks() < capture_retry_time) return 0;
    spec.format = SDL_AUDIO_S16;
    spec.channels = 1;
    spec.freq = NATIVE_VOICE_RATE;
    capture_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_RECORDING, &spec, NULL, NULL);
    if (!capture_stream) {
        capture_retry_time = SDL_GetTicks() + VOICE_CAPTURE_RETRY_MS;
        return 0;
    }
    SDL_ResumeAudioStreamDevice(capture_stream);
    return 1;
}

static void capture_pause(void)
{
    if (capture_stream) {
        SDL_PauseAudioStreamDevice(capture_stream);
        SDL_ClearAudioStream(capture_stream);
    }
    capture_pending_count = 0;
    memset(capture_pending, 0, sizeof(capture_pending));
}

static int make_wire(const int16_t *samples, unsigned char *wire)
{
    struct native_voice_frame frame;
    memcpy(frame.samples, samples, sizeof(frame.samples));
    frame.sequence = capture_sequence++;
    frame.controller = 0;
    return (int)native_voice_encode(wire, NATIVE_VOICE_FRAME_BYTES, &frame);
}

static void relay_local_host_frame(const unsigned char *wire, int size,
    const unsigned long *addresses, const int *slots, int count, int local_slot)
{
    int i;
    const unsigned char *speaker = p2p_identifier();
    for (i = 0; i < count; i++) {
        unsigned char destination[6];
        if (!addresses[i] || slots[i] == local_slot ||
            !p2p_voice_peer_for_address(voice_socket_address(addresses[i]), destination))
            continue;
        trace_sent += p2p_voice_send_relay(destination, speaker, local_slot, wire, size);
    }
}

static void process_capture_frame(const int16_t *samples)
{
    unsigned char wire[NATIVE_VOICE_FRAME_BYTES];
    unsigned long addresses[VOICE_PLAYER_LIMIT];
    int slots[VOICE_PLAYER_LIMIT], count, local_slot = -1, i;
    int size = make_wire(samples, wire);
    trace_capture++;
    if (!size) return;
    count = network_game_server_port_voice_roster(addresses, slots, VOICE_PLAYER_LIMIT);
    if (count > 0) {
        for (i = 0; i < count; i++) if (!addresses[i]) { local_slot = slots[i]; break; }
        if (local_slot >= 0) {
            relay_local_host_frame(wire, size, addresses, slots, count, local_slot);
            /* Do not send the host microphone back to its own speakers. */
        }
    } else {
        trace_sent += p2p_voice_send_host(wire, size);
    }
}

static void capture_drain(void)
{
    int available;
    int16_t buffer[VOICE_CAPTURE_CHUNK];
    if (!capture_stream) return;
    available = SDL_GetAudioStreamAvailable(capture_stream);
    while (available >= (int)sizeof(int16_t)) {
        int bytes = available;
        int samples, i;
        if (bytes > (int)sizeof(buffer)) bytes = sizeof(buffer);
        bytes -= bytes % (int)sizeof(int16_t);
        int received = SDL_GetAudioStreamData(capture_stream, buffer, bytes);
        if (received <= 0) break;
        samples = received / (int)sizeof(int16_t);
        if (!samples) break;
        for (i = 0; i < samples; i++) {
            capture_pending[capture_pending_count++] = buffer[i];
            if (capture_pending_count == NATIVE_VOICE_SAMPLES) {
                process_capture_frame(capture_pending);
                capture_pending_count = 0;
            }
        }
        available = SDL_GetAudioStreamAvailable(capture_stream);
    }
}

static void receive_drain(int allow_playback)
{
    unsigned long addresses[VOICE_PLAYER_LIMIT];
    int slots[VOICE_PLAYER_LIMIT], roster_count, limit = 64;
    roster_count = network_game_server_port_voice_roster(addresses, slots, VOICE_PLAYER_LIMIT);
    while (limit-- > 0 && p2p_voice_poll_ex(receive_sender, receive_speaker, &receive_slot,
        &receive_relayed, voice_wire, sizeof(voice_wire)) == NATIVE_VOICE_FRAME_BYTES) {
        struct native_voice_frame frame;
        trace_received++;
        if (!native_voice_decode(&frame, voice_wire, sizeof(voice_wire))) continue;
        if (roster_count > 0) {
            unsigned long sender_address, endpoint_address;
            unsigned char direct_identity[6];
            int verified_slot, i;
            if (receive_relayed || !p2p_voice_peer_addresses(receive_sender, &sender_address, &endpoint_address)) { trace_mapping_drops++; continue; }
            sender_address = voice_game_address(sender_address);
            if (!network_game_server_port_voice_sender_slot(sender_address, frame.controller, &verified_slot)) {
                if (!endpoint_address || !p2p_voice_peer_for_address(endpoint_address, direct_identity) ||
                    memcmp(direct_identity, receive_sender, sizeof(direct_identity)) ||
                    !network_game_server_port_voice_sender_slot(voice_game_address(endpoint_address), frame.controller, &verified_slot)) { trace_mapping_drops++; continue; }
                sender_address = voice_game_address(endpoint_address);
            }
            for (i = 0; i < roster_count; i++) {
                unsigned char destination[6];
                if (!addresses[i] || addresses[i] == sender_address ||
                    !p2p_voice_peer_for_address(voice_socket_address(addresses[i]), destination)) continue;
                trace_sent += p2p_voice_send_relay(destination, receive_sender, verified_slot, voice_wire, sizeof(voice_wire));
            }
            if (allow_playback) playback_push(&frame, verified_slot);
        } else {
            static const unsigned char zero_identity[6] = { 0 };
            if (!receive_relayed || receive_slot < 0 || receive_slot >= VOICE_PLAYER_LIMIT ||
                memcmp(receive_speaker, zero_identity, sizeof(zero_identity)) == 0) continue;
            if (allow_playback) playback_push(&frame, receive_slot);
        }
    }
}

/* Only an explicit "open" is open mic; anything else stays push-to-talk. */
static int voice_open_mode(void)
{
    return strcmp(config_string("audio.voice_mode"), "open") == 0;
}

void native_voice_runtime_update(int ptt_pressed, int menus_active)
{
    int enabled = config_boolean("audio.voice_enabled");
    int open_mode = enabled && voice_open_mode();
    int in_menus = config_boolean("audio.voice_in_menus");
    int connected = network_game_port_voice_session_connected();
    int gameplay = network_game_port_voice_session_active();
    int local_allowed = enabled && connected &&
        ((gameplay && !menus_active) || in_menus);
    int lobby_mix = connected && !gameplay;
    unsigned char current_host[6] = { 0 };
    int has_host = p2p_voice_session_host(current_host);
    unsigned long current_map = network_game_port_voice_map_token();
    int map_changed = session_map_initialized && current_map != session_map_token;
    if (trace_enabled < 0) {
        const char *trace = getenv("HALO_NETWORK_TEST_TRACE");
        trace_enabled = trace && *trace && *trace != '0';
    }
    if (trace_enabled && enabled && SDL_GetTicks() - trace_time >= 2000) {
        unsigned long addresses[VOICE_PLAYER_LIMIT]; int slots[VOICE_PLAYER_LIMIT];
        trace_time = SDL_GetTicks();
        platform_log("native voice: enabled=%d ptt=%d menus=%d host=%d roster=%d capture=%lu sent=%lu received=%lu spatial=%lu mapping_drop=%lu mixed=%lu open=%d lobby_voice=%d connected=%d allowed=%d",
            enabled, ptt_pressed, menus_active, has_host,
            network_game_server_port_voice_roster(addresses, slots, VOICE_PLAYER_LIMIT),
            trace_capture, trace_sent, trace_received, trace_spatial, trace_mapping_drops, atomic_load(&trace_mixed),
            open_mode, in_menus, connected, local_allowed);
    }
    if (!has_host) memset(current_host, 0, sizeof(current_host));
    if (voice_enabled != enabled || !session_host_initialized || map_changed ||
        session_connected_previous != connected ||
        memcmp(session_host, current_host, sizeof(session_host))) {
        voice_enabled = enabled;
        session_connected_previous = connected;
        session_host_initialized = 1;
        memcpy(session_host, current_host, sizeof(session_host));
        session_map_initialized = 1;
        session_map_token = current_map;
        capture_pause();
        ptt_active = 0;
        p2p_voice_enable(enabled && connected);
        playback_clear();
    }
    if (voice_open != open_mode || voice_menus != in_menus ||
        local_allowed_previous != local_allowed || voice_lobby_mix != lobby_mix) {
        voice_open = open_mode;
        voice_menus = in_menus;
        voice_lobby_mix = lobby_mix;
        local_allowed_previous = local_allowed;
        capture_pause();
        playback_clear();
        ptt_active = 0;
    }
    if (!enabled) {
        if (capture_stream) {
            SDL_DestroyAudioStream(capture_stream);
            capture_stream = NULL;
        }
        ptt_active = 0;
        playback_clear();
        return;
    }
    if (local_allowed && (open_mode || ptt_pressed) && capture_open()) {
        if (!ptt_active) {
            SDL_ClearAudioStream(capture_stream);
            SDL_ResumeAudioStreamDevice(capture_stream);
            ptt_active = 1;
        }
        capture_drain();
    } else if (ptt_active) {
        capture_pause();
        ptt_active = 0;
    }
    /* A host's local menu preference must not interrupt consenting peers'
     * relay. Admission and identity checks remain in receive_drain. */
    if (connected) receive_drain(local_allowed);
}

void native_voice_runtime_mix(float *stereo, size_t frames)
{
    size_t out;
    unsigned long mixed_frames = 0;
    int stream_index;
    Uint64 now;
    if (!stereo || !frames || pthread_mutex_trylock(&voice_lock) != 0) return;
    now = SDL_GetTicks();
    for (out = 0; out < frames; out++) {
        for (stream_index = 0; stream_index < VOICE_PLAYBACK_STREAMS; stream_index++) {
            struct playback_stream *stream = &playback[stream_index];
            struct playback_frame *entry;
            unsigned int source;
            float sample;
            while (stream->count > 0) {
                entry = &stream->frames[stream->head];
                if (now - entry->queued_at <= 250) break;
                memset(entry, 0, sizeof(*entry));
                stream->head = (stream->head + 1) % VOICE_PLAYBACK_FRAMES_PER_STREAM;
                stream->count--;
            }
            if (stream->count == 0) {
                stream->used = 0;
                stream->speaker_slot = -1;
                stream->head = 0;
                continue;
            }
            entry = &stream->frames[stream->head];
            source = entry->position / 3;
            sample = (float)entry->samples[source] / 32768.0f;
            mixed_frames++;
            stereo[2 * out] += sample * entry->left;
            stereo[2 * out + 1] += sample * entry->right;
            entry->position++;
            if (entry->position >= NATIVE_VOICE_SAMPLES * 3) {
                memset(entry, 0, sizeof(*entry));
                stream->head = (stream->head + 1) % VOICE_PLAYBACK_FRAMES_PER_STREAM;
                stream->count--;
                if (stream->count == 0) {
                    stream->used = 0;
                    stream->speaker_slot = -1;
                    stream->head = 0;
                }
            }
        }
    }
    atomic_fetch_add(&trace_mixed, mixed_frames);
    pthread_mutex_unlock(&voice_lock);
}

void native_voice_runtime_reset(void)
{
    capture_pause();
    if (capture_stream) {
        SDL_DestroyAudioStream(capture_stream);
        capture_stream = NULL;
    }
    ptt_active = voice_enabled = voice_open = voice_menus = voice_lobby_mix = 0;
    session_host_initialized = session_map_initialized = session_connected_previous = local_allowed_previous = 0;
    capture_retry_time = 0;
    session_map_token = 0;
    memset(session_host, 0, sizeof(session_host));
    playback_clear();
    p2p_voice_enable(0);
}

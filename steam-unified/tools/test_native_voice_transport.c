/* Private in-process acceptance of the actual transport send/receive queue.
 * Network send is captured into memory. No audio device or live socket opens. */
#include "../port/linux/src/p2p.c"
#include "../port/linux/src/native_voice_runtime.c"
#include <assert.h>

static unsigned long test_clock = 1000;
static unsigned long test_map_token;
static int test_voice_session_active;
static int test_voice_session_connected = 1;
static int test_voice_in_menus;
static int test_spatial_available = 1;
static int test_voice_enabled;
static const char *test_voice_mode = "ptt";
static int test_capture_device, test_capture_samples;
static unsigned char captured[MAXIMUM_PACKET_SIZE];
static int captured_size;
void platform_log(const char *format, ...) { (void)format; }

int config_boolean(const char *name) { return !strcmp(name, "audio.voice_in_menus") ? test_voice_in_menus : test_voice_enabled; }
const char *config_string(const char *name) { (void)name; return test_voice_mode; }
int network_game_server_port_voice_roster(unsigned long *addresses, int *slots, int capacity)
{ (void)addresses; (void)slots; (void)capacity; return 0; }
int network_game_server_port_voice_sender_slot(unsigned long address, int controller, int *slot)
{ (void)address; (void)controller; (void)slot; return 0; }
int network_game_port_voice_spatial(int speaker_slot, float listener[3], float right[3], float speaker[3])
{ (void)speaker_slot; listener[0]=listener[1]=listener[2]=0; right[0]=1; right[1]=right[2]=0; speaker[0]=speaker[1]=speaker[2]=0; return test_spatial_available; }
unsigned long network_game_port_voice_map_token(void) { return test_map_token; }
int network_game_port_voice_session_active(void) { return test_voice_session_active; }
int network_game_port_voice_session_connected(void) { return test_voice_session_connected; }
SDL_AudioStream *SDL_OpenAudioDeviceStream(SDL_AudioDeviceID device, const SDL_AudioSpec *spec, SDL_AudioStreamCallback callback, void *userdata)
{
    /* A fake microphone, only while a test asks for one: no audio device opens. */
    static char fake_stream;
    (void)device; (void)spec; (void)callback; (void)userdata;
    return test_capture_device ? (SDL_AudioStream *)(void *)&fake_stream : NULL;
}
bool SDL_ResumeAudioStreamDevice(SDL_AudioStream *stream) { (void)stream; return false; }
bool SDL_PauseAudioStreamDevice(SDL_AudioStream *stream) { (void)stream; return true; }
bool SDL_ClearAudioStream(SDL_AudioStream *stream) { (void)stream; test_capture_samples = 0; return true; }
void SDL_DestroyAudioStream(SDL_AudioStream *stream) { (void)stream; }
int SDL_GetAudioStreamAvailable(SDL_AudioStream *stream) { (void)stream; return test_capture_samples * (int)sizeof(int16_t); }
int SDL_GetAudioStreamData(SDL_AudioStream *stream, void *buffer, int length)
{
    int count = length / (int)sizeof(int16_t), i;
    (void)stream;
    if (count > test_capture_samples) count = test_capture_samples;
    for (i = 0; i < count; i++) ((int16_t *)buffer)[i] = 4321;
    test_capture_samples -= count;
    return count * (int)sizeof(int16_t);
}
Uint64 SDL_GetTicks(void) { return test_clock; }
unsigned long __stdcall GetTickCount(void) { return test_clock; }
void posix_random_bytes(void *buffer, posix_ulong size) { memset(buffer, 0x42, (size_t)size); }
int posix_socket_sendto(int socket, const void *buffer, int length, int flags,
    const void *address, int address_length)
{
    (void)socket; (void)flags; (void)address; (void)address_length;
    assert(length <= (int)sizeof(captured));
    memcpy(captured, buffer, length);
    captured_size = length;
    return length;
}

static void receive(struct peer *peer, struct native_voice_frame *frame, unsigned int sequence)
{
    unsigned char wire[NATIVE_VOICE_FRAME_BYTES];
    frame->sequence = sequence;
    assert(native_voice_encode(wire, sizeof(wire), frame) == sizeof(wire));
    pthread_mutex_lock(&p2p_lock);
    voice_received(peer, wire, sizeof(wire));
    pthread_mutex_unlock(&p2p_lock);
}

int main(void)
{
    struct peer *peer = &p2p.peers[0];
    struct native_voice_frame frame = {0}, decoded;
    unsigned char wire[NATIVE_VOICE_FRAME_BYTES], out[NATIVE_VOICE_FRAME_BYTES];
    unsigned char sender[P2P_IDENTIFIER_SIZE], speaker[P2P_IDENTIFIER_SIZE], nonce[P2P_NONCE_SIZE], plain[MAXIMUM_INNER_SIZE];
    int speaker_slot = -1, relayed = 0;
    int i;
    memcpy(peer->identifier, "peer01", P2P_IDENTIFIER_SIZE);
    memcpy(identifier, "self01", P2P_IDENTIFIER_SIZE);
    peer->used = peer->connected = 1;
    memset(peer->send_key, 7, sizeof(peer->send_key));
    p2p.tunnel_socket = 123; /* Captured only, never passed to an OS socket. */
    { /* LAN identity must be a unique current authenticated endpoint, never a candidate. */
        unsigned long virtual_address, endpoint;
        unsigned char found[6];
        p2p.running = 1;
        peer->virtual_address = 123;
        peer->endpoint.address = 456;
        assert(p2p_voice_peer_addresses(peer->identifier, &virtual_address, &endpoint));
        assert(virtual_address == 123 && endpoint == 456);
        assert(p2p_voice_peer_for_address(456, found) && !memcmp(found, peer->identifier, 6));
        p2p.peers[1].used = p2p.peers[1].connected = 1;
        p2p.peers[1].endpoint.address = 456;
        assert(!p2p_voice_peer_for_address(456, found));
        assert(p2p_voice_peer_for_address(123, found));
        p2p.peers[1].used = p2p.peers[1].connected = 0;
        peer->endpoint.address = 0;
        peer->candidate_count = 1; peer->candidates[0].address = 456;
        assert(!p2p_voice_peer_for_address(456, found));
        peer->connected = 0;
        assert(!p2p_voice_peer_addresses(peer->identifier, &virtual_address, &endpoint));
        peer->connected = 1;
    }
    { /* Actual engine↔socket IPv4 ABI, not equality on two socket-order values. */
        unsigned char bytes[4] = {100, 101, 226, 7}; uint32_t socket;
        memcpy(&socket, bytes, 4);
        assert(voice_game_address(socket) == 0x6465e207UL);
        assert(voice_socket_address(0x6465e207UL) == socket);
    }
    frame.samples[0] = -2345;
    assert(native_voice_encode(wire, sizeof(wire), &frame) == sizeof(wire));
    assert(!p2p_voice_send(peer->identifier, wire, sizeof(wire)) && !captured_size);
    receive(peer, &frame, 1);
    assert(!p2p_voice_poll(sender, out, sizeof(out)));
    p2p_voice_enable(1);
    assert(!p2p_voice_send(peer->identifier, wire, sizeof(wire)-1));
    assert(!p2p_voice_send((const unsigned char *)"absent", wire, sizeof(wire)));
    assert(p2p_voice_send(peer->identifier, wire, sizeof(wire)));
    assert(captured[0] == TUNNEL_MAGIC && !memcmp(captured + 1, identifier, P2P_IDENTIFIER_SIZE));
    packet_nonce(captured, nonce);
    assert(p2p_aead_open(peer->send_key, nonce, captured, TUNNEL_HEADER_SIZE,
        captured + TUNNEL_HEADER_SIZE, captured_size - TUNNEL_HEADER_SIZE, plain) == 1 + NATIVE_VOICE_FRAME_BYTES);
    assert(plain[0] == _packet_voice && !memcmp(plain + 1, wire, sizeof(wire)));
    captured[captured_size-1] ^= 1;
    assert(p2p_aead_open(peer->send_key, nonce, captured, TUNNEL_HEADER_SIZE,
        captured + TUNNEL_HEADER_SIZE, captured_size - TUNNEL_HEADER_SIZE, plain) < 0);
    receive(peer, &frame, 10);
    assert(p2p_voice_poll(sender, out, sizeof(out)) == sizeof(out));
    assert(!memcmp(sender, peer->identifier, P2P_IDENTIFIER_SIZE));
    assert(native_voice_decode(&decoded, out, sizeof(out)) && decoded.sequence == 10 && decoded.samples[0] == -2345);
    test_clock += 20; receive(peer, &frame, 10); /* Duplicate sequence. */
    assert(!p2p_voice_poll(sender, out, sizeof(out)));
    receive(peer, &frame, 9); /* Old sequence. */
    assert(!p2p_voice_poll(sender, out, sizeof(out)));
    test_clock += 100; /* New 100 ms burst window. */
    receive(peer, &frame, 11); receive(peer, &frame, 12); receive(peer, &frame, 13);
    receive(peer, &frame, 14); /* Three-frame burst is accepted; the fourth is bounded. */
    assert(native_voice_queue.count == 3);
    for (i=11; i<=13; i++) {
        assert(p2p_voice_poll(sender, out, sizeof(out)) == sizeof(out));
        assert(native_voice_decode(&decoded, out, sizeof(out)) && decoded.sequence == (unsigned)i);
    }
    test_clock += 100; receive(peer, &frame, 15);
    frame.controller = 1; receive(peer, &frame, 1);
    assert(p2p_voice_poll(sender, out, sizeof(out)) == sizeof(out));
    assert(native_voice_decode(&decoded, out, sizeof(out)) && decoded.controller == 0 && decoded.sequence == 15);
    assert(p2p_voice_poll(sender, out, sizeof(out)) == sizeof(out));
    assert(native_voice_decode(&decoded, out, sizeof(out)) && decoded.controller == 1 && decoded.sequence == 1);
    frame.controller = 0; test_clock += 251;
    assert(!p2p_voice_poll(sender, out, sizeof(out))); /* Expired frame. */
    test_clock += 20; receive(peer, &frame, 14); peer->connected = 0;
    assert(!p2p_voice_poll(sender, out, sizeof(out)));
    assert(!p2p_voice_send(peer->identifier, wire, sizeof(wire)));
    receive(peer, &frame, 15); assert(!native_voice_queue.count);
    peer->connected = 1;
    for (i=20; i<90; i++) { test_clock += 40; receive(peer, &frame, i); }
    assert(native_voice_queue.count == MAXIMUM_VOICE_QUEUE);
    assert(p2p_voice_poll(sender, out, sizeof(out)) == sizeof(out));
    assert(native_voice_decode(&decoded, out, sizeof(out)) && decoded.sequence == 83); /* Old queued frames expire; first remaining is within 250 ms. */
    p2p_voice_enable(1);
    peer->voice_sequence_seen[1] = 0;
    frame.controller = 1;
    test_clock += 20; receive(peer, &frame, UINT32_MAX);
    assert(!p2p_voice_poll(sender, out, sizeof(out)-1));
    assert(p2p_voice_poll(sender, out, sizeof(out)) == sizeof(out));
    test_clock += 20; receive(peer, &frame, 0); /* Sequence wrap is valid. */
    assert(p2p_voice_poll(sender, out, sizeof(out)) == sizeof(out));
    assert(native_voice_decode(&decoded, out, sizeof(out)) && decoded.sequence == 0 && decoded.controller == 1);
    wire[7] = 1;
    pthread_mutex_lock(&p2p_lock); voice_received(peer, wire, sizeof(wire)); pthread_mutex_unlock(&p2p_lock);
    assert(!native_voice_queue.count);
    wire[7] = 0;
    p2p.tunnel_socket = -1;
    assert(!p2p_voice_send(peer->identifier, wire, sizeof(wire)));
    p2p.tunnel_socket = 123;
    p2p_voice_enable(0);
    assert(!native_voice_queue.count && !p2p_voice_poll(sender, out, sizeof(out)));
    assert(!p2p_voice_send(peer->identifier, wire, sizeof(wire)));

    /* A joining client sends only to a peer authenticated as its host. */
    p2p_voice_enable(1);
    p2p.hosting = 0;
    peer->is_host = 1;
    assert(p2p_voice_send_host(wire, sizeof(wire)));
    packet_nonce(captured, nonce);
    assert(p2p_aead_open(peer->send_key, nonce, captured, TUNNEL_HEADER_SIZE,
        captured + TUNNEL_HEADER_SIZE, captured_size - TUNNEL_HEADER_SIZE, plain) == 1 + NATIVE_VOICE_FRAME_BYTES);
    assert(plain[0] == _packet_voice);
    peer->is_host = 0;
    assert(!p2p_voice_send_host(wire, sizeof(wire)));

    /* The relay is host-only, carries a host-verified player slot, and its
     * receiver identity is the authenticated host rather than the speaker. */
    p2p.hosting = 1;
    memcpy(speaker, "source", P2P_IDENTIFIER_SIZE);
    assert(p2p_voice_send_relay(peer->identifier, speaker, 2, wire, sizeof(wire)));
    packet_nonce(captured, nonce);
    assert(p2p_aead_open(peer->send_key, nonce, captured, TUNNEL_HEADER_SIZE,
        captured + TUNNEL_HEADER_SIZE, captured_size - TUNNEL_HEADER_SIZE, plain) ==
        2 + P2P_IDENTIFIER_SIZE + NATIVE_VOICE_FRAME_BYTES);
    assert(plain[0] == _packet_voice_relay && plain[1] == 2 &&
        !memcmp(plain + 2, speaker, P2P_IDENTIFIER_SIZE));
    p2p_voice_enable(1);
    peer->is_host = 0;
    {
        unsigned char payload[1 + P2P_IDENTIFIER_SIZE + NATIVE_VOICE_FRAME_BYTES];
        payload[0] = 2;
        memcpy(payload + 1, speaker, P2P_IDENTIFIER_SIZE);
        memcpy(payload + 1 + P2P_IDENTIFIER_SIZE, wire, NATIVE_VOICE_FRAME_BYTES);
        voice_relay_received(peer, payload, sizeof(payload));
        assert(!p2p_voice_poll_ex(sender, speaker, &speaker_slot, &relayed, out, sizeof(out)));
        peer->is_host = 1;
        memset(peer->voice_raw_pacing, 0, sizeof(peer->voice_raw_pacing)); memset(peer->voice_relay_pacing, 0, sizeof(peer->voice_relay_pacing));
        voice_relay_received(peer, payload, sizeof(payload));
        assert(p2p_voice_poll_ex(sender, speaker, &speaker_slot, &relayed, out, sizeof(out)) == sizeof(out));
        assert(!memcmp(sender, peer->identifier, P2P_IDENTIFIER_SIZE));
        assert(!memcmp(speaker, "source", P2P_IDENTIFIER_SIZE) && speaker_slot == 2 && relayed);
        payload[0] = 2;
        voice_relay_received(peer, payload, sizeof(payload));
        assert(p2p_voice_poll_ex(sender, speaker, &speaker_slot, &relayed, out, sizeof(out)) == sizeof(out));
        assert(speaker_slot == 2 && relayed); /* same-stream second frame in one game-tick drain */
        voice_relay_received(peer, payload, sizeof(payload));
        assert(p2p_voice_poll_ex(sender, speaker, &speaker_slot, &relayed, out, sizeof(out)) == sizeof(out));
        assert(speaker_slot == 2 && relayed);
        voice_relay_received(peer, payload, sizeof(payload));
        assert(!p2p_voice_poll_ex(sender, speaker, &speaker_slot, &relayed, out, sizeof(out)));
        payload[0] = 3;
        voice_relay_received(peer, payload, sizeof(payload));
        assert(p2p_voice_poll_ex(sender, speaker, &speaker_slot, &relayed, out, sizeof(out)) == sizeof(out));
        assert(speaker_slot == 3 && relayed); /* distinct stream is independently paced */
    }
    /* A host tunnel has a 256-frame/s aggregate budget across speakers. */
    p2p_voice_enable(1);
    peer->is_host = 1;
    {
        unsigned char payload[1 + P2P_IDENTIFIER_SIZE + NATIVE_VOICE_FRAME_BYTES];
        int slot, pass;
        frame.controller = 0;
        frame.sequence = 300;
        assert(native_voice_encode(wire, sizeof(wire), &frame) == sizeof(wire));
        memset(payload + 1, 0x53, P2P_IDENTIFIER_SIZE);
        memcpy(payload + 1 + P2P_IDENTIFIER_SIZE, wire, sizeof(wire));
        for (pass = 0; pass < 2; pass++) {
            for (slot = 0; slot < 128; slot++) {
                payload[0] = (unsigned char)slot;
                voice_relay_received(peer, payload, sizeof(payload));
            }
        }
        assert(peer->voice_budget_count == VOICE_PEER_BUDGET_PER_SECOND);
        assert(native_voice_queue.count == MAXIMUM_VOICE_QUEUE);
        payload[0] = 0;
        voice_relay_received(peer, payload, sizeof(payload));
        assert(peer->voice_budget_count == VOICE_PEER_BUDGET_PER_SECOND);
        assert(native_voice_queue.count == MAXIMUM_VOICE_QUEUE);
    }

    /* Generated PCM traverses client-to-host encryption, authenticated host
     * verification, host relay, client host-only receive, and the real mixer. */
    {
        unsigned char host_sender[6], host_speaker[6], relay_payload[1 + P2P_IDENTIFIER_SIZE + NATIVE_VOICE_FRAME_BYTES];
        unsigned char received_sender[6], received_speaker[6], mixed_wire[NATIVE_VOICE_FRAME_BYTES];
        float mixed[2 * NATIVE_VOICE_SAMPLES * 3] = { 0 };
        int host_slot = -1, client_slot = -1, is_relay = 0;
        struct native_voice_frame injected = {0}, got;
        int sample, positive = 0;
        injected.sequence = 201;
        injected.controller = 0;
        for (sample = 0; sample < NATIVE_VOICE_SAMPLES; sample++) injected.samples[sample] = 12000;
        assert(native_voice_encode(wire, sizeof(wire), &injected) == sizeof(wire));
        p2p_voice_enable(1);
        p2p.hosting = 0; peer->is_host = 1;
        assert(p2p_voice_send_host(wire, sizeof(wire)));
        packet_nonce(captured, nonce);
        assert(p2p_aead_open(peer->send_key, nonce, captured, TUNNEL_HEADER_SIZE,
            captured + TUNNEL_HEADER_SIZE, captured_size - TUNNEL_HEADER_SIZE, plain) == 1 + NATIVE_VOICE_FRAME_BYTES);
        assert(plain[0] == _packet_voice);

        p2p.hosting = 1; peer->is_host = 0; memset(peer->voice_raw_pacing, 0, sizeof(peer->voice_raw_pacing)); memset(peer->voice_relay_pacing, 0, sizeof(peer->voice_relay_pacing));
        peer->voice_sequence_seen[0] = 0;
        voice_received(peer, wire, sizeof(wire));
        assert(p2p_voice_poll_ex(host_sender, host_speaker, &host_slot, &is_relay, mixed_wire,
            sizeof(mixed_wire)) == NATIVE_VOICE_FRAME_BYTES && !is_relay);
        assert(!memcmp(host_sender, peer->identifier, P2P_IDENTIFIER_SIZE));
        assert(p2p_voice_send_relay(peer->identifier, host_sender, 2, mixed_wire, sizeof(mixed_wire)));
        packet_nonce(captured, nonce);
        assert(p2p_aead_open(peer->send_key, nonce, captured, TUNNEL_HEADER_SIZE,
            captured + TUNNEL_HEADER_SIZE, captured_size - TUNNEL_HEADER_SIZE, plain) ==
            2 + P2P_IDENTIFIER_SIZE + NATIVE_VOICE_FRAME_BYTES);
        assert(plain[0] == _packet_voice_relay && plain[1] == 2);
        memcpy(relay_payload, plain + 1, sizeof(relay_payload));

        p2p.hosting = 0; peer->is_host = 1; memset(peer->voice_raw_pacing, 0, sizeof(peer->voice_raw_pacing)); memset(peer->voice_relay_pacing, 0, sizeof(peer->voice_relay_pacing));
        p2p_voice_enable(1);
        voice_relay_received(peer, relay_payload, sizeof(relay_payload));
        assert(p2p_voice_poll_ex(received_sender, received_speaker, &client_slot, &is_relay,
            mixed_wire, sizeof(mixed_wire)) == NATIVE_VOICE_FRAME_BYTES);
        assert(is_relay && client_slot == 2 && !memcmp(received_sender, peer->identifier, P2P_IDENTIFIER_SIZE));
        assert(!memcmp(received_speaker, host_sender, P2P_IDENTIFIER_SIZE));
        assert(native_voice_decode(&got, mixed_wire, sizeof(mixed_wire)) && got.sequence == 201);
        playback_clear();
        playback_push(&got, client_slot);
        got.samples[0] = 6000;
        for (sample = 1; sample < NATIVE_VOICE_SAMPLES; sample++) got.samples[sample] = 6000;
        playback_push(&got, client_slot + 1);
        native_voice_runtime_mix(mixed, NATIVE_VOICE_SAMPLES * 3);
        for (sample = 0; sample < 2 * NATIVE_VOICE_SAMPLES * 3; sample++) if (mixed[sample] > 0.17f) positive++;
        assert(positive == 2 * NATIVE_VOICE_SAMPLES * 3);
        memset(mixed, 0, sizeof(mixed));
        playback_push(&got, client_slot);
        test_clock += 251;
        native_voice_runtime_mix(mixed, NATIVE_VOICE_SAMPLES * 3);
        for (sample = 0; sample < 2 * NATIVE_VOICE_SAMPLES * 3; sample++) assert(mixed[sample] == 0.0f);
        test_voice_enabled = 1; test_map_token = 10;
        native_voice_runtime_update(0, 0);
        playback_push(&got, client_slot);
        test_map_token = 11; /* Same authenticated host, new map generation. */
        native_voice_runtime_update(0, 0);
        native_voice_runtime_mix(mixed, NATIVE_VOICE_SAMPLES * 3);
        for (sample = 0; sample < 2 * NATIVE_VOICE_SAMPLES * 3; sample++) assert(mixed[sample] == 0.0f);
        test_voice_enabled = 0;
        native_voice_runtime_update(0, 0);
    }
    { /* Open mic: the fake microphone's PCM reaches the host without the key
       * only when voice is enabled, the mode is "open" and play is live. */
        const int half = NATIVE_VOICE_SAMPLES / 2;
        test_capture_device = 1;
        test_voice_enabled = 1;
        test_voice_session_active = 0;
        peer->is_host = 1; /* A connected P2P host alone is still pregame. */
        assert(p2p_voice_session_host(sender));
        native_voice_runtime_update(0, 0);
        test_capture_samples = NATIVE_VOICE_SAMPLES; captured_size = 0;
        native_voice_runtime_update(0, 0);
        assert(!captured_size && test_capture_samples == NATIVE_VOICE_SAMPLES); /* Push-to-talk, key up. */
        test_voice_mode = "OPEN MIC"; /* An unknown mode is push-to-talk. */
        native_voice_runtime_update(0, 0);
        assert(!captured_size && test_capture_samples == NATIVE_VOICE_SAMPLES);
        test_voice_mode = "open";
        test_capture_samples = NATIVE_VOICE_SAMPLES;
        native_voice_runtime_update(0, 0);
        assert(!captured_size && test_capture_samples == NATIVE_VOICE_SAMPLES); /* P2P host exists, but no in-game local unit: don't read. */
        native_voice_runtime_update(1, 0);
        assert(!captured_size && test_capture_samples == NATIVE_VOICE_SAMPLES); /* Open mode cannot bypass the live-game gate with its PTT key. */
        test_voice_session_active = 1;
        native_voice_runtime_update(0, 0);
        assert(!captured_size && !test_capture_samples); /* Entering open mic discards queued pregame samples. */
        test_capture_samples = NATIVE_VOICE_SAMPLES;
        native_voice_runtime_update(0, 0);
        assert(captured_size > 0 && !capture_pending_count); /* Live play: one frame sent, no key. */
        captured_size = 0; test_capture_samples = half;
        native_voice_runtime_update(0, 0);
        assert(!captured_size && capture_pending_count == (unsigned int)half);
        test_voice_session_active = 0; /* Game stops while a partial frame is buffered. */
        native_voice_runtime_update(1, 0);
        assert(!captured_size && !capture_pending_count && test_capture_samples == 0);
        test_voice_session_active = 1;
        native_voice_runtime_update(0, 1); /* A menu opens over a partial frame. */
        assert(!capture_pending_count);
        test_capture_samples = NATIVE_VOICE_SAMPLES;
        native_voice_runtime_update(0, 1);
        assert(!captured_size && test_capture_samples == NATIVE_VOICE_SAMPLES); /* Menus: nothing read or sent. */
        native_voice_runtime_update(0, 0);
        assert(!captured_size && !test_capture_samples); /* Menu-time audio never reaches play. */
        test_capture_samples = half;
        native_voice_runtime_update(0, 0);
        assert(capture_pending_count == (unsigned int)half);
        test_voice_mode = "ptt"; /* Changing mode live clears queued capture. */
        native_voice_runtime_update(0, 0);
        assert(!captured_size && !capture_pending_count);
        native_voice_runtime_update(1, 0); /* The separate push-to-talk key still works. */
        test_capture_samples = NATIVE_VOICE_SAMPLES;
        native_voice_runtime_update(1, 0);
        assert(captured_size > 0);
        test_voice_mode = "open";
        native_voice_runtime_update(0, 0);
        captured_size = 0; test_capture_samples = half;
        native_voice_runtime_update(0, 0);
        assert(capture_pending_count == (unsigned int)half);
        test_map_token = 12; /* A new map never sends the old map's partial frame. */
        native_voice_runtime_update(0, 0);
        assert(!captured_size && !capture_pending_count);
        test_voice_enabled = 0; test_capture_samples = NATIVE_VOICE_SAMPLES;
        native_voice_runtime_update(0, 0);
        assert(!captured_size && !capture_stream); /* "open" alone, without voice enabled, opens nothing. */
        test_voice_mode = "ptt"; test_capture_device = 0; test_capture_samples = 0;
    }
    { /* Lobby/menu voice is a separate opt-in, gated by real session admission. */
        float mixed[NATIVE_VOICE_SAMPLES * 6] = {0};
        struct native_voice_frame tone = {0};
        test_capture_device = test_voice_enabled = 1;
        test_voice_mode = "open";
        test_voice_session_active = 0;
        test_voice_session_connected = 1;
        test_voice_in_menus = 0;
        captured_size = 0;
        native_voice_runtime_update(0, 1);
        test_capture_samples = NATIVE_VOICE_SAMPLES;
        native_voice_runtime_update(0, 1);
        assert(!captured_size && !ptt_active);
        test_voice_in_menus = 1;
        native_voice_runtime_update(0, 1);
        assert(!captured_size && !test_capture_samples && ptt_active);
        test_capture_samples = NATIVE_VOICE_SAMPLES;
        native_voice_runtime_update(0, 1);
        assert(captured_size > 0); /* No push-to-talk press. */
        test_spatial_available = 0;
        for (i=0; i<NATIVE_VOICE_SAMPLES; i++) tone.samples[i]=8000;
        playback_push(&tone, 1);
        native_voice_runtime_mix(mixed, NATIVE_VOICE_SAMPLES * 3);
        for (i=0; i<NATIVE_VOICE_SAMPLES * 3; i++)
            assert(mixed[2*i] > 0.07f && mixed[2*i] == mixed[2*i+1]);
        captured_size = 0; test_capture_samples = NATIVE_VOICE_SAMPLES / 2;
        native_voice_runtime_update(0, 1);
        assert(capture_pending_count);
        test_voice_session_connected = 0;
        native_voice_runtime_update(1, 1);
        assert(!captured_size && !capture_pending_count && !ptt_active);
        test_capture_samples = NATIVE_VOICE_SAMPLES;
        native_voice_runtime_update(1, 1);
        assert(!captured_size); /* Setting and key cannot open unjoined main menu. */
        test_voice_session_connected = 1; test_voice_mode = "ptt";
        native_voice_runtime_update(0, 1);
        test_capture_samples = NATIVE_VOICE_SAMPLES;
        native_voice_runtime_update(0, 1);
        assert(!captured_size && !ptt_active);
        native_voice_runtime_update(1, 1);
        test_capture_samples = NATIVE_VOICE_SAMPLES;
        native_voice_runtime_update(1, 1);
        assert(captured_size > 0);
        test_voice_in_menus = 0; captured_size = 0;
        native_voice_runtime_update(1, 1);
        assert(!ptt_active && !capture_pending_count);
        test_voice_in_menus = 1; test_voice_enabled = 0;
        native_voice_runtime_update(1, 1);
        assert(!capture_stream && !captured_size);
        test_voice_in_menus = 0; test_capture_device = 0;
        test_capture_samples = 0; test_spatial_available = 1;
    }
    { /* A 30 Hz game update drains a genuine sustained 50 Hz PCM stream. */
        unsigned long second = 0, refill = 0;
        unsigned int second_count = 0, spent = 0, accepted = 0, emitted = 0;
        unsigned char initialized = 0;
        unsigned int tick;
        for (tick = 0; tick < 30; tick++) {
            unsigned long now = 50000 + (tick * 1000) / 30;
            unsigned int due = (tick * 50) / 30 + 1;
            while (emitted < due) {
                accepted += voice_pacing_allow(now, &second, &second_count, &refill, &spent, &initialized);
                emitted++;
            }
        }
        assert(emitted == 49 && accepted == emitted);
        second = refill = 0; second_count = spent = 0; initialized = 0;
        for (tick = 0; tick < 50; tick++)
            assert(voice_pacing_allow(60000 + tick * 20, &second, &second_count, &refill, &spent, &initialized));
    }
    p2p_voice_enable(0);
    puts("PASS native voice transport and generated-PCM relay-to-mixer path; no audio device or external traffic");
    return 0;
}

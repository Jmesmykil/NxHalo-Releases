/* Private in-process acceptance of the actual transport send/receive queue.
 * Network send is captured into memory. No audio device or live socket opens. */
#include "../port/linux/src/p2p.c"
#include <assert.h>

static unsigned long test_clock = 1000;
static unsigned char captured[MAXIMUM_PACKET_SIZE];
static int captured_size;
unsigned long __stdcall GetTickCount(void) { return test_clock; }
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
    unsigned char sender[P2P_IDENTIFIER_SIZE], nonce[P2P_NONCE_SIZE], plain[MAXIMUM_INNER_SIZE];
    int i;
    memcpy(peer->identifier, "peer01", P2P_IDENTIFIER_SIZE);
    memcpy(identifier, "self01", P2P_IDENTIFIER_SIZE);
    peer->used = peer->connected = 1;
    memset(peer->send_key, 7, sizeof(peer->send_key));
    p2p.tunnel_socket = 123; /* Captured only, never passed to an OS socket. */
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
    receive(peer, &frame, 11); receive(peer, &frame, 12); /* Rate limit. */
    assert(native_voice_queue.count == 1);
    assert(p2p_voice_poll(sender, out, sizeof(out)) == sizeof(out));
    assert(native_voice_decode(&decoded, out, sizeof(out)) && decoded.sequence == 11);
    test_clock += 20; receive(peer, &frame, 13); test_clock += 251;
    assert(!p2p_voice_poll(sender, out, sizeof(out))); /* Expired frame. */
    test_clock += 20; receive(peer, &frame, 14); peer->connected = 0;
    assert(!p2p_voice_poll(sender, out, sizeof(out)));
    assert(!p2p_voice_send(peer->identifier, wire, sizeof(wire)));
    receive(peer, &frame, 15); assert(!native_voice_queue.count);
    peer->connected = 1;
    for (i=20; i<90; i++) { test_clock += 10; receive(peer, &frame, i); }
    assert(native_voice_queue.count == MAXIMUM_VOICE_QUEUE);
    assert(p2p_voice_poll(sender, out, sizeof(out)) == sizeof(out));
    assert(native_voice_decode(&decoded, out, sizeof(out)) && decoded.sequence == 64); /* Age 250ms is still current. */
    p2p_voice_enable(1);
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
    puts("PASS native voice: default off, authenticated encrypted send, tamper rejection, identity, sequence/rate checks, bounded queue, expiry, disconnected peers, disable purge; no audio or external traffic");
    return 0;
}

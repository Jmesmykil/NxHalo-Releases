#ifndef HALO_NATIVE_VOICE_WIRE_H
#define HALO_NATIVE_VOICE_WIRE_H

#include <stddef.h>
#include <stdint.h>

enum {
    NATIVE_VOICE_RATE = 16000,
    NATIVE_VOICE_SAMPLES = 320,
    NATIVE_VOICE_HEADER_BYTES = 16,
    NATIVE_VOICE_FRAME_BYTES = NATIVE_VOICE_HEADER_BYTES + NATIVE_VOICE_SAMPLES * 2
};

struct native_voice_frame {
    uint32_t sequence;
    unsigned char controller;
    int16_t samples[NATIVE_VOICE_SAMPLES];
};

/* Identity is supplied by the authenticated transport, never by this payload.
 * A receiver must map that identity/controller to a current session player. */
size_t native_voice_encode(unsigned char *wire, size_t capacity,
    const struct native_voice_frame *frame);
int native_voice_decode(struct native_voice_frame *frame,
    const unsigned char *wire, size_t size);
int native_voice_sequence_newer(uint32_t candidate, uint32_t previous);

/* Spatial stereo gain from current replicated world positions. Invalid vectors
 * or ranges are silent. No payload may choose its own position or loudness. */
void native_voice_spatial_gains(const float listener[3], const float right[3],
    const float speaker[3], float near_distance, float far_distance,
    float *left_gain, float *right_gain);
/* Mix at most one bounded frame; larger counts are rejected without writing. */
void native_voice_mix(float *stereo, size_t frames, const int16_t *mono,
    float left_gain, float right_gain);

#endif

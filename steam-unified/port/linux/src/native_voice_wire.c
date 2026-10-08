#include "native_voice_wire.h"

#include <math.h>
#include <string.h>

static unsigned int read16(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static uint32_t read32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void write16(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
}

size_t native_voice_encode(unsigned char *wire, size_t capacity,
    const struct native_voice_frame *frame)
{
    unsigned int i;
    if (!wire || !frame || capacity < NATIVE_VOICE_FRAME_BYTES || frame->controller > 3)
        return 0;
    memcpy(wire, "HVO1", 4);
    wire[4] = 1;
    wire[5] = frame->controller;
    wire[6] = 1; /* PCM16 mono, little endian. */
    wire[7] = 0;
    for (i = 0; i < 4; i++)
        wire[8 + i] = (unsigned char)(frame->sequence >> (8 * i));
    write16(wire + 12, NATIVE_VOICE_SAMPLES);
    write16(wire + 14, NATIVE_VOICE_RATE);
    for (i = 0; i < NATIVE_VOICE_SAMPLES; i++)
        write16(wire + NATIVE_VOICE_HEADER_BYTES + 2 * i, (uint16_t)frame->samples[i]);
    return NATIVE_VOICE_FRAME_BYTES;
}

int native_voice_decode(struct native_voice_frame *frame,
    const unsigned char *wire, size_t size)
{
    unsigned int i;
    if (!frame || !wire || size != NATIVE_VOICE_FRAME_BYTES ||
        memcmp(wire, "HVO1", 4) || wire[4] != 1 || wire[5] > 3 ||
        wire[6] != 1 || wire[7] || read16(wire + 12) != NATIVE_VOICE_SAMPLES ||
        read16(wire + 14) != NATIVE_VOICE_RATE)
        return 0;
    frame->sequence = read32(wire + 8);
    frame->controller = wire[5];
    for (i = 0; i < NATIVE_VOICE_SAMPLES; i++) {
        unsigned int value = read16(wire + NATIVE_VOICE_HEADER_BYTES + 2 * i);
        frame->samples[i] = (int16_t)(value < 32768u ? (int)value : (int)value - 65536);
    }
    return 1;
}

int native_voice_sequence_newer(uint32_t candidate, uint32_t previous)
{
    uint32_t difference = candidate - previous;
    return difference != 0 && difference < UINT32_C(0x80000000);
}

void native_voice_spatial_gains(const float listener[3], const float right[3],
    const float speaker[3], float near_distance, float far_distance,
    float *left_gain, float *right_gain)
{
    float delta[3], distance2 = 0, right2 = 0, dot = 0, distance, pan, gain;
    unsigned int i;
    if (!left_gain || !right_gain)
        return;
    *left_gain = *right_gain = 0;
    if (!listener || !right || !speaker || !isfinite(near_distance) ||
        !isfinite(far_distance) || near_distance < 0 || far_distance <= near_distance)
        return;
    for (i = 0; i < 3; i++) {
        if (!isfinite(listener[i]) || !isfinite(right[i]) || !isfinite(speaker[i]))
            return;
        delta[i] = speaker[i] - listener[i];
        distance2 += delta[i] * delta[i];
        right2 += right[i] * right[i];
        dot += delta[i] * right[i];
    }
    if (!isfinite(distance2) || !isfinite(right2) || !isfinite(dot) || right2 <= 0)
        return;
    distance = sqrtf(distance2);
    if (distance >= far_distance)
        return;
    gain = distance <= near_distance ? 1 :
        (far_distance - distance) / (far_distance - near_distance);
    pan = distance > 0.00001f ? dot / (distance * sqrtf(right2)) : 0;
    if (pan > 1) pan = 1;
    if (pan < -1) pan = -1;
    *left_gain = gain * sqrtf(0.5f * (1 - pan));
    *right_gain = gain * sqrtf(0.5f * (1 + pan));
}

void native_voice_mix(float *stereo, size_t frames, const int16_t *mono,
    float left_gain, float right_gain)
{
    size_t i;
    if (!stereo || !mono || frames > NATIVE_VOICE_SAMPLES ||
        !isfinite(left_gain) || !isfinite(right_gain))
        return;
    if (left_gain < 0) left_gain = 0;
    if (right_gain < 0) right_gain = 0;
    if (left_gain > 1) left_gain = 1;
    if (right_gain > 1) right_gain = 1;
    for (i = 0; i < frames; i++) {
        float sample = (float)mono[i] / 32768.0f;
        float left = (isfinite(stereo[2 * i]) ? stereo[2 * i] : 0) + sample * left_gain;
        float right = (isfinite(stereo[2 * i + 1]) ? stereo[2 * i + 1] : 0) + sample * right_gain;
        stereo[2 * i] = left > 1 ? 1 : left < -1 ? -1 : left;
        stereo[2 * i + 1] = right > 1 ? 1 : right < -1 ? -1 : right;
    }
}

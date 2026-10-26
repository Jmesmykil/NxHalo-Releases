#include "native_voice_wire.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    struct native_voice_frame a = {0}, b;
    unsigned char wire[NATIVE_VOICE_FRAME_BYTES + 1], copy[sizeof(wire)];
    unsigned int i;
    float listener[3] = {0,0,0}, right[3] = {1,0,0}, speaker[3] = {0,0,0};
    float left, gain_right, stereo[2] = {0.95f,-0.95f};
    int16_t sample = 32767;
    a.sequence = UINT32_C(0xfedcba98); a.controller = 3;
    for (i = 0; i < NATIVE_VOICE_SAMPLES; i++) a.samples[i] = (int16_t)((int)i * 199 - 32000);
    a.samples[0] = -32768; a.samples[1] = 32767;
    assert(native_voice_encode(wire, NATIVE_VOICE_FRAME_BYTES - 1, &a) == 0);
    assert(native_voice_encode(wire, sizeof(wire), &a) == NATIVE_VOICE_FRAME_BYTES);
    assert(native_voice_decode(&b, wire, NATIVE_VOICE_FRAME_BYTES));
    assert(b.sequence == a.sequence && b.controller == a.controller);
    assert(!memcmp(b.samples, a.samples, sizeof(a.samples)));
    for (i = 0; i < NATIVE_VOICE_FRAME_BYTES; i++) assert(!native_voice_decode(&b, wire, i));
    assert(!native_voice_decode(&b, wire, NATIVE_VOICE_FRAME_BYTES + 1));
    for (i = 0; i < 8; i++) {
        memcpy(copy, wire, sizeof(wire)); copy[i] ^= 0xff;
        assert(!native_voice_decode(&b, copy, NATIVE_VOICE_FRAME_BYTES));
    }
    for (i = 12; i < 16; i++) {
        memcpy(copy, wire, sizeof(wire)); copy[i] ^= 1;
        assert(!native_voice_decode(&b, copy, NATIVE_VOICE_FRAME_BYTES));
    }
    a.controller = 4; assert(!native_voice_encode(wire, sizeof(wire), &a));
    assert(native_voice_sequence_newer(0, UINT32_MAX));
    assert(!native_voice_sequence_newer(1, 1));
    assert(!native_voice_sequence_newer(1, 2));
    assert(!native_voice_sequence_newer(UINT32_C(0x80000000), 0));
    native_voice_spatial_gains(listener,right,speaker,2,20,&left,&gain_right);
    assert(fabsf(left - 0.70710678f) < 0.0001f && fabsf(left - gain_right) < 0.0001f);
    speaker[0] = 1; native_voice_spatial_gains(listener,right,speaker,2,20,&left,&gain_right);
    assert(left == 0 && gain_right == 1);
    speaker[0] = -1; native_voice_spatial_gains(listener,right,speaker,2,20,&left,&gain_right);
    assert(left == 1 && gain_right == 0);
    speaker[0] = 20; native_voice_spatial_gains(listener,right,speaker,2,20,&left,&gain_right);
    assert(left == 0 && gain_right == 0);
    speaker[0] = NAN; native_voice_spatial_gains(listener,right,speaker,2,20,&left,&gain_right);
    assert(left == 0 && gain_right == 0);
    native_voice_mix(stereo,1,&sample,1,1);
    assert(stereo[0] == 1 && stereo[1] < 0.05f && stereo[1] > 0);
    stereo[0] = 0.25f; stereo[1] = -0.25f;
    native_voice_mix(stereo,NATIVE_VOICE_SAMPLES + 1,&sample,1,1);
    assert(stereo[0] == 0.25f && stereo[1] == -0.25f);
    puts("PASS: PCM wire roundtrip, exact bounds, invalid metadata, wrap ordering, spatial gains and clipping");
    return 0;
}

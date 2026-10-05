#include "../host/host_audio_profile.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    struct host_audio_profile p = {0};
    /* 4096 bytes at 192000 bytes/sec has a 409600 tick deadline. */
    host_audio_profile_complete(&p, 409600, 4096, 4096, 192000);
    assert(p.callbacks == 1 && p.late_callbacks == 0 && p.partial_fills == 0);
    host_audio_profile_complete(&p, 409601, 4096, 2048, 192000);
    assert(p.callbacks == 2 && p.late_callbacks == 1 && p.partial_fills == 1);
    assert(p.requested_bytes == 8192 && p.supplied_bytes == 6144);
    assert(p.callback_ticks == 819201 && p.callback_max_ticks == 409601);
    host_audio_profile_complete(&p, 1, 4096, 0, 0);
    assert(p.partial_fills == 2 && p.late_callbacks == 1);
    assert(p.callback_max_ticks == 409601);
    puts("audio profile deadline, partial-fill, totals and unknown-rate checks passed");
    return 0;
}

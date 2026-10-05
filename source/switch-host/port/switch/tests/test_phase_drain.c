#include "../host/host_phase_profile.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    struct host_phase_state s = {0}; uint32_t guest = 0;
    assert(host_extra_phase_receive(&s, "HP2 300 1 10 10 299 0 0 0 0 0 0 0 0"));
    assert(host_phase_drain(&s, 600, &guest) && guest == 300);
    assert(s.consumed_frame == 300 && s.mismatched == 1 && s.pending_frame == 0);
    assert(!host_phase_drain(&s, 900, &guest));
    assert(!host_extra_phase_receive(&s, "HP2 300 1 10 10 299 0 0 0 0 0 0 0 0"));
    assert(s.rejected == 1);
    assert(host_extra_phase_receive(&s, "HP2 600 0 0 0 0 0 0 0 0 0 0 0 0"));
    assert(host_extra_phase_receive(&s, "HP2 900 0 0 0 0 0 0 0 0 0 0 0 0"));
    assert(s.overwritten == 1);
    assert(host_phase_drain(&s, 900, &guest) && guest == 900);
    assert(s.mismatched == 1);
    puts("phase drain: divergent counters preserved, duplicate rejected, overwrite reported");
}

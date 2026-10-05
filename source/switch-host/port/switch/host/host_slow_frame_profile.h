#ifndef HOST_SLOW_FRAME_PROFILE_H
#define HOST_SLOW_FRAME_PROFILE_H
#include "host_gpu_profile.h"
#include "host_io_profile.h"
#include "host_memory_profile.h"
#define HOST_SLOW_FRAME_GROUPS 3
#define HOST_SLOW_FRAME_THRESHOLD_NS UINT64_C(50000000)
#define HOST_SLOW_GPU_INCOMPLETE 1u
#define HOST_SLOW_FRAME_DISCONTINUOUS 2u
#define HOST_SLOW_FRAME_STARTUP 8u
struct host_slow_frame_record {
    uint32_t frame, flags, guest_frame;
    uint64_t frame_ns, swap_ns;
    struct gpu_profile gpu;
    struct host_io_profile io;
    struct host_memory_frame memory;
    uint32_t io_available;
};
struct host_slow_frame_group {
    unsigned valid, before_valid, after_valid;
    struct host_slow_frame_record before, spike, after;
};
struct host_slow_frame_profile {
    unsigned previous_valid;
    struct host_slow_frame_record previous;
    struct host_slow_frame_group groups[HOST_SLOW_FRAME_GROUPS];
};
/* Fixed memory. Retain the three worst >50ms presentation intervals and one
 * adjacent frame on each side. A last-window-frame spike has no future frame
 * yet; explicitly report missing after-context rather than fabricate one. */
static inline void host_slow_frame_push(struct host_slow_frame_profile *p,
    const struct host_slow_frame_record *record)
{
    struct host_slow_frame_record current = *record;
    int adjacent = p->previous_valid && record->frame > p->previous.frame &&
        record->frame - p->previous.frame == 1 &&
        !(p->previous.flags & HOST_SLOW_FRAME_STARTUP);
    if (p->previous_valid && !adjacent) current.flags |= HOST_SLOW_FRAME_DISCONTINUOUS;
    record = &current;
    for (unsigned i = 0; i < HOST_SLOW_FRAME_GROUPS; ++i) {
        struct host_slow_frame_group *g = &p->groups[i];
        if (g->valid && record->frame > g->spike.frame && record->frame - g->spike.frame == 1) {
            g->after = *record; g->after_valid = 1;
        }
    }
    if (!(record->flags & HOST_SLOW_FRAME_STARTUP) && record->frame_ns > HOST_SLOW_FRAME_THRESHOLD_NS) {
        unsigned slot = 0;
        for (unsigned i = 0; i < HOST_SLOW_FRAME_GROUPS; ++i) {
            if (!p->groups[i].valid) {slot = i; break;}
            if (p->groups[i].spike.frame_ns < p->groups[slot].spike.frame_ns) slot = i;
        }
        struct host_slow_frame_group *g = &p->groups[slot];
        if (!g->valid || record->frame_ns > g->spike.frame_ns) {
            *g = (struct host_slow_frame_group){ .valid = 1, .before_valid = (unsigned)adjacent, .spike = *record };
            if (adjacent) g->before = p->previous;
        }
    }
    p->previous = *record;
    p->previous_valid = 1;
}
static inline void host_slow_frame_reset(struct host_slow_frame_profile *p)
{ memset(p->groups, 0, sizeof(p->groups)); }
/* Diagnostic wire counters clamp explicitly to fit bounded <=512byte rows. */
static inline uint32_t host_slow_u32(uint64_t value)
{ return value > UINT32_MAX ? UINT32_MAX : (uint32_t)value; }
#endif

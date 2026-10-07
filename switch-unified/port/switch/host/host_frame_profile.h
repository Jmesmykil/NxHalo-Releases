/* Bounded frame diagnostics. No allocations or logging on ordinary frames. */
#ifndef HOST_FRAME_PROFILE_H
#define HOST_FRAME_PROFILE_H

#include <stdint.h>
#include <stdlib.h>

#define HOST_FRAME_PROFILE_WINDOW 300

struct host_frame_profile
{
    uint64_t previous_end_ns;
    uint64_t frame_ns[HOST_FRAME_PROFILE_WINDOW];
    uint64_t swap_ns;
    unsigned count;
    int initialized;
};

struct host_frame_summary
{
    uint64_t elapsed_ns, swap_ns, p50_ns, p95_ns, max_ns;
    unsigned over_33ms, over_50ms, over_100ms, max_index;
};

static inline int host_frame_compare(const void *a, const void *b)
{
    uint64_t left = *(const uint64_t *)a, right = *(const uint64_t *)b;
    return (left > right) - (left < right);
}

/* First interval starts at the first swap call. Subsequent intervals run
 * between completed presentations, including game work and diagnostic I/O.
 * swap_ns is time inside SwapWindow; it is not a measurement of GPU work. */
static inline int host_frame_record(struct host_frame_profile *profile,
    uint64_t before_ns, uint64_t after_ns, struct host_frame_summary *summary)
{
    uint64_t sorted[HOST_FRAME_PROFILE_WINDOW];
    unsigned index;

    if (!profile->initialized)
    {
        profile->previous_end_ns = before_ns;
        profile->initialized = 1;
    }
    profile->frame_ns[profile->count++] = after_ns - profile->previous_end_ns;
    profile->previous_end_ns = after_ns;
    profile->swap_ns += after_ns - before_ns;
    if (profile->count != HOST_FRAME_PROFILE_WINDOW)
        return 0;

    *summary = (struct host_frame_summary){ .swap_ns = profile->swap_ns };
    for (index = 0; index < HOST_FRAME_PROFILE_WINDOW; index++)
    {
        uint64_t duration = profile->frame_ns[index];
        sorted[index] = duration;
        summary->elapsed_ns += duration;
        if (duration > summary->max_ns)
        {
            summary->max_ns = duration;
            summary->max_index = index;
        }
        summary->over_33ms += duration > 33333333ULL;
        summary->over_50ms += duration > 50000000ULL;
        summary->over_100ms += duration > 100000000ULL;
    }
    qsort(sorted, HOST_FRAME_PROFILE_WINDOW, sizeof(sorted[0]), host_frame_compare);
    /* Nearest-rank percentiles, with ranks 150 and 285 for 300 samples. */
    summary->p50_ns = sorted[149];
    summary->p95_ns = sorted[284];
    summary->max_ns = sorted[HOST_FRAME_PROFILE_WINDOW - 1];
    profile->count = 0;
    profile->swap_ns = 0;
    return 1;
}

#endif

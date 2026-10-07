#ifndef HOST_AUDIO_PROFILE_H
#define HOST_AUDIO_PROFILE_H
#include <stdint.h>

/* Updated under the stream lock; reporting never runs in the audio callback.
 * A late callback is a deadline violation, not proof of an audible underrun. */
struct host_audio_profile {
    uint64_t callbacks, requested_bytes, supplied_bytes, partial_fills;
    uint64_t rejected_callbacks, rejected_writes, late_callbacks;
    uint64_t callback_ticks, callback_max_ticks, worker_ticks, worker_max_ticks;
};
static inline void host_audio_profile_complete(struct host_audio_profile *p,
    uint64_t elapsed_ticks, uint64_t requested, uint64_t supplied,
    uint64_t bytes_per_second)
{
    p->callbacks++;
    p->requested_bytes += requested;
    p->supplied_bytes += supplied;
    p->partial_fills += supplied < requested;
    p->callback_ticks += elapsed_ticks;
    if (elapsed_ticks > p->callback_max_ticks) p->callback_max_ticks = elapsed_ticks;
    /* SDL callback sizes are positive ints: this product fits uint64_t. */
    if (bytes_per_second && elapsed_ticks > requested * 19200000ULL / bytes_per_second)
        p->late_callbacks++;
}
#endif

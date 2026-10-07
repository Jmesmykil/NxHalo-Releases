#ifndef HOST_GPU_PROFILE_H
#define HOST_GPU_PROFILE_H

#include <stdint.h>
#include <string.h>

/* libnx system ticks are 19.2 MHz. Store ticks on the hot path; convert only
 * when reporting. Durations use unsigned subtraction across tick rollover. */
enum gpu_metric {
    GPU_READ_TOTAL, GPU_READ_MAP, GPU_WAIT,
    GPU_WRITE_TOTAL, GPU_WRITE_MAP, GPU_WRITE_COPY, GPU_WRITE_UNMAP,
    GPU_WRITE_FALLBACK,
    GPU_COMPILE, GPU_LINK, GPU_SHADER_QUERY, GPU_PROGRAM_QUERY,
    GPU_TEXTURE_UPLOAD, GPU_BUFFER_SUBDATA, GPU_BLIT,
    GPU_DRAW_ARRAYS, GPU_DRAW_ELEMENTS, GPU_DRAW_BASE_VERTEX, GPU_METRIC_COUNT
};
enum gpu_event {
    GPU_READ_MAP_FAILED, GPU_READ_UNMAP_FAILED, GPU_WRITE_MAP_FAILED,
    GPU_WRITE_UNMAP_FAILED, GPU_WAIT_ALREADY, GPU_WAIT_SATISFIED,
    GPU_WAIT_TIMEOUT, GPU_WAIT_FAILED, GPU_WAIT_OTHER, GPU_WAIT_SKIPPED,
    GPU_WRITE_BYTES, GPU_COMPILE_STATUS_CHECKS, GPU_COMPILE_STATUS_FAILED,
    GPU_LINK_STATUS_CHECKS, GPU_LINK_STATUS_FAILED, GPU_EVENT_COUNT
};
struct gpu_timing { uint64_t calls, ticks, max_ticks; };
struct gpu_profile {
    struct gpu_timing timing[GPU_METRIC_COUNT];
    uint64_t events[GPU_EVENT_COUNT];
};
static inline uint64_t gpu_saturating_add(uint64_t a, uint64_t b)
{
    return b > UINT64_MAX - a ? UINT64_MAX : a + b;
}
static inline void gpu_record(struct gpu_profile *p, enum gpu_metric metric,
                              uint64_t begin, uint64_t end)
{
    struct gpu_timing *t = &p->timing[metric];
    uint64_t elapsed = end - begin;
    t->calls = gpu_saturating_add(t->calls, 1);
    t->ticks = gpu_saturating_add(t->ticks, elapsed);
    if (elapsed > t->max_ticks) t->max_ticks = elapsed;
}
static inline void gpu_merge_metrics(struct gpu_profile *p, const struct gpu_profile *delta,
                                     unsigned metric_count)
{
    for (unsigned i = 0; i < metric_count; i++) {
        p->timing[i].calls = gpu_saturating_add(p->timing[i].calls, delta->timing[i].calls);
        p->timing[i].ticks = gpu_saturating_add(p->timing[i].ticks, delta->timing[i].ticks);
        if (delta->timing[i].max_ticks > p->timing[i].max_ticks)
            p->timing[i].max_ticks = delta->timing[i].max_ticks;
    }
    for (unsigned i = 0; i < GPU_EVENT_COUNT; i++)
        p->events[i] = gpu_saturating_add(p->events[i], delta->events[i]);
}
static inline void gpu_merge(struct gpu_profile *p, const struct gpu_profile *delta)
{
    gpu_merge_metrics(p, delta, GPU_METRIC_COUNT);
}
/* Legacy map/write/wait helpers produce only their original eight metrics.
 * Avoid scanning the new direct-call counters on every helper invocation. */
static inline void gpu_merge_helpers(struct gpu_profile *p, const struct gpu_profile *delta)
{
    gpu_merge_metrics(p, delta, GPU_COMPILE);
}
static inline void gpu_snapshot_reset(struct gpu_profile *p, struct gpu_profile *snapshot)
{
    *snapshot = *p;
    memset(p, 0, sizeof(*p));
}
static inline uint64_t gpu_ticks_us(uint64_t ticks)
{
    return (ticks / 19200000) * 1000000 + (ticks % 19200000) * 1000000 / 19200000;
}
/* Totals-only completed-call deltas. A cumulative maximum cannot recover a
 * per-frame maximum; leave delta.max_ticks zero rather than misattribute it.
 * Saturated/discontinuous counters mark the whole snapshot incomplete. */
static inline int gpu_delta_totals(const struct gpu_profile *now,
    const struct gpu_profile *previous, struct gpu_profile *delta)
{
    int valid = 1;
    memset(delta, 0, sizeof(*delta));
    for (unsigned i = 0; i < GPU_METRIC_COUNT; ++i) {
        const struct gpu_timing *a = &now->timing[i], *b = &previous->timing[i];
        if (a->calls < b->calls || a->ticks < b->ticks ||
            a->calls == UINT64_MAX || a->ticks == UINT64_MAX) valid = 0;
        else { delta->timing[i].calls = a->calls - b->calls;
               delta->timing[i].ticks = a->ticks - b->ticks; }
    }
    for (unsigned i = 0; i < GPU_EVENT_COUNT; ++i) {
        if (now->events[i] < previous->events[i] || now->events[i] == UINT64_MAX) valid = 0;
        else delta->events[i] = now->events[i] - previous->events[i];
    }
    return valid;
}
/* Exactly one boundary snapshot; no additional aggregation on each GL call. */
int host_gl_frame_snapshot(struct gpu_profile *delta);
/* Call once per reporting interval, after the frame is presented. */
void host_gl_log_stats(uint32_t frame);

#endif

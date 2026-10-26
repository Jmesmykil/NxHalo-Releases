/* Fixed storage; only the guest main/render thread owns these counters.
 * Times are CPU elapsed microseconds. Vehicle/VS times nest in game/render. */
#ifndef HALO_PHASE_PROFILE_H
#define HALO_PHASE_PROFILE_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
/* The ILP32 AArch64 guest uses a different ABI for variadic calls. Every
 * profiler must see this prototype; an implicit declaration corrupts arguments. */
extern void platform_log(const char *format, ...) __attribute__((format(printf, 1, 2)));
#define HALO_PHASE_LOG_PRIORITY 0x48475031
#define HALO_PHASE_RECORD_SIZE 512
#define HALO_PHASE_GAME 0
#define HALO_PHASE_VEHICLE 1
#define HALO_PHASE_RENDER 2
#define HALO_PHASE_VS 3
struct halo_phase_timing { uint64_t calls, total_us, max_us, max_frame; };
struct halo_phase_profile {
    struct halo_phase_timing timing[4];
    uint64_t frame, mask_changes, variant_hits, dropped_records;
};
extern struct halo_phase_profile halo_guest_phases;
uint64_t halo_profile_now_us(void);
static inline void halo_phase_add(struct halo_phase_profile *p, unsigned slot, uint64_t begin, uint64_t end, uint64_t entry_frame)
{
    struct halo_phase_timing *t=&p->timing[slot];
    uint64_t elapsed=end-begin;
    t->calls++; t->total_us+=elapsed;
    if (elapsed>t->max_us) { t->max_us=elapsed; t->max_frame=entry_frame; }
}
static inline void halo_phase_reset_window(struct halo_phase_profile *p)
{
    memset(p->timing,0,sizeof(p->timing)); p->mask_changes=0; p->variant_hits=0;
}
static inline int halo_phase_format(char *out, size_t capacity,
    const struct halo_phase_profile *p, uint64_t frame,
    uint64_t source_hits, uint64_t source_misses, uint64_t source_entries,
    uint64_t source_bytes, uint64_t source_bypassed)
{
    const struct halo_phase_timing *g=&p->timing[0], *v=&p->timing[1], *r=&p->timing[2], *vs=&p->timing[3];
#define HPU(v) ((unsigned long long)(v))
    int n=snprintf(out,capacity,
        "HP1 %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
        HPU(frame),HPU(g->calls),HPU(g->total_us),HPU(g->max_us),HPU(g->max_frame),
        HPU(v->calls),HPU(v->total_us),HPU(v->max_us),HPU(v->max_frame),
        HPU(r->calls),HPU(r->total_us),HPU(r->max_us),HPU(r->max_frame),
        HPU(vs->calls),HPU(vs->total_us),HPU(vs->max_us),HPU(p->mask_changes),HPU(p->variant_hits),
        HPU(source_hits),HPU(source_misses),HPU(source_entries),HPU(source_bytes),HPU(source_bypassed));
#undef HPU
    return n>=0 && (size_t)n<capacity ? n : -1;
}

#define HALO_FRAME_TOTAL 0
#define HALO_FRAME_EFFECTS 1
#define HALO_FRAME_SOUND 2
/* HP2 scopes are nested CPU elapsed time, completed-call windows. */
struct halo_frame_profile { struct halo_phase_timing timing[3]; uint64_t dropped_records; };
extern struct halo_frame_profile halo_guest_frame_phases;
static inline void halo_frame_phase_add(struct halo_frame_profile *p, unsigned slot,
    uint64_t begin, uint64_t end, uint64_t entry_frame)
{
    struct halo_phase_timing *t=&p->timing[slot];
    uint64_t elapsed=end-begin;
    t->calls++; t->total_us+=elapsed;
    if (elapsed>t->max_us) { t->max_us=elapsed; t->max_frame=entry_frame; }
}
static inline void halo_frame_reset_window(struct halo_frame_profile *p)
{
    memset(p->timing,0,sizeof(p->timing));
}
static inline int halo_frame_format(char *out, size_t capacity,
    const struct halo_frame_profile *p, uint64_t frame)
{
#define HFU(v) ((unsigned long long)(v))
    const struct halo_phase_timing *a=&p->timing[0], *b=&p->timing[1], *c=&p->timing[2];
    int n=snprintf(out,capacity,
        "HP2 %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
        HFU(frame),HFU(a->calls),HFU(a->total_us),HFU(a->max_us),HFU(a->max_frame),
        HFU(b->calls),HFU(b->total_us),HFU(b->max_us),HFU(b->max_frame),
        HFU(c->calls),HFU(c->total_us),HFU(c->max_us),HFU(c->max_frame));
#undef HFU
    return n>=0 && (size_t)n<capacity ? n : -1;
}
#endif

#ifndef HOST_PHASE_PROFILE_H
#define HOST_PHASE_PROFILE_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define HOST_PHASE_PRIORITY 0x48475031
#define HOST_PHASE_PACKET_SIZE 512
#define HOST_PHASE_FIELDS 23
/* Reserve room for async row label, maximum frame, prefix and newline. */
#define HOST_PHASE_TEXT_LIMIT 450
/* HP1 fields: frame; game calls/total_us/max_us/max_frame; vehicle same;
 * render same; VS generated/total_us/max_us/mask_changes/variant_hits;
 * source hits/misses/entries/bytes/bypassed. Completed scopes can cross a
 * report boundary, so max_frame can precede the nominal 300-frame window.
 * Stored verbatim only after complete numeric validation. Single guest
 * rendering thread: no receiver locks, allocations, clocks or logging. */
struct host_phase_state {
    char packet[HOST_PHASE_PACKET_SIZE];
    uint32_t pending_frame, consumed_frame;
    uint64_t rejected, mismatched, overwritten;
};
static inline void host_phase_increment(uint64_t *value)
{ if (*value != UINT64_MAX) ++*value; }
static inline int host_phase_receive(struct host_phase_state *state, const char *text)
{
    size_t length = 0, at = 4;
    uint64_t fields[HOST_PHASE_FIELDS];
    if (!text) goto reject;
    while (length < HOST_PHASE_PACKET_SIZE && text[length]) ++length;
    if (length > HOST_PHASE_TEXT_LIMIT || length < 5 || memcmp(text, "HP1 ", 4)) goto reject;
    for (unsigned i = 0; i < HOST_PHASE_FIELDS; ++i) {
        uint64_t value = 0;
        if (at >= length || text[at] < '0' || text[at] > '9') goto reject;
        do {
            unsigned digit = (unsigned)(text[at] - '0');
            if (value > (UINT64_MAX - digit) / 10) goto reject;
            value = value * 10 + digit;
            ++at;
        } while (at < length && text[at] >= '0' && text[at] <= '9');
        fields[i] = value;
        if (i + 1 < HOST_PHASE_FIELDS) {
            if (at >= length || text[at++] != ' ') goto reject;
        } else if (at != length) goto reject;
    }
    if (!fields[0] || fields[0] > UINT32_MAX || fields[0] % 300 ||
        fields[0] <= state->consumed_frame || fields[20] > 4096 ||
        fields[21] > 16 * 1024 * 1024) goto reject;
    for (unsigned i = 1; i <= 9; i += 4)
        if (fields[i + 2] > fields[i + 1] || fields[i + 3] > fields[0]) goto reject;
    if (fields[15] > fields[14]) goto reject;
    if (state->pending_frame) host_phase_increment(&state->overwritten);
    memcpy(state->packet, text, length + 1);
    state->pending_frame = (uint32_t)fields[0];
    return 1;
reject:
    host_phase_increment(&state->rejected);
    return 0;
}
static inline const char *host_phase_take(struct host_phase_state *state, uint32_t frame)
{
    if (!state->pending_frame) return NULL;
    if (state->pending_frame != frame) {
        host_phase_increment(&state->mismatched);
        if (state->pending_frame < frame) state->pending_frame = 0;
        return NULL;
    }
    state->pending_frame = 0;
    state->consumed_frame = frame;
    return state->packet;
}
/* Guest and presentation counters are independent (loading and extra swaps).
 * Preserve a validated report even when their numeric boundaries differ.
 * The caller logs both IDs; this is receipt association, not frame equivalence. */
static inline const char *host_phase_drain(struct host_phase_state *state,
    uint32_t presentation_frame, uint32_t *guest_frame)
{
    if (!state->pending_frame) return NULL;
    *guest_frame = state->pending_frame;
    if (*guest_frame != presentation_frame) host_phase_increment(&state->mismatched);
    state->consumed_frame = state->pending_frame;
    state->pending_frame = 0;
    return state->packet;
}
/* HP2: frame; game_frame, effects_frame_group, game_sound; each calls, total,
 * max, max_frame. These are completed CPU scopes, with group/audio nested. */
static inline int host_extra_phase_receive(struct host_phase_state *state, const char *text)
{
    size_t length = 0, at = 4;
    uint64_t fields[13];
    if (!text) goto reject;
    while (length < HOST_PHASE_PACKET_SIZE && text[length]) ++length;
    if (length > HOST_PHASE_TEXT_LIMIT || length < 5 || memcmp(text, "HP2 ", 4)) goto reject;
    for (unsigned i = 0; i < 13; ++i) {
        uint64_t value = 0;
        if (at >= length || text[at] < '0' || text[at] > '9') goto reject;
        do {
            unsigned digit = (unsigned)(text[at] - '0');
            if (value > (UINT64_MAX - digit) / 10) goto reject;
            value = value * 10 + digit;
            ++at;
        } while (at < length && text[at] >= '0' && text[at] <= '9');
        fields[i] = value;
        if (i + 1 < 13) {
            if (at >= length || text[at++] != ' ') goto reject;
        } else if (at != length) goto reject;
    }
    if (!fields[0] || fields[0] > UINT32_MAX || fields[0] % 300 ||
        fields[0] <= state->consumed_frame) goto reject;
    for (unsigned i = 1; i <= 9; i += 4)
        if (fields[i + 2] > fields[i + 1] || fields[i + 3] > fields[0]) goto reject;
    if (state->pending_frame) host_phase_increment(&state->overwritten);
    memcpy(state->packet, text, length + 1);
    state->pending_frame = (uint32_t)fields[0];
    return 1;
reject:
    host_phase_increment(&state->rejected);
    return 0;
}

void host_extra_phase_log_stats(uint32_t frame);
#endif

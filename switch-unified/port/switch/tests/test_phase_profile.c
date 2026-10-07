#include "host_phase_profile.h"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#define HOST_LOG_INFO 4
static unsigned ordinary_logs, buffered_logs;
static int ordinary_priority;
static size_t buffered_bytes, largest_row;
static char ordinary_text[128], output[2048];
static void host_logf(int priority, const char *format, ...)
{
    va_list args; va_start(args, format);
    vsnprintf(ordinary_text, sizeof(ordinary_text), format, args);
    va_end(args); ordinary_logs++; ordinary_priority = priority;
}
static void host_logf_buffered(int priority, const char *format, ...)
{
    assert(priority == HOST_LOG_INFO);
    va_list args; va_start(args, format);
    int length = vsnprintf(output, sizeof(output), format, args);
    va_end(args);
    assert(length > 0 && (size_t)length < sizeof(output));
    buffered_logs++; buffered_bytes += (size_t)length + 8;
    if ((size_t)length + 8 > largest_row) largest_row = (size_t)length + 8;
}
#include "host_checkpoint_profile.h"
#include "host_frame_bridge.h"
static struct host_frame_bridge frame_bridge;
static struct host_checkpoint_state checkpoint_state;
static int checkpoint_lock;
static void mutexLock(int *lock) { assert(!*lock); *lock = 1; }
static void mutexUnlock(int *lock) { assert(*lock); *lock = 0; }
#include "phase_production.h"
static void packet(char *out, size_t capacity, const uint64_t *fields)
{
    size_t at = (size_t)snprintf(out, capacity, "HP1");
    for (unsigned i = 0; i < HOST_PHASE_FIELDS; i++) {
        int n = snprintf(out + at, capacity - at, " %llu", (unsigned long long)fields[i]);
        assert(n > 0 && (size_t)n < capacity - at); at += (size_t)n;
    }
}
static void reject(const char *text)
{
    uint64_t before = phase_state.rejected;
    uint32_t pending = phase_state.pending_frame;
    char old[512]; memcpy(old, phase_state.packet, sizeof(old));
    host_log(HOST_PHASE_PRIORITY, text);
    assert(phase_state.rejected == before + 1 && ordinary_logs == 1 && !buffered_logs);
    assert(phase_state.pending_frame == pending && !memcmp(old, phase_state.packet, sizeof(old)));
}
int main(void)
{
    char text[512], altered[600];
    uint64_t fields[HOST_PHASE_FIELDS] = {300, 300, 20, 10, 299, 100, 7, 5, 200, 299, 13, 12, 299,
        8, 15, 14, 3, 2, 11, 6, 20, 3000, 0};
    host_log(5, "normal %% text");
    assert(ordinary_logs == 1 && ordinary_priority == 5 && !strcmp(ordinary_text, "normal %% text"));
    packet(text, sizeof(text), fields); host_log(HOST_PHASE_PRIORITY, text);
    assert(phase_state.pending_frame == 300 && !buffered_logs && ordinary_logs == 1);
    reject(NULL); reject(""); reject("HP1"); reject("HP1 +300"); reject("HP1 -300");
    strcpy(altered, text); strcat(altered, " 0"); reject(altered);
    strcpy(altered, text); strcat(altered, " "); reject(altered);
    strcpy(altered, text); altered[5] = 'x'; reject(altered);
    memset(altered, '0', 512); reject(altered); /* bounded unterminated buffer */
    strcpy(altered, "HP1 18446744073709551616"); reject(altered);
    fields[0] = 301; packet(altered, sizeof(altered), fields); reject(altered); fields[0] = 300;
    fields[4] = 301; packet(altered, sizeof(altered), fields); reject(altered); fields[4] = 299;
    fields[20] = 4097; packet(altered, sizeof(altered), fields); reject(altered); fields[20] = 20;
    fields[21] = 16777217; packet(altered, sizeof(altered), fields); reject(altered); fields[21] = 3000;
    fields[3] = 21; packet(altered, sizeof(altered), fields); reject(altered); fields[3] = 10;
    host_phase_log_stats(300);
    assert(buffered_logs == 2 && phase_state.consumed_frame == 300 && !phase_state.pending_frame);
    buffered_logs = 0; reject(text); /* never replay consumed packet */
    host_phase_log_stats(300); assert(buffered_logs == 1); /* errors row only */
    memset(&phase_state, 0, sizeof(phase_state)); buffered_logs = 0;
    fields[0] = 600; packet(text, sizeof(text), fields); host_log(HOST_PHASE_PRIORITY, text);
    host_phase_log_stats(300); assert(!phase_state.pending_frame && phase_state.consumed_frame == 600 && phase_state.mismatched == 1);
    host_phase_log_stats(900); assert(!phase_state.pending_frame && phase_state.mismatched == 1);
    /* Saturated uint64 fields, legal source/frame bounds and complete batch. */
    memset(&phase_state, 0, sizeof(phase_state)); buffered_logs = 0;
    for (unsigned i = 0; i < HOST_PHASE_FIELDS; i++) fields[i] = UINT64_MAX;
    fields[0] = fields[4] = fields[8] = fields[12] = UINT32_C(4294967100);
    fields[20] = 4096; fields[21] = 16777216;
    packet(text, sizeof(text), fields); host_log(HOST_PHASE_PRIORITY, text);
    assert(phase_state.pending_frame == fields[0]);
    phase_state.rejected = phase_state.mismatched = UINT64_MAX;
    buffered_bytes = largest_row = 0; host_phase_log_stats((uint32_t)fields[0]);
    assert(buffered_logs == 2 && largest_row < 512);
    size_t canonical_bytes = buffered_bytes;
    /* Leading-zero unsigned tokens can be longer than canonical formatting.
     * Accept up to450 text bytes; reject451 while retaining last good state. */
    memset(&phase_state, 0, sizeof(phase_state)); buffered_logs = 0;
    size_t base_length = strlen(text), padding = HOST_PHASE_TEXT_LIMIT - base_length;
    assert(base_length < HOST_PHASE_TEXT_LIMIT);
    memcpy(altered, "HP1 ", 4); memset(altered + 4, '0', padding);
    strcpy(altered + 4 + padding, text + 4);
    assert(strlen(altered) == HOST_PHASE_TEXT_LIMIT);
    host_log(HOST_PHASE_PRIORITY, altered); assert(phase_state.pending_frame == fields[0]);
    memmove(altered + 5, altered + 4, strlen(altered + 4) + 1); altered[4] = '0';
    reject(altered);
    phase_state.rejected = phase_state.mismatched = UINT64_MAX;
    buffered_bytes = largest_row = 0; host_phase_log_stats((uint32_t)fields[0]);
    assert(largest_row < 512 && buffered_logs == 2);
    size_t combined = buffered_bytes + 2517 + other_report_bytes();
    assert(combined <= 4096);
    printf("PASS: production HP1 receiver/report and ordinary logging, malformed/overflow/frame/source rejection, no replay, bounded rows; canonical phase %zu, padded phase %zu, GPU2517+production other%zu => batch%zu bytes\n", canonical_bytes, buffered_bytes, other_report_bytes(), combined);
    return 0;
}

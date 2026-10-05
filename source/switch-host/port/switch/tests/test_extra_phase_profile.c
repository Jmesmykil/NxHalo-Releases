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
#include "host_stall_profile.h"
static struct host_checkpoint_state checkpoint_state;
static struct host_stall_state stall_state;
static int checkpoint_lock;
static void mutexLock(int *lock) { assert(!*lock); *lock = 1; }
static void mutexUnlock(int *lock) { assert(*lock); *lock = 0; }
#include "phase_production.h"
static void packet(char *out, size_t capacity, const uint64_t *fields)
{
    size_t at = (size_t)snprintf(out, capacity, "HP2");
    for (unsigned i = 0; i < 13; i++) {
        int n = snprintf(out + at, capacity - at, " %llu", (unsigned long long)fields[i]);
        assert(n > 0 && (size_t)n < capacity - at); at += (size_t)n;
    }
}

int main(void)
{
 assert(other_report_bytes()<4096);
 char text[512], bad[600];
 uint64_t fields[13]={300,300,100,30,299,200,50,20,298,250,40,10,299};
 host_log(4,"normal");assert(ordinary_logs==1);
 packet(text,sizeof(text),fields);
 host_log(HOST_PHASE_PRIORITY,text);
 assert(extra_phase_state.pending_frame==300 && phase_state.pending_frame==0);
 host_extra_phase_log_stats(300);
 assert(buffered_logs==1 && extra_phase_state.consumed_frame==300);
 uint64_t reject0=extra_phase_state.rejected;
 host_log(HOST_PHASE_PRIORITY,text); /* consumed replay */
 assert(extra_phase_state.rejected==reject0+1);
 host_log(HOST_PHASE_PRIORITY,"HP2 18446744073709551616");
 host_log(HOST_PHASE_PRIORITY,"HP2 +300");
 assert(extra_phase_state.rejected==reject0+3 && ordinary_logs==1);
 memset(&extra_phase_state,0,sizeof(extra_phase_state));buffered_logs=0;
 fields[0]=600;packet(text,sizeof(text),fields);host_log(HOST_PHASE_PRIORITY,text);
 strcpy(bad,text);strcat(bad," 0");host_log(HOST_PHASE_PRIORITY,bad);
 assert(extra_phase_state.pending_frame==600 && extra_phase_state.rejected==1);
 host_extra_phase_log_stats(300);
 assert(extra_phase_state.pending_frame==0 && extra_phase_state.consumed_frame==600 && extra_phase_state.mismatched==1);
 host_extra_phase_log_stats(900);
 assert(extra_phase_state.pending_frame==0 && extra_phase_state.mismatched==1);
 memset(&extra_phase_state,0,sizeof(extra_phase_state));buffered_logs=0;
 fields[0]=fields[4]=fields[8]=fields[12]=4294967100U;
 for(unsigned i=1;i<13;i++)if(i!=4 && i!=8 && i!=12)fields[i]=UINT64_MAX;
 packet(text,sizeof(text),fields);
 size_t padding=HOST_PHASE_TEXT_LIMIT-strlen(text);
 memcpy(bad,"HP2 ",4);memset(bad+4,'0',padding);strcpy(bad+4+padding,text+4);
 host_log(HOST_PHASE_PRIORITY,bad);assert(extra_phase_state.pending_frame==fields[0]);
 extra_phase_state.rejected=extra_phase_state.mismatched=UINT64_MAX;
 buffered_bytes=largest_row=0;host_extra_phase_log_stats((uint32_t)fields[0]);
 assert(buffered_logs==2 && largest_row<512 && buffered_bytes<4096);
 printf("PASS: actual HP2 routing/receiver/drain; independent HP1 state, replay/malformed/overflow/stale/pending rejection, worst row%zu and separate batch%zu\n",largest_row,buffered_bytes);
 return 0;
}

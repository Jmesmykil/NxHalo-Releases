#include "host_sysinfo_guest.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
typedef uint32_t u32;
typedef struct { uint64_t addr, size; uint32_t perm; } MemoryInfo;
#define Perm_W 2
#define InfoType_TotalMemorySize 6
#define InfoType_UsedMemorySize 7
#define CUR_PROCESS_HANDLE 0xffff8001U
#define R_FAILED(r) ((r)!=0)
#define R_SUCCEEDED(r) ((r)==0)
#define BASE UINT64_C(0x88010000)
static unsigned char target[344];
static uint64_t total_bytes, used_bytes, ticks;
static unsigned writes, info_calls, memory_calls, tick_calls;
static int total_fail, used_fail, query_fail, writable = 1, owned = 1, split_regions, malformed_region;
static int host_low_owns(uintptr_t address, size_t size)
{
    return owned && address >= BASE && size <= 312 && address - BASE <= 312 - size;
}
static int svcQueryMemory(MemoryInfo *memory, u32 *page, uint64_t address)
{
    memory_calls++; *page=0;
    if (query_fail) return 1;
    memory->addr = BASE;
    memory->size = 4096;
    if (split_regions) {
        if (address < BASE + 156) memory->size = 156;
        else {memory->addr = BASE + 156; memory->size = 4096;}
    }
    if (malformed_region) memory->size = 0;
    memory->perm = writable ? Perm_W : 1;
    return 0;
}
static int svcGetInfo(uint64_t *out, u32 id, u32 handle, uint64_t subid)
{
    assert(handle==CUR_PROCESS_HANDLE && subid==0); info_calls++;
    if (id==InfoType_TotalMemorySize) { if (total_fail) return 1; *out=total_bytes; }
    else {assert(id==InfoType_UsedMemorySize); if (used_fail) return 1; *out=used_bytes;}
    return 0;
}
static uint64_t armGetSystemTick(void) {tick_calls++; return ticks;}
/* Translate only the final guest write into sanitizer-visible fixture memory.
 * Production address bounds/ownership/query checks run unmodified. */
static void *guest_memcpy(void *dest, const void *src, size_t size)
{
    uintptr_t address=(uintptr_t)dest;
    assert(address==BASE && size==312); writes++;
    return memcpy(target+16,src,size);
}
#define memcpy guest_memcpy
#include "sysinfo_production.h"
#undef memcpy
static uint32_t word(unsigned offset)
{ uint32_t value; memcpy(&value,target+16+offset,4); return value; }
static void reset(void)
{
    memset(target,0xa5,sizeof(target)); writes=info_calls=memory_calls=tick_calls=0;
    total_fail=used_fail=query_fail=split_regions=malformed_region=0; owned=writable=1;
    total_bytes=UINT64_C(3)*1024*1024*1024; used_bytes=UINT64_C(1)*1024*1024*1024;
    ticks=UINT64_C(42)*19200000;
}
static void sentinels(void)
{
    for(unsigned i=0;i<16;i++) assert(target[i]==0xa5 && target[328+i]==0xa5);
}
int main(void)
{
    reset(); assert(host_sysinfo(BASE)==0);
    assert(writes==1 && info_calls==2 && memory_calls==1 && tick_calls==1);
    assert(word(0)==42 && word(16)==786432 && word(20)==524288 && word(28)==0 && word(52)==4096);
    assert(target[56]==1 && target[57]==0); /* procs offset40 + guard16 */
    for(unsigned i=56;i<312;i++) assert(target[16+i]==0);
    for(unsigned i=4;i<16;i++) assert(target[16+i]==0);
    for(unsigned i=24;i<40;i++) assert(target[16+i]==0);
    sentinels();
    /* All invalid pointers fail before memory queries/services/writes. */
    reset(); assert(host_sysinfo(0)==-EFAULT); assert(host_sysinfo(UINT64_C(0x100000000))==-EFAULT);
    assert(host_sysinfo(UINT32_MAX-310)==-EFAULT); assert(!writes && !info_calls && !memory_calls);
    reset(); owned=0; assert(host_sysinfo(BASE)==-EFAULT && !info_calls && !memory_calls);
    reset(); writable=0; assert(host_sysinfo(BASE)==-EFAULT && !writes && !info_calls);
    reset(); query_fail=1; assert(host_sysinfo(BASE)==-EFAULT && !writes && !info_calls);
    reset(); malformed_region=1; assert(host_sysinfo(BASE)==-EFAULT && !writes && !info_calls);
    reset(); split_regions=1; assert(host_sysinfo(BASE)==0 && memory_calls==2); sentinels();
    reset(); total_fail=1; assert(host_sysinfo(BASE)==0 && word(16)==0 && word(20)==0 && word(52)==4096); sentinels();
    reset(); used_fail=1; assert(host_sysinfo(BASE)==0 && word(16)==786432 && word(20)==0); sentinels();
    reset(); used_bytes=total_bytes+1; assert(host_sysinfo(BASE)==0 && word(20)==0);
    reset(); total_bytes=UINT64_MAX; used_bytes=0; ticks=UINT64_MAX;
    assert(host_sysinfo(BASE)==0 && word(16)==UINT32_MAX && word(20)==UINT32_MAX);
    assert(word(0)==UINT32_MAX); sentinels();
    reset(); total_bytes=8193; used_bytes=4098; assert(host_sysinfo(BASE)==0 && word(16)==2 && word(20)==0); sentinels();
    puts("PASS: actual syscall179 handler initialized312byte ABI, exact offsets/unit/procs/padding,3GiB memory, null/range/ownership/write-permission rejection, two-region span, services failure fallback, saturation/flooring and guards; no logging");
    return 0;
}

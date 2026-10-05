#ifndef HOST_SYSINFO_GUEST_H
#define HOST_SYSINFO_GUEST_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
/* Exact compiled arm64_32 musl1.2.5 ABI. __lsysinfo forwards this object
 * directly to syscall179; there is no AArch64 LP64 kernel-layout shim. */
struct host_guest_sysinfo {
    uint32_t uptime, loads[3];
    uint32_t totalram, freeram, sharedram, bufferram, totalswap, freeswap;
    uint16_t procs, pad;
    uint32_t totalhigh, freehigh, mem_unit;
    unsigned char reserved[256];
};
_Static_assert(sizeof(struct host_guest_sysinfo) == 312, "guest sysinfo size");
_Static_assert(offsetof(struct host_guest_sysinfo, totalram) == 16, "totalram offset");
_Static_assert(offsetof(struct host_guest_sysinfo, freeram) == 20, "freeram offset");
_Static_assert(offsetof(struct host_guest_sysinfo, bufferram) == 28, "bufferram offset");
_Static_assert(offsetof(struct host_guest_sysinfo, mem_unit) == 52, "unit offset");
_Static_assert(offsetof(struct host_guest_sysinfo, reserved) == 56, "padding offset");
static inline int host_sysinfo_address_valid(uint64_t address)
{
    return address && address <= UINT32_MAX - sizeof(struct host_guest_sysinfo) + 1;
}
static inline uint32_t host_sysinfo_u32(uint64_t value)
{ return value > UINT32_MAX ? UINT32_MAX : (uint32_t)value; }
static inline void host_sysinfo_fill(struct host_guest_sysinfo *out,
    uint64_t uptime_seconds, uint64_t total_bytes, uint64_t used_bytes,
    int total_valid, int used_valid)
{
    memset(out, 0, sizeof(*out));
    out->uptime = host_sysinfo_u32(uptime_seconds);
    /* This is a process-scoped Linux-compatible view of Horizon's memory
     * allowance, not whole-system physical RAM. Page units avoid truncating
     * multi-GiB totals into byte-sized unsigned longs. Unknown availability
     * is conservatively zero; do not invent an Xbox free-memory budget. */
    out->mem_unit = 4096;
    out->procs = 1;
    if (total_valid) {
        out->totalram = host_sysinfo_u32(total_bytes / out->mem_unit);
        if (used_valid && used_bytes <= total_bytes) {
            out->freeram = host_sysinfo_u32((total_bytes - used_bytes) / out->mem_unit);
            if (out->freeram > out->totalram) out->freeram = out->totalram;
        }
    }
}
#endif

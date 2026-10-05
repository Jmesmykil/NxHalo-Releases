/* Fixed ILP32 guest mappings on the Switch kernel.
 * CodeMemory supplies RW data aliases and separate RW/RX code aliases.
 * No executable heap or particular ASLR heap address is assumed. */
#include "host.h"
#include <switch.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

#define NATIVE_PAGE 0x1000ULL
#define NATIVE_LOW_END 0x100000000ULL
#define NATIVE_DATA_MAPS 20

struct native_data_map { Handle handle; uintptr_t address; size_t size; void *backing; };
static struct native_data_map data_maps[NATIVE_DATA_MAPS];
static unsigned data_count;
static u64 aslr_base, aslr_size, heap_base, heap_size, alias_base, alias_size;
static Handle code_handle;
static void *code_backing, *code_writer;
static uintptr_t code_base, data_base, image_end;
static size_t code_size;
static bool code_rx_mapped;

static int native_overlap(u64 address, u64 size, u64 other, u64 other_size)
{
    return size && other_size && address < other + other_size && other < address + size;
}

/* Caller holds virtmemLock. Check reserved regions too: QueryMemory can report
 * an unused heap/IPC reservation as unmapped, although the SVC rejects it. */
static int native_range_free(uintptr_t address, size_t size)
{
    MemoryInfo info;
    u32 page_info;
    if (!size || (address & (NATIVE_PAGE - 1)) || (size & (NATIVE_PAGE - 1)) ||
        address >= NATIVE_LOW_END || size > NATIVE_LOW_END - address ||
        address < aslr_base || size > aslr_size || address - aslr_base > aslr_size - size ||
        native_overlap(address, size, heap_base, heap_size) ||
        native_overlap(address, size, alias_base, alias_size))
        return 0;
    Result rc = svcQueryMemory(&info, &page_info, address);
    return R_SUCCEEDED(rc) && info.type == MemType_Unmapped &&
        address >= info.addr && address - info.addr <= info.size &&
        size <= info.size - (address - info.addr);
}

static int native_verify(uintptr_t address, size_t size, u32 permission)
{
    MemoryInfo info;
    u32 page_info;
    Result rc = svcQueryMemory(&info, &page_info, address);
    if (R_FAILED(rc))
    {
        host_logf(HOST_LOG_ERROR, "native QueryMemory address=%lx result=0x%x", (unsigned long)address, rc);
        return -1;
    }
    host_logf(HOST_LOG_INFO, "native mapped %lx-%lx type=%u perm=%u",
        (unsigned long)address, (unsigned long)(address + size), info.type, info.perm);
    return info.perm == permission && address >= info.addr &&
        address - info.addr <= info.size && size <= info.size - (address - info.addr) ? 0 : -1;
}

static int native_map_data(uintptr_t address, size_t size)
{
    Handle handle = INVALID_HANDLE;
    if (data_count == NATIVE_DATA_MAPS) return -1;
    virtmemLock();
    int available = native_range_free(address, size);
    virtmemUnlock();
    if (!available)
    {
        host_logf(HOST_LOG_ERROR, "native data range unavailable %lx-%lx", (unsigned long)address,
            (unsigned long)(address + size));
        return -1;
    }
    void *backing = memalign(NATIVE_PAGE, size);
    if (!backing) { host_logf(HOST_LOG_ERROR, "native data backing allocation failed size=%lx", (unsigned long)size); return -1; }
    Result rc = svcCreateCodeMemory(&handle, backing, size);
    host_logf(R_FAILED(rc) ? HOST_LOG_ERROR : HOST_LOG_INFO,
        "native data CreateCodeMemory size=%lx result=0x%x", (unsigned long)size, rc);
    if (R_FAILED(rc)) { free(backing); return -1; }
    virtmemLock();
    if (!native_range_free(address, size))
    {
        virtmemUnlock();
        if (R_SUCCEEDED(svcCloseHandle(handle))) free(backing);
        return -1;
    }
    rc = svcControlCodeMemory(handle, CodeMapOperation_MapOwner, (void *)address, size, Perm_Rw);
    virtmemUnlock();
    host_logf(R_FAILED(rc) ? HOST_LOG_ERROR : HOST_LOG_INFO,
        "native data CodeMemory RW address=%lx size=%lx result=0x%x",
        (unsigned long)address, (unsigned long)size, rc);
    if (R_FAILED(rc)) { if (R_SUCCEEDED(svcCloseHandle(handle))) free(backing); return -1; }
    data_maps[data_count++] = (struct native_data_map){handle, address, size, backing};
    if (native_verify(address, size, Perm_Rw) != 0)
    {
        host_low_backing_free((void *)address, size);
        return -1;
    }
    memset((void *)address, 0, size);
    return 0;
}

void host_low_backing_free(void *address, size_t size)
{
    for (unsigned i = 0; i < data_count; i++)
    {
        struct native_data_map *map = &data_maps[i];
        if (map->address != (uintptr_t)address || map->size != size) continue;
        virtmemLock();
        Result rc = svcControlCodeMemory(map->handle, CodeMapOperation_UnmapOwner, address, size, Perm_None);
        virtmemUnlock();
        if (R_FAILED(rc))
        {
            host_logf(HOST_LOG_ERROR, "native data CodeMemory unmap address=%lx result=0x%x",
                (unsigned long)(uintptr_t)address, rc);
            return;
        }
        rc = svcCloseHandle(map->handle);
        if (R_FAILED(rc))
        {
            host_logf(HOST_LOG_ERROR, "native data CloseHandle result=0x%x; retaining backing", rc);
            return;
        }
        free(map->backing);
        *map = data_maps[--data_count];
        return;
    }
}

void *host_low_backing_allocate(size_t size)
{
    if (!size || size > 0x70000000ULL || (size & (NATIVE_PAGE - 1))) return NULL;
    /* Pool blocks follow the initial pool. The backing pointer stays private
     * to the host; the guest receives only its explicit low RW alias. */
    for (uintptr_t address = 0x90000000ULL; size <= NATIVE_LOW_END - address; )
    {
        MemoryInfo info;
        u32 page_info;
        virtmemLock();
        int available = native_range_free(address, size);
        Result rc = svcQueryMemory(&info, &page_info, address);
        virtmemUnlock();
        if (available)
            return native_map_data(address, size) == 0 ? (void *)address : NULL;
        if (R_FAILED(rc)) break;
        uintptr_t next = address + NATIVE_PAGE;
        if (native_overlap(address, size, heap_base, heap_size) && heap_base + heap_size > next)
            next = heap_base + heap_size;
        if (native_overlap(address, size, alias_base, alias_size) && alias_base + alias_size > next)
            next = alias_base + alias_size;
        if (info.type != MemType_Unmapped && info.addr + info.size > next)
            next = info.addr + info.size;
        if (next <= address || next >= NATIVE_LOW_END) break;
        address = next;
    }
    return NULL;
}

static void native_init_cleanup(void)
{
    /* Do not release backing pages if an unmap fails; process exit safely
     * reclaims them. This path is only used before any guest thread starts. */
    bool safe = true;
    virtmemLock();
    if (code_writer)
    {
        Result rc = svcControlCodeMemory(code_handle, CodeMapOperation_UnmapOwner,
            code_writer, code_size, Perm_None);
        safe = R_SUCCEEDED(rc);
        if (R_SUCCEEDED(rc)) code_writer = NULL;
    }
    if (code_rx_mapped)
    {
        Result rc = svcControlCodeMemory(code_handle, CodeMapOperation_UnmapSlave,
            (void *)code_base, code_size, Perm_None);
        safe = safe && R_SUCCEEDED(rc);
        if (R_SUCCEEDED(rc)) code_rx_mapped = false;
    }
    virtmemUnlock();
    if (safe)
    {
        if (code_handle) svcCloseHandle(code_handle);
        code_handle = INVALID_HANDLE;
        free(code_backing);
        code_backing = NULL;
    }
    while (data_count)
    {
        struct native_data_map map = data_maps[data_count - 1];
        unsigned previous = data_count;
        host_low_backing_free((void *)map.address, map.size);
        if (data_count == previous) break;
    }
}

int host_native_memory_initialize(uint32_t base, uint32_t size, uint32_t prefix_size, uint32_t xbox_base)
{
    if (data_count || code_handle || base != HALO_GUEST_IMAGE_BASE || !prefix_size ||
        prefix_size >= size || ((base | size | prefix_size) & (NATIVE_PAGE - 1)) ||
        (u64)base + size > HOST_GUEST_IMAGE_LIMIT) return -1;
    const u32 keys[] = {InfoType_AslrRegionAddress, InfoType_AslrRegionSize,
        InfoType_HeapRegionAddress, InfoType_HeapRegionSize,
        InfoType_AliasRegionAddress, InfoType_AliasRegionSize};
    u64 *values[] = {&aslr_base, &aslr_size, &heap_base, &heap_size, &alias_base, &alias_size};
    for (unsigned i = 0; i < 6; i++)
    {
        Result rc = svcGetInfo(values[i], keys[i], CUR_PROCESS_HANDLE, 0);
        if (R_FAILED(rc))
        {
            host_logf(HOST_LOG_ERROR, "native GetInfo key=%u result=0x%x", keys[i], rc);
            return -1;
        }
    }
    host_logf(HOST_LOG_INFO, "native regions aslr=%llx+%llx heap=%llx+%llx alias=%llx+%llx",
        (unsigned long long)aslr_base, (unsigned long long)aslr_size,
        (unsigned long long)heap_base, (unsigned long long)heap_size,
        (unsigned long long)alias_base, (unsigned long long)alias_size);
    code_base = base; code_size = prefix_size;
    data_base = (uintptr_t)base + prefix_size; image_end = (uintptr_t)base + size;
    /* Check each required range, including reserved-but-unmapped regions.
     * Xbox base is supplied explicitly so a standalone kernel-API probe can
     * run where the emulator has placed its own host code. The game always
     * supplies the original 0x80000000 contract. */
    const uintptr_t required_base[] = {xbox_base, base, HOST_GUEST_IMAGE_LIMIT};
    const size_t required_size[] = {HALO_GUEST_WINDOW_SIZE, size, HOST_GUEST_POOL_SIZE};
    for (unsigned i = 0; i < 3; i++)
    {
        for (unsigned j = 0; j < i; j++)
            if (native_overlap(required_base[i], required_size[i], required_base[j], required_size[j]))
                return -1;
        virtmemLock();
        int available = native_range_free(required_base[i], required_size[i]);
        virtmemUnlock();
        if (available) continue;
        for (uintptr_t address = required_base[i]; address < required_base[i] + required_size[i]; )
        {
            MemoryInfo info; u32 page_info;
            Result query = svcQueryMemory(&info, &page_info, address);
            if (R_FAILED(query)) { host_logf(HOST_LOG_ERROR, "native conflict QueryMemory result=0x%x", query); break; }
            host_logf(HOST_LOG_ERROR, "native conflict query=%lx region=%llx+%llx type=%u perm=%u",
                (unsigned long)address, (unsigned long long)info.addr,
                (unsigned long long)info.size, info.type, info.perm);
            if (info.addr + info.size <= address) break;
            address = info.addr + info.size;
        }
        host_logf(HOST_LOG_ERROR, "native required range %lx-%lx overlaps a reserved or mapped region",
            (unsigned long)required_base[i], (unsigned long)(required_base[i] + required_size[i]));
        return -1;
    }
    if (native_map_data(xbox_base, HALO_GUEST_WINDOW_SIZE) != 0 ||
        native_map_data(data_base, image_end - data_base) != 0 ||
        native_map_data(HOST_GUEST_IMAGE_LIMIT, HOST_GUEST_POOL_SIZE) != 0) goto fail;
    code_backing = memalign(NATIVE_PAGE, code_size);
    if (!code_backing) goto fail;
    Result rc = svcCreateCodeMemory(&code_handle, code_backing, code_size);
    host_logf(R_FAILED(rc) ? HOST_LOG_ERROR : HOST_LOG_INFO,
        "native CreateCodeMemory size=%lx result=0x%x", (unsigned long)code_size, rc);
    if (R_FAILED(rc)) goto fail;
    virtmemLock();
    rc = svcControlCodeMemory(code_handle, CodeMapOperation_MapSlave, (void *)code_base, code_size, Perm_Rx);
    if (R_SUCCEEDED(rc)) code_rx_mapped = true;
    virtmemUnlock();
    host_logf(R_FAILED(rc) ? HOST_LOG_ERROR : HOST_LOG_INFO,
        "native CodeMemory RX address=%lx result=0x%x", (unsigned long)code_base, rc);
    if (R_FAILED(rc)) goto fail;
    virtmemLock();
    void *writer = virtmemFindCodeMemory(code_size, NATIVE_PAGE);
    rc = writer ? svcControlCodeMemory(code_handle, CodeMapOperation_MapOwner, writer, code_size, Perm_Rw)
                : MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    if (R_SUCCEEDED(rc)) code_writer = writer;
    virtmemUnlock();
    host_logf(R_FAILED(rc) ? HOST_LOG_ERROR : HOST_LOG_INFO,
        "native CodeMemory RW alias=%lx result=0x%x", (unsigned long)(uintptr_t)writer, rc);
    if (R_FAILED(rc) || native_verify(code_base, code_size, Perm_Rx) != 0 ||
        native_verify((uintptr_t)code_writer, code_size, Perm_Rw) != 0) goto fail;

    /* Exercise the exact fixed RX mapping before loading or entering the game.
     * mov w0,#0x48; ret. The loader replaces these bytes immediately. */
    ((u32 *)code_writer)[0] = 0x52800900;
    ((u32 *)code_writer)[1] = 0xd65f03c0;
    host_native_image_flush();
    host_logf(HOST_LOG_INFO, "native execution probe entering");
    u32 probe = ((u32 (*)(void))code_base)();
    host_logf(HOST_LOG_INFO, "native execution probe returned=0x%x expected=0x48", probe);
    if (probe != 0x48) goto fail;
    memset(code_writer, 0, code_size);
    return 0;
fail:
    native_init_cleanup();
    return -1;
}

void *host_native_image_pointer(uint32_t address, size_t size)
{
    if (address >= code_base && address <= data_base && size <= data_base - address)
        return (char *)code_writer + (address - code_base);
    if (address >= data_base && address <= image_end && size <= image_end - address)
        return (void *)(uintptr_t)address;
    return NULL;
}

void host_native_image_flush(void)
{
    armDCacheFlush(code_writer, code_size);
    armICacheInvalidate((void *)code_base, code_size);
}

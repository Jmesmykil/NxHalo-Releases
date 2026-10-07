/*
HOST_MEMORY.C — Nintendo Switch

Guest memory management for the Switch host. The guest (ILP32 AArch64
code) keeps all its pointers in 32-bit registers, so every address it
touches must be below 4 GB (0x1_0000_0000).

Writable guest ranges are explicit CodeMemory RW aliases. The executable
image uses CodeMemory with a separate writable loading alias. The native
mapping layer checks the kernel's actual ASLR layout before claiming ranges.
*/

#include "host.h"

#include <switch.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <errno.h>

#define PAGE 0x1000
#define LOW_LIMIT 0x100000000ULL

/* ---------- address space reservation ---------- */

/* Pool of low-4GB address space for guest allocations (stacks, malloc
   arenas, anonymous mappings). We reserve large blocks from virtmem
   and sub-allocate from them. */

#define POOL_COUNT 16
#define POOL_BLOCK_SIZE (16 * 1024 * 1024) /* 16 MB blocks */

struct pool_block
{
	uintptr_t base;
	size_t size;
	size_t used; /* live bytes, not a high-water mark */
	uint8_t *page_used;
	VirtmemReservation *reservation;
};

static struct pool_block pools[POOL_COUNT];
static int pool_count;
static Mutex pool_lock;

/* Fixed ranges (Xbox memory window + guest image) */
static uintptr_t window_base, window_end;
static uintptr_t image_base, image_end;

static uintptr_t round_up(uintptr_t size)
{
	if (!size || size > UINTPTR_MAX - (PAGE - 1))
		return 0;
	return (size + PAGE - 1) & ~(uintptr_t)(PAGE - 1);
}

static int in_range(uintptr_t address, size_t size, uintptr_t start, uintptr_t end)
{
	return address >= start && address <= end && size <= end - address;
}

static int pool_initialize_pages(struct pool_block *block)
{
	/* One bit per guest page: 3.5 KiB for the fixed 112 MiB pool. Metadata
	 * cannot fragment or run out merely because mappings split and merge. */
	block->page_used = calloc((block->size / PAGE + 7) / 8, 1);
	return block->page_used ? 0 : -1;
}

static int pool_page_live(const struct pool_block *block, size_t page)
{
	return (block->page_used[page / 8] >> (page % 8)) & 1;
}

static void pool_set_page(struct pool_block *block, size_t page, int live)
{
	uint8_t mask = (uint8_t)(1U << (page % 8));
	if (live)
		block->page_used[page / 8] |= mask;
	else
		block->page_used[page / 8] &= (uint8_t)~mask;
}

/* Caller holds pool_lock. Ownership includes only currently mapped pages. */
static int pool_range_live(const struct pool_block *block, uintptr_t address, size_t size)
{
	size_t first, last;
	if (!size || !block->page_used ||
	    !in_range(address, size, block->base, block->base + block->size))
		return 0;
	first = (address - block->base) / PAGE;
	last = (address - block->base + size - 1) / PAGE;
	for (size_t page = first; page <= last; page++)
		if (!pool_page_live(block, page))
			return 0;
	return 1;
}

/* First-fit free runs automatically coalesce after partial unmaps. */
static void *pool_map_pages(struct pool_block *block, size_t aligned)
{
	size_t needed = aligned / PAGE, run = 0;
	if (!block->page_used || block->used > block->size || aligned > block->size - block->used)
		return NULL;
	for (size_t page = 0; page < block->size / PAGE; page++)
	{
		run = pool_page_live(block, page) ? 0 : run + 1;
		if (run == needed)
		{
			size_t first = page + 1 - needed;
			void *result = (void *)(block->base + first * PAGE);
			for (size_t allocated = first; allocated <= page; allocated++)
				pool_set_page(block, allocated, 1);
			block->used += aligned;
			memset(result, 0, aligned);
			return result;
		}
	}
	return NULL;
}

int host_memory_initialize(uint32_t img_base, uint32_t img_size, uint32_t code_size)
{
	mutexInit(&pool_lock);
	if (host_native_memory_initialize(img_base, img_size, code_size, HALO_GUEST_WINDOW_BASE) != 0)
		return -1;
	window_base = HALO_GUEST_WINDOW_BASE;
	window_end = HALO_GUEST_WINDOW_BASE + HALO_GUEST_WINDOW_SIZE;
	image_base = img_base;
	image_end = (uintptr_t)img_base + img_size;
	pools[0].base = HOST_GUEST_IMAGE_LIMIT;
	pools[0].size = HOST_GUEST_POOL_SIZE;
	pools[0].used = 0;
	pools[0].reservation = NULL;
	if (pool_initialize_pages(&pools[0]) != 0)
	{
		host_logf(HOST_LOG_ERROR, "cannot allocate initial guest pool page bitmap");
		return -1;
	}
	pool_count = 1;
	host_logf(HOST_LOG_INFO, "guest pool block 0: %08lx-%08lx (96 MB)",
		(unsigned long)pools[0].base,
		(unsigned long)(pools[0].base + pools[0].size));
	return 0;
}

/* Allocate a new pool block of low address space */
static int pool_grow(size_t minimum_size)
{
	struct pool_block *block;
	void *address = NULL;
	size_t block_size = minimum_size > POOL_BLOCK_SIZE ? minimum_size : POOL_BLOCK_SIZE;

	if (pool_count >= POOL_COUNT || !minimum_size ||
	    (minimum_size & (PAGE - 1)) || block_size >= LOW_LIMIT)
		return -1;

	/* Guest musl mallocng derives metadata from mmap page boundaries. malloc
	 * alignment is insufficient even when the returned address is below 4 GB. */
	address = host_low_backing_allocate(block_size);
	if (!address || ((uintptr_t)address & (PAGE - 1)) ||
	    (uintptr_t)address >= LOW_LIMIT || block_size > LOW_LIMIT - (uintptr_t)address)
	{
		if (address) host_low_backing_free(address, block_size);
		return -1;
	}

	block = &pools[pool_count];
	block->base = (uintptr_t)address;
	block->size = block_size;
	block->used = 0;
	block->reservation = NULL;
	if (pool_initialize_pages(block) != 0)
	{
		host_low_backing_free(address, block_size);
		return -1;
	}
	pool_count++;

	host_logf(HOST_LOG_INFO, "guest pool block %d: %08lx-%08lx",
		pool_count - 1, (unsigned long)block->base,
		(unsigned long)(block->base + block->size));

	return 0;
}

void *host_low_map(size_t size, int protection)
{
	size_t aligned = round_up(size);
	int i;

	(void)protection; /* Dynamic guest allocations are writable, never executable. */
	if (!aligned || aligned >= LOW_LIMIT)
		return NULL;

	mutexLock(&pool_lock);
	for (i = 0; i < pool_count; i++)
	{
		void *result = pool_map_pages(&pools[i], aligned);
		if (result)
		{
			mutexUnlock(&pool_lock);
			return result;
		}
	}

	/* Need a new block */
	if (pool_grow(aligned) == 0)
	{
		void *result = pool_map_pages(&pools[pool_count - 1], aligned);
		mutexUnlock(&pool_lock);
		return result;
	}

	mutexUnlock(&pool_lock);
	host_logf(HOST_LOG_ERROR, "host_low_map: out of low memory (0x%lx bytes)",
		(unsigned long)aligned);
	return NULL;
}

static int pool_release_pages(uintptr_t start, size_t aligned)
{
	if (!start || !aligned || (start & (PAGE - 1)) || start >= LOW_LIMIT ||
	    aligned > LOW_LIMIT - start)
		return -EINVAL;
	mutexLock(&pool_lock);
	for (int i = 0; i < pool_count; i++)
	{
		struct pool_block *block = &pools[i];
		if (pool_range_live(block, start, aligned))
		{
			size_t first = (start - block->base) / PAGE;
			/* Validate the whole range before changing any page. A double
			 * unmap or a range crossing a hole must not release live neighbors. */
			memset((void *)start, 0, aligned);
			for (size_t page = first; page < first + aligned / PAGE; page++)
				pool_set_page(block, page, 0);
			block->used -= aligned;
			mutexUnlock(&pool_lock);
			return 0;
		}
	}
	mutexUnlock(&pool_lock);
	return -EINVAL;
}

void host_low_unmap(void *address, size_t size)
{
	(void)pool_release_pages((uintptr_t)address, round_up(size));
}

int host_low_owns(uintptr_t address, size_t size)
{
	int i;

	if (in_range(address, size, window_base, window_end))
		return 1;
	if (in_range(address, size, image_base, image_end))
		return 1;

	mutexLock(&pool_lock);
	for (i = 0; i < pool_count; i++)
	{
		struct pool_block *block = &pools[i];
		if (pool_range_live(block, address, size))
		{
			mutexUnlock(&pool_lock);
			return 1;
		}
	}
	mutexUnlock(&pool_lock);
	return 0;
}

/* ---------- guest mmap/munmap/mprotect ---------- */

long host_guest_mmap(uint64_t address, uint64_t size, int protection,
                     int flags, int fd, int64_t offset)
{
	uint64_t length = round_up(size);
	void *result;

	(void)protection;
	(void)flags;
	(void)fd;
	(void)offset;

	if (!length || length >= LOW_LIMIT || address >= LOW_LIMIT ||
	    (address & (PAGE - 1)) || length > LOW_LIMIT - address)
		return -EINVAL;

	/* If a fixed address is requested (e.g. contiguous arena at 0x80000000)
	   or if the address falls within a region already owned/pre-mapped */
	if (address != 0 && host_low_owns((uintptr_t)address, length))
	{
		return (long)(uintptr_t)address;
	}

	/* The guest's musl never does file-backed or shared mappings;
	   it only uses anonymous private mappings for malloc and stacks */
	result = host_low_map(length, 0);
	if (!result)
		return -ENOMEM;
	return (long)(uintptr_t)result;
}

long host_guest_munmap(uint64_t address, uint64_t size)
{
	uint64_t length = round_up(size);

	if (!length || address >= LOW_LIMIT || (address & (PAGE - 1)) ||
	    length > LOW_LIMIT - address)
		return -EINVAL;

	/* Don't unmap the Xbox window or guest image */
	if (in_range(address, length, window_base, window_end))
	{
		memset((void *)(uintptr_t)address, 0, length);
		return 0;
	}
	if (in_range(address, length, image_base, image_end))
		return -EINVAL;

	return pool_release_pages((uintptr_t)address, length);
}

long host_guest_mprotect(uint64_t address, uint64_t size, int protection)
{
	/* Guest data stays RW and write tracking uses software CRCs. The loader
	 * establishes executable image permissions separately through CodeMemory.
	 * Guard-page and write-watch protection hints remain advisory here. */
	(void)address;
	(void)size;
	(void)protection;
	return 0;
}

/* ---------- write tracking (GPU texture cache coherency) ---------- */

#include "host_crc32_page.h"

/* The Xbox renderer tracks which 4 KB pages of the contiguous memory
   window have been written by the CPU, so it only re-uploads textures
   and vertex data that changed. On Android/Linux this was done with
   mprotect(PROT_READ) + SIGSEGV handler.

   On the Switch we don't have SIGSEGV for memory protection faults.
   Instead we use a software approach: the guest's memory_watch
   functions call into these host functions, and the renderer checks
   generation counters. The overhead is acceptable because the game
   only writes to a small fraction of pages per frame. */

#define WATCH_PAGE_COUNT (HALO_GUEST_WINDOW_SIZE / PAGE)

static uint8_t page_dirty[WATCH_PAGE_COUNT];
static uint32_t page_generation[WATCH_PAGE_COUNT];
static uint32_t current_generation = 1;
static uint32_t query_serial;
/* libnx permits static zero initialization. Protect all watch bookkeeping;
 * serial queries use a separate atomic token, never a content generation. */
static Mutex watch_lock;

/* The complete mapped Xbox window has exactly 32768 pages. Direct CRC
 * records cover that entire domain: range aliases share a baseline, and
 * historical vertex/texture ranges cannot fill a hash table or evict it.
 * CRC reads retain the existing <=128 KiB query limit. Whole-page checks
 * may conservatively invalidate small ranges after unrelated page writes. */
#define CRC_WATCH_MAX_SIZE (128 * 1024)
static uint32_t page_crc[WATCH_PAGE_COUNT];
static uint8_t page_crc_valid[WATCH_PAGE_COUNT];

/* Cumulative counters under watch_lock; pages_valid is a gauge.
 * Baseline CRCs are separate from changes to already observed contents. */
static struct watch_diagnostics
{
	uint64_t queries, crc_bytes, crc_baselines, crc_changed;
	uint64_t crc_page_checks, crc_queries;
	uint64_t crc_sample_queries, crc_sample_bytes, crc_sample_ticks;
	uint32_t pages_valid;
} watch_stats;
#include "host_memory_profile.h"
void host_memory_frame_snapshot(struct host_memory_frame *out)
{
    static struct watch_diagnostics previous;
    static unsigned initialized;
    struct watch_diagnostics now;
    mutexLock(&watch_lock);now=watch_stats;mutexUnlock(&watch_lock);
    memset(out,0,sizeof(*out));
    if(initialized && now.queries>=previous.queries && now.crc_bytes>=previous.crc_bytes &&
       now.crc_changed>=previous.crc_changed && now.crc_sample_queries>=previous.crc_sample_queries &&
       now.crc_sample_ticks>=previous.crc_sample_ticks) {
        out->valid=1;out->queries=now.queries-previous.queries;
        out->crc_bytes=now.crc_bytes-previous.crc_bytes;
        out->changed_pages=now.crc_changed-previous.crc_changed;
        out->sample_queries=now.crc_sample_queries-previous.crc_sample_queries;
        out->sample_ticks=now.crc_sample_ticks-previous.crc_sample_ticks;
    }
    previous=now;initialized=1;
}

void host_memory_watch_log_stats(uint32_t frame, uint64_t elapsed_ns)
{
	struct watch_diagnostics snapshot;
	uint64_t fps_milli = elapsed_ns ? 300000000000000ULL / elapsed_ns : 0;
	mutexLock(&watch_lock);
	snapshot = watch_stats;
	mutexUnlock(&watch_lock);
	/* Called once per 300 presented frames. Never hold watch_lock during I/O
	 * and never pass floating-point arguments across the logging ABI. */
	host_logf_buffered(HOST_LOG_INFO,
		"watch frame=%u interval_ms=%llu mean_fps=%llu.%03llu queries=%llu "
		"crc_bytes=%llu crc_baselines=%llu crc_changed=%llu pages_valid=%u/%u "
		"crc_page_checks=%llu crc_sample_queries=%llu crc_sample_bytes=%llu "
		"crc_sample_ns=%llu",
		frame, (unsigned long long)(elapsed_ns / 1000000),
		(unsigned long long)(fps_milli / 1000), (unsigned long long)(fps_milli % 1000),
		(unsigned long long)snapshot.queries, (unsigned long long)snapshot.crc_bytes,
		(unsigned long long)snapshot.crc_baselines, (unsigned long long)snapshot.crc_changed,
		snapshot.pages_valid, (unsigned)WATCH_PAGE_COUNT,
		(unsigned long long)snapshot.crc_page_checks,
		(unsigned long long)snapshot.crc_sample_queries,
		(unsigned long long)snapshot.crc_sample_bytes,
		(unsigned long long)armTicksToNs(snapshot.crc_sample_ticks));
}

void host_install_fault_handler(void)
{
	/* No signal handler needed on Switch — we use software tracking */
	host_logf(HOST_LOG_INFO, "using software write tracking (no SIGSEGV)");
}

void host_memory_watch_initialize(void)
{
	mutexLock(&watch_lock);
	memset(page_dirty, 0, sizeof(page_dirty));
	memset(page_generation, 0, sizeof(page_generation));
	memset(page_crc, 0, sizeof(page_crc));
	memset(page_crc_valid, 0, sizeof(page_crc_valid));
	memset(&watch_stats, 0, sizeof(watch_stats));
	current_generation = 1;
	__atomic_store_n(&query_serial, 0, __ATOMIC_RELAXED);
	mutexUnlock(&watch_lock);
}

static int in_window(uint64_t address)
{
	return address >= HALO_GUEST_WINDOW_BASE &&
	       address - HALO_GUEST_WINDOW_BASE < HALO_GUEST_WINDOW_SIZE;
}

static int range_in_window(uint64_t address, uint32_t size)
{
	return size && in_window(address) &&
		size <= (uint64_t)HALO_GUEST_WINDOW_BASE + HALO_GUEST_WINDOW_SIZE - address;
}

static uint64_t watch_page(uint64_t address)
{
	return (address - HALO_GUEST_WINDOW_BASE) / PAGE;
}

void host_memory_watch_protect(uint32_t address, uint32_t size)
{
	uint64_t first, last, p;

	if (!size || !in_window(address))
		return;
	first = watch_page(address);
	last = watch_page((uint64_t)address + size - 1);
	if (last >= WATCH_PAGE_COUNT)
		last = WATCH_PAGE_COUNT - 1;
	mutexLock(&watch_lock);
	for (p = first; p <= last; p++)
		page_dirty[p] = 0;
	mutexUnlock(&watch_lock);
}

uint32_t host_memory_watch_serial(void)
{
	/* Raw font-atlas writes cannot advance a software write counter. A fresh
	 * token defeats xgpu_texture_get's recent-entry return before its CRC
	 * query, without forcing uploads when the texture bytes are unchanged. */
	return __atomic_add_fetch(&query_serial, 1, __ATOMIC_RELAXED);
}

static uint32_t serial_crc32(const void *buf, size_t len)
{
	const uint8_t *ptr = buf;
	uint32_t crc = 0xFFFFFFFF;
	while (len >= 8)
	{
		uint64_t word;
		memcpy(&word, ptr, sizeof(word));
		crc = __builtin_aarch64_crc32x(crc, word);
		ptr += sizeof(word);
		len -= 8;
	}
	while (len > 0)
	{
		crc = __builtin_aarch64_crc32b(crc, *ptr++);
		len--;
	}
	return crc ^ 0xFFFFFFFF;
}

/* Validate both algorithms on this CPU, then select the faster one. The
 * benchmark is bounded to 24 MiB before the guest starts, never in gameplay.
 * A volatile function pointer prevents the compiler hoisting identical CRCs.
 * Failure or a nonpositive measured duration retains the serial fallback. */
static int use_parallel_crc = 1;
static uint32_t parallel_crc32(const void *buffer, size_t length)
{
    return length == PAGE ? host_crc32_page(buffer) : serial_crc32(buffer, length);
}

static uint64_t benchmark_crc(uint8_t *buffer,
                             uint32_t (*compute)(const void *, size_t),
                             uint32_t *checksum)
{
    uint32_t (*volatile operation)(const void *, size_t) = compute;
    uint32_t result = 1;
    uint64_t begin = armGetSystemTick();
    for (unsigned pass = 0; pass < 64; pass++)
        for (unsigned page = 0; page < 16; page++)
            result = (result * 33U) ^ operation(buffer + page * PAGE, PAGE);
    uint64_t ticks = armGetSystemTick() - begin;
    *checksum = result;
    return ticks;
}

static uint64_t median3(uint64_t a, uint64_t b, uint64_t c)
{
    if (a > b) { uint64_t swap = a; a = b; b = swap; }
    if (b > c) { uint64_t swap = b; b = c; c = swap; }
    return a > b ? a : b;
}

void host_memory_crc_benchmark(void)
{
    uint8_t *buffer = malloc(16 * PAGE);
    uint32_t seed = 0x7139e2a5U, serial_sum = 0, parallel_sum = 0;
    uint64_t serial_times[3], parallel_times[3];
    use_parallel_crc = 0;
    if (!buffer)
    {
        host_logf(HOST_LOG_INFO, "crc_benchmark skipped: allocation unavailable; serial CRC selected");
        return;
    }
    for (unsigned i = 0; i < 16 * PAGE; i++)
    {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        buffer[i] = (uint8_t)seed;
    }
    for (unsigned page = 0; page < 16; page++)
        if (serial_crc32(buffer + page * PAGE, PAGE) != host_crc32_page(buffer + page * PAGE))
        {
            free(buffer);
            host_logf(HOST_LOG_ERROR, "crc_benchmark mismatch; serial CRC selected");
            return;
        }
    for (unsigned trial = 0; trial < 3; trial++)
    {
        /* Alternate order to reduce warm-cache/clock-order bias. */
        if (trial & 1U)
        {
            parallel_times[trial] = benchmark_crc(buffer, parallel_crc32, &parallel_sum);
            serial_times[trial] = benchmark_crc(buffer, serial_crc32, &serial_sum);
        }
        else
        {
            serial_times[trial] = benchmark_crc(buffer, serial_crc32, &serial_sum);
            parallel_times[trial] = benchmark_crc(buffer, parallel_crc32, &parallel_sum);
        }
    }
    free(buffer);
    uint64_t serial_ticks = median3(serial_times[0], serial_times[1], serial_times[2]);
    uint64_t parallel_ticks = median3(parallel_times[0], parallel_times[1], parallel_times[2]);
    use_parallel_crc = serial_sum == parallel_sum && parallel_ticks && serial_ticks &&
                       parallel_ticks < serial_ticks;
    host_logf(HOST_LOG_INFO,
        "crc_benchmark bytes_per_trial=4194304 trials=3 serial_us=%llu parallel_us=%llu "
        "equal=%u selected=%s",
        (unsigned long long)(armTicksToNs(serial_ticks) / 1000),
        (unsigned long long)(armTicksToNs(parallel_ticks) / 1000),
        (unsigned)(serial_sum == parallel_sum), use_parallel_crc ? "parallel4" : "serial");
}

static inline uint32_t fast_crc32(const void *buffer, size_t length)
{
    return use_parallel_crc && length == PAGE ? host_crc32_page(buffer) : serial_crc32(buffer, length);
}

uint32_t host_memory_watch_generation(uint32_t address, uint32_t size)
{
	uint64_t first, last, p;
	uint32_t newest = 0;

	/* Do not hash even one byte beyond the mapped Xbox window. */
	if (!range_in_window(address, size))
		return 0;
	first = watch_page(address);
	last = watch_page((uint64_t)address + size - 1);
	mutexLock(&watch_lock);
	watch_stats.queries++;

	/* Dynamic textures (such as the 32 KB font character cache) are updated by
	   the CPU without SIGSEGV on Switch. Detect changes via fast hardware CRC32. */
	if (size <= CRC_WATCH_MAX_SIZE)
	{
        /* Time one query per 256. Sampling excludes mutex wait and never
         * skips a coherence check. These are sampled times, not total CPU
         * time; repeated call ordering can bias the sampled workload. */
        int sampled = ((watch_stats.crc_queries++ & 255U) == 0);
        uint64_t sample_start = sampled ? armGetSystemTick() : 0;
		for (p = first; p <= last; p++)
		{
			uint32_t page_address = (uint32_t)(HALO_GUEST_WINDOW_BASE + p * PAGE);
			uint32_t crc = fast_crc32((const void *)(uintptr_t)page_address, PAGE);
			watch_stats.crc_bytes += PAGE;
			watch_stats.crc_page_checks++;
			if (!page_crc_valid[p] || page_crc[p] != crc)
			{
				if (page_crc_valid[p]) watch_stats.crc_changed++;
				else
				{
					watch_stats.crc_baselines++;
					watch_stats.pages_valid++;
				}
				page_crc_valid[p] = 1;
				page_crc[p] = crc;
				page_generation[p] = ++current_generation;
			}
		}
        if (sampled)
        {
            watch_stats.crc_sample_ticks += armGetSystemTick() - sample_start;
            watch_stats.crc_sample_queries++;
            watch_stats.crc_sample_bytes += (last - first + 1) * PAGE;
        }
	}

	/* Include prepare_write and overlapping watches even when our CRC is
	 * unchanged; an entry's own generation alone can hide newer page writes. */
	for (p = first; p <= last; p++)
	{
		if (page_generation[p] > newest)
			newest = page_generation[p];
	}
	mutexUnlock(&watch_lock);
	return newest;
}

void host_memory_watch_prepare_write(uint32_t address, uint32_t size)
{
	uint64_t start = address, first, last, p;

	if (!size)
		return;
	if (start + size <= HALO_GUEST_WINDOW_BASE ||
	    start >= (uint64_t)HALO_GUEST_WINDOW_BASE + HALO_GUEST_WINDOW_SIZE)
		return;
	if (start < HALO_GUEST_WINDOW_BASE)
		start = HALO_GUEST_WINDOW_BASE;
	first = watch_page(start);
	last = watch_page((uint64_t)address + size - 1);
	if (last >= WATCH_PAGE_COUNT)
		last = WATCH_PAGE_COUNT - 1;
	mutexLock(&watch_lock);
	for (p = first; p <= last; p++)
	{
		page_generation[p] = ++current_generation;
		page_dirty[p] = 1;
	}
	mutexUnlock(&watch_lock);
}

void host_memory_watch_forget(uint32_t address, uint32_t size)
{
	uint64_t start = address, end = (uint64_t)address + size;
	uint64_t first, last, p;

	if (!size || end <= HALO_GUEST_WINDOW_BASE ||
	    start >= (uint64_t)HALO_GUEST_WINDOW_BASE + HALO_GUEST_WINDOW_SIZE)
		return;
	if (start < HALO_GUEST_WINDOW_BASE)
		start = HALO_GUEST_WINDOW_BASE;
	first = watch_page(start);
	last = watch_page(end - 1);
	if (last >= WATCH_PAGE_COUNT)
		last = WATCH_PAGE_COUNT - 1;
	mutexLock(&watch_lock);
	for (p = first; p <= last; p++)
	{
		if (page_crc_valid[p])
		{
			page_crc_valid[p] = 0;
			watch_stats.pages_valid--;
		}
		page_dirty[p] = 0;
		page_generation[p] = ++current_generation;
	}
	mutexUnlock(&watch_lock);
}

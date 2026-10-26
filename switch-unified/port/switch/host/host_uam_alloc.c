/*
HOST_UAM_ALLOC.C

The memory UAM's shader compiler allocates, a thread's own (DEKO3D.md,
phase 5). Mesa allocates all the time - some 57,000 times a shader, 43
million for the corpus of 744 - and newlib's malloc has one lock for the
whole process: compiling on two threads, each waited on it for 59% of its
time, and two threads took 66 s where one took 24 (three, 104 s). On the
Switch a lock another thread holds is a request to the kernel.

So UAM's calls to malloc, calloc, realloc, free, strdup, strndup and C++'s
new and delete are renamed to these (the build's combine step, as the
probe's Makefile does). A block of up to LARGEST_SMALL bytes comes from the
calling thread's free list of its size class, without a lock; the lists are
filled a page at a time (PAGE_SIZE, one class a page, its class in the
page's first bytes), the pages cut from regions (REGION_SIZE, aligned to
their size) taken from newlib now and then, under a lock taken once a page.
Anything larger goes to newlib, as does aligned_alloc (not renamed). A
block is this file's if its region is in the table of regions, and free
gives newlib what is not.

A block freed goes to the freeing thread's list, whichever thread took it:
memory moves between threads, and never back to newlib. A thread that
stops compiling gives its lists to a pool (host_uam_alloc_thread_end), which
a thread whose list is empty takes from before it cuts a new page: without
it, every thread that ever compiled would keep its own memory (the deko3d
probe, starting fresh workers for each of its runs, held 94 MB). Pages are
not zeroed; calloc clears what it gives.
*/

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "host_uam_alloc.h"

#define REGION_SIZE (2u * 1024 * 1024)
#define REGION_MAXIMUM 512
#define PAGE_SIZE (64u * 1024)
/* a page's first bytes: its class (a cache line, so blocks stay aligned) */
#define PAGE_HEADER 64
/* the size classes: 16 to 128 by 16, to 512 by 64, to 2048 by 256, to 8192
in larger steps (beyond, newlib: 1.7 million of a corpus's allocations were
over 2048 bytes, and each took newlib's lock) */
#define LARGEST_SMALL 8192
#define CLASS_COUNT 26

static const uint32_t class_sizes[CLASS_COUNT] =
{
	16, 32, 48, 64, 80, 96, 112, 128,
	192, 256, 320, 384, 448, 512,
	768, 1024, 1280, 1536, 1792, 2048,
	2560, 3072, 4096, 5120, 6144, 8192,
};

struct free_block
{
	struct free_block *next;
};

/* the regions, and the next page of the last (under the lock) */
static pthread_mutex_t region_lock = PTHREAD_MUTEX_INITIALIZER;
static uintptr_t regions[REGION_MAXIMUM];
static int region_count;
static uintptr_t region_next, region_end;

/* the blocks threads gave back as they stopped (under the lock) */
static struct free_block *pool_lists[CLASS_COUNT];
static unsigned long pool_bytes;

/* the calling thread's free lists */
static __thread struct free_block *thread_lists[CLASS_COUNT];

#ifdef HOST_UAM_ALLOC_SWITCHABLE
/* the deko3d probe's, to time newlib's malloc and this file's in one run:
while set, allocations go to newlib (free still knows both) */
int host_uam_alloc_bypass;
#endif

static int size_class(size_t size)
{
	int index;

	if (size <= 128)
		return size ? (int)((size - 1) / 16) : 0;
	for (index = 8; index < CLASS_COUNT; index++)
	{
		if (size <= class_sizes[index])
			return index;
	}
	return -1;
}

/* whether a block is in one of this file's regions (read without the lock:
the table only grows, and a region is in it before any block of it is
given out) */
static int block_ours(const void *block)
{
	uintptr_t base = (uintptr_t)block & ~(uintptr_t)(REGION_SIZE - 1);
	int count = __atomic_load_n(&region_count, __ATOMIC_ACQUIRE);
	int index;

	for (index = 0; index < count; index++)
	{
		if (regions[index] == base)
			return 1;
	}
	return 0;
}

static uint32_t block_size(const void *block)
{
	const uint32_t *header = (const uint32_t *)((uintptr_t)block & ~(uintptr_t)(PAGE_SIZE - 1));

	return class_sizes[*header];
}

/* a new page of a class, cut into blocks on the thread's list; 0 if there
is no memory */
static int page_fill(int class_index)
{
	uintptr_t page = 0;
	uint32_t size = class_sizes[class_index];
	uintptr_t block;

	pthread_mutex_lock(&region_lock);
	/* (blocks a thread gave back first: a page's worth at most) */
	if (pool_lists[class_index])
	{
		unsigned long taken = 0, most = (PAGE_SIZE - PAGE_HEADER) / size;

		while (pool_lists[class_index] && taken < most)
		{
			struct free_block *one = pool_lists[class_index];

			pool_lists[class_index] = one->next;
			one->next = thread_lists[class_index];
			thread_lists[class_index] = one;
			taken++;
		}
		pool_bytes -= taken * size;
		pthread_mutex_unlock(&region_lock);
		return 1;
	}
	if (region_next + PAGE_SIZE > region_end && region_count < REGION_MAXIMUM)
	{
		void *region = aligned_alloc(REGION_SIZE, REGION_SIZE);

		if (region)
		{
			regions[region_count] = (uintptr_t)region;
			__atomic_store_n(&region_count, region_count + 1, __ATOMIC_RELEASE);
			region_next = (uintptr_t)region;
			region_end = region_next + REGION_SIZE;
		}
	}
	if (region_next + PAGE_SIZE <= region_end)
	{
		page = region_next;
		region_next += PAGE_SIZE;
	}
	pthread_mutex_unlock(&region_lock);
	if (!page)
		return 0;
	*(uint32_t *)page = (uint32_t)class_index;
	for (block = page + PAGE_HEADER; block + size <= page + PAGE_SIZE; block += size)
	{
		struct free_block *free_one = (struct free_block *)block;

		free_one->next = thread_lists[class_index];
		thread_lists[class_index] = free_one;
	}
	return 1;
}

void *host_uam_malloc(size_t size)
{
	int class_index = size_class(size);
	struct free_block *block;

#ifdef HOST_UAM_ALLOC_SWITCHABLE
	if (__atomic_load_n(&host_uam_alloc_bypass, __ATOMIC_RELAXED))
		return malloc(size);
#endif
	if (class_index < 0)
		return malloc(size);
	if (!thread_lists[class_index] && !page_fill(class_index))
		return malloc(size);
	block = thread_lists[class_index];
	thread_lists[class_index] = block->next;
	return block;
}

void host_uam_free(void *block)
{
	int class_index;
	struct free_block *free_one;

	if (!block)
		return;
	if (!block_ours(block))
	{
		free(block);
		return;
	}
	class_index = (int)*(const uint32_t *)((uintptr_t)block & ~(uintptr_t)(PAGE_SIZE - 1));
	free_one = (struct free_block *)block;
	free_one->next = thread_lists[class_index];
	thread_lists[class_index] = free_one;
}

void *host_uam_calloc(size_t count, size_t size)
{
	size_t total;
	void *block;

	if (size && count > (size_t)-1 / size)
		return NULL;
	total = count * size;
	block = host_uam_malloc(total);
	if (block)
		memset(block, 0, total);
	return block;
}

void *host_uam_realloc(void *block, size_t size)
{
	uint32_t old_size;
	void *grown;

	if (!block)
		return host_uam_malloc(size);
	if (!size)
	{
		host_uam_free(block);
		return NULL;
	}
	if (!block_ours(block))
	{
		/* (newlib's, and staying newlib's: a large block, or one an
		allocation of this file fell back for) */
		return realloc(block, size);
	}
	old_size = block_size(block);
	if (size <= old_size && size_class(size) == size_class(old_size))
		return block;
	grown = host_uam_malloc(size);
	if (!grown)
		return NULL;
	memcpy(grown, block, size < old_size ? size : old_size);
	host_uam_free(block);
	return grown;
}

char *host_uam_strdup(const char *text)
{
	size_t length = strlen(text);
	char *copy = (char *)host_uam_malloc(length + 1);

	if (copy)
		memcpy(copy, text, length + 1);
	return copy;
}

char *host_uam_strndup(const char *text, size_t most)
{
	size_t length = strnlen(text, most);
	char *copy = (char *)host_uam_malloc(length + 1);

	if (copy)
	{
		memcpy(copy, text, length);
		copy[length] = 0;
	}
	return copy;
}

/* C++'s new and delete (operator new(size_t), new[], delete, delete[]):
UAM is built without exceptions, and a compile that cannot have its memory
cannot go on */
void *host_uam_new(size_t size)
{
	void *block = host_uam_malloc(size ? size : 1);

	if (!block)
		abort();
	return block;
}

void host_uam_delete(void *block)
{
	host_uam_free(block);
}

/* (new[] and delete[]: the same, under names of their own, as a rename
gives each symbol its own) */
void *host_uam_new_array(size_t size)
{
	return host_uam_new(size);
}

void host_uam_delete_array(void *block)
{
	host_uam_free(block);
}

void host_uam_alloc_thread_end(void)
{
	int class_index;

	pthread_mutex_lock(&region_lock);
	for (class_index = 0; class_index < CLASS_COUNT; class_index++)
	{
		struct free_block *last = thread_lists[class_index];
		unsigned long count = 0;

		if (!last)
			continue;
		for (count = 1; last->next; last = last->next)
			count++;
		last->next = pool_lists[class_index];
		pool_lists[class_index] = thread_lists[class_index];
		thread_lists[class_index] = NULL;
		pool_bytes += count * class_sizes[class_index];
	}
	pthread_mutex_unlock(&region_lock);
}

void host_uam_alloc_figures(struct host_uam_alloc_figures *figures)
{
	pthread_mutex_lock(&region_lock);
	figures->regions = (unsigned)region_count;
	figures->bytes = (unsigned long)region_count * REGION_SIZE -
		(unsigned long)(region_end - region_next);
	figures->pooled = pool_bytes;
	pthread_mutex_unlock(&region_lock);
}

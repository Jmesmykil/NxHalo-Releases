/* Exact generated-source reuse for the renderer's single GL context.
 *
 * Entries borrow successful shader IDs. The renderer already retains those
 * immutable GL objects, and all linked programs, until process exit. There is
 * no eviction or glDeleteShader here: a shader may belong to several programs.
 * Any future context destruction must invalidate ALL renderer caches together.
 * Only source metadata is owned here; allocation/capacity failure leaves the
 * original compilation path and its returned object intact.
 */
#ifndef HALO_SHADER_SOURCE_CACHE_H
#define HALO_SHADER_SOURCE_CACHE_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SHADER_SOURCE_CACHE_BUCKETS 1024
#define SHADER_SOURCE_CACHE_MAX_ENTRIES 4096
#define SHADER_SOURCE_CACHE_MAX_BYTES (16 * 1024 * 1024)

#ifndef SHADER_SOURCE_CACHE_ALLOC
#define SHADER_SOURCE_CACHE_ALLOC malloc
#endif
#ifndef SHADER_SOURCE_CACHE_FREE
#define SHADER_SOURCE_CACHE_FREE free
#endif

struct shader_source_entry
{
	struct shader_source_entry *next;
	uint32_t hash, stage, shader;
	size_t length;
	char source[];
};

struct shader_source_cache
{
	struct shader_source_entry *buckets[SHADER_SOURCE_CACHE_BUCKETS];
	size_t bytes;
	unsigned int entries;
	unsigned long long hits, misses, failed, bypassed;
};

#ifndef SHADER_SOURCE_CACHE_HASH
static inline uint32_t shader_source_hash(const char *source, size_t length)
{
	uint32_t hash = UINT32_C(2166136261);
	size_t index;

	for (index = 0; index < length; index++)
		hash = (hash ^ (unsigned char)source[index]) * UINT32_C(16777619);
	return hash;
}
#define SHADER_SOURCE_CACHE_HASH shader_source_hash
#endif

typedef uint32_t (*shader_source_compile_fn)(uint32_t stage, const char *source, void *user);

static inline uint32_t shader_source_cache_get(struct shader_source_cache *cache,
	uint32_t stage, const char *source, shader_source_compile_fn compile, void *user)
{
	struct shader_source_entry *entry;
	size_t length = source ? strlen(source) : 0;
	uint32_t hash = source ? SHADER_SOURCE_CACHE_HASH(source, length) : 0;
	unsigned int bucket = (hash ^ stage) % SHADER_SOURCE_CACHE_BUCKETS;
	uint32_t shader;
	size_t allocation;

	if (source)
	{
		for (entry = cache->buckets[bucket]; entry; entry = entry->next)
		{
			if (entry->hash == hash && entry->stage == stage && entry->length == length &&
				!memcmp(entry->source, source, length))
			{
				cache->hits++;
				return entry->shader;
			}
		}
	}
	cache->misses++;
	shader = compile(stage, source, user);
	if (!shader)
	{
		cache->failed++;
		return 0;
	}
	/* Check before addition, including the NUL and metadata in the byte cap. */
	if (!source || cache->entries >= SHADER_SOURCE_CACHE_MAX_ENTRIES ||
		length >= SHADER_SOURCE_CACHE_MAX_BYTES - sizeof(*entry) ||
		(allocation = sizeof(*entry) + length + 1) > SHADER_SOURCE_CACHE_MAX_BYTES - cache->bytes)
	{
		cache->bypassed++;
		return shader;
	}
	entry = SHADER_SOURCE_CACHE_ALLOC(allocation);
	if (!entry)
	{
		cache->bypassed++;
		return shader;
	}
	entry->hash = hash;
	entry->stage = stage;
	entry->shader = shader;
	entry->length = length;
	memcpy(entry->source, source, length + 1);
	entry->next = cache->buckets[bucket];
	cache->buckets[bucket] = entry;
	cache->entries++;
	cache->bytes += allocation;
	return shader;
}

/* Metadata only. Call after every consumer of borrowed IDs has been torn down.
 * The current renderer has no context teardown and never calls this. */
static inline void shader_source_cache_discard(struct shader_source_cache *cache)
{
	unsigned int bucket;

	for (bucket = 0; bucket < SHADER_SOURCE_CACHE_BUCKETS; bucket++)
	{
		struct shader_source_entry *entry = cache->buckets[bucket];
		while (entry)
		{
			struct shader_source_entry *next = entry->next;
			SHADER_SOURCE_CACHE_FREE(entry);
			entry = next;
		}
	}
	memset(cache, 0, sizeof(*cache));
}

#endif

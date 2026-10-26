/* Borrowed shader IDs for one immutable vertex program and one GL context.
 * Bounded metadata, no allocation, no eviction or GL deletion. Full tables
 * miss truthfully and retain the existing generated-source compilation path. */
#ifndef HALO_VERTEX_VARIANT_CACHE_H
#define HALO_VERTEX_VARIANT_CACHE_H
#include <stdint.h>
#define VERTEX_VARIANT_CACHE_CAPACITY 8
struct vertex_variant_entry { uint32_t packed_mask, shader; };
struct vertex_variant_cache {
    struct vertex_variant_entry entries[VERTEX_VARIANT_CACHE_CAPACITY];
    unsigned count;
};
static inline uint32_t vertex_variant_find(const struct vertex_variant_cache *cache, uint32_t mask)
{
    unsigned i;
    for (i=0;i<cache->count;i++) if (cache->entries[i].packed_mask==mask) return cache->entries[i].shader;
    return 0;
}
static inline int vertex_variant_remember(struct vertex_variant_cache *cache, uint32_t mask, uint32_t shader)
{
    unsigned i;
    if (!shader) return 0;
    for (i=0;i<cache->count;i++) if (cache->entries[i].packed_mask==mask) return cache->entries[i].shader==shader;
    if (cache->count==VERTEX_VARIANT_CACHE_CAPACITY) return 0;
    cache->entries[cache->count].packed_mask=mask;
    cache->entries[cache->count++].shader=shader;
    return 1;
}
#endif

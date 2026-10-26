#ifndef HALO_DIRECTORY_CACHE_STATE_H
#define HALO_DIRECTORY_CACHE_STATE_H
#include <stdio.h>
#include <stddef.h>
struct directory_cache_state {
 int count, total, have_snapshot, failed, fetching;
 unsigned long long updated_ms;
};
static void directory_cache_status(struct directory_cache_state const *s,
 unsigned long long now, char *out, size_t capacity)
{
 unsigned long long age = now >= s->updated_ms ? (now - s->updated_ms) / 1000 : 0;
 if (!capacity) return;
 if (!s->have_snapshot)
  snprintf(out, capacity, "%s", s->failed ? "Directory unavailable; no cached games" :
   s->fetching ? "Checking community directory..." : "Directory has not been checked");
 else if (s->failed)
  snprintf(out, capacity, "Directory unavailable; cached %d games (%llus old)", s->count, age);
 else if (s->fetching)
  snprintf(out, capacity, "Refreshing; cached %d games (%llus old)", s->count, age);
 else if (s->total > s->count)
  snprintf(out, capacity, "Directory: %d of %d games (limit; %llus old)", s->count, s->total, age);
 else if (!s->count)
  snprintf(out, capacity, "Directory: no games (checked %llus ago)", age);
 else
  snprintf(out, capacity, "Directory: %d games (checked %llus ago)", s->count, age);
}
static void directory_snapshot_status(char const *label,int have,int failed,int count,int total,
 unsigned long long age,char *out,size_t capacity)
{
 if(!have)snprintf(out,capacity,"%s snapshot unavailable",label);
 else if(failed)snprintf(out,capacity,"%s: %d cached; read failed",label,count);
 else if(total>count)snprintf(out,capacity,"%s: %d/%d file (%lluh old)",label,count,total,age/3600);
 else snprintf(out,capacity,"%s: %d file (%lluh old)",label,count,age/3600);
}
#endif

#ifndef HOST_MEMORY_PROFILE_H
#define HOST_MEMORY_PROFILE_H
#include <stdint.h>
struct host_memory_frame {
    uint64_t queries,crc_bytes,changed_pages,sample_queries,sample_ticks;
    unsigned valid;
};
void host_memory_frame_snapshot(struct host_memory_frame *out);
#endif

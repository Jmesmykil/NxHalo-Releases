#ifndef HOST_UPLOAD_DETAIL_H
#define HOST_UPLOAD_DETAIL_H
#include <stdint.h>
#include <string.h>
#define HOST_UPLOAD_DETAIL_SLOTS 3
#define HOST_UPLOAD_DETAIL_MIN_TICKS 38400 /* 2 ms at 19.2 MHz */
struct host_upload_detail {
 uint64_t ticks; uint32_t frame,kind,target,format;
 int32_t level,width,height,pixels;
 int64_t offset,bytes;
};
struct host_upload_top { struct host_upload_detail texture[3],buffer[3]; };
static inline void host_upload_remember(struct host_upload_detail top[3],struct host_upload_detail row) {
 if(row.ticks<HOST_UPLOAD_DETAIL_MIN_TICKS)return;
 unsigned i;for(i=0;i<3;i++)if(row.ticks>top[i].ticks)break;
 if(i==3)return;
 for(unsigned j=2;j>i;j--) { top[j]=top[j-1]; }
 top[i]=row;
}
#endif

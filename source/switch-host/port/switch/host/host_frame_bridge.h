#ifndef HOST_FRAME_BRIDGE_H
#define HOST_FRAME_BRIDGE_H
#include <stdint.h>
#include <string.h>
struct host_frame_bridge { uint32_t pending; uint64_t received,rejected,overwritten,unmapped; };
static inline int host_frame_bridge_receive(struct host_frame_bridge *s,const char *text)
{
    uint64_t value=0; unsigned n=0;
    if(!text || strncmp(text,"HF1 ",4))goto reject;
    text+=4;
    while(*text>='0' && *text<='9' && n++<10) {
        value=value*10+(unsigned)(*text++-'0');
        if(value>UINT32_MAX)goto reject;
    }
    if(!n || *text || !value)goto reject;
    if(s->pending)s->overwritten++;
    s->pending=(uint32_t)value;s->received++;return 1;
reject:
    s->rejected++;return 0;
}
static inline uint32_t host_frame_bridge_take(struct host_frame_bridge *s)
{
    uint32_t frame=s->pending;s->pending=0;
    if(!frame)s->unmapped++;
    return frame;
}
uint32_t host_frame_bridge_next(void);
void host_frame_bridge_report(uint32_t frame);
#endif

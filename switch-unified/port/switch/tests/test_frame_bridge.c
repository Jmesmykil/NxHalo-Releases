#include "../host/host_frame_bridge.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    struct host_frame_bridge s={0};
    assert(!host_frame_bridge_receive(&s,"HF1 4294967296"));
    assert(!host_frame_bridge_receive(&s,"HF1 0"));
    assert(!host_frame_bridge_receive(&s,"HF1 1 extra"));
    assert(!host_frame_bridge_receive(&s,"HF1 -1"));
    assert(host_frame_bridge_receive(&s,"HF1 4294967295"));
    assert(host_frame_bridge_take(&s)==UINT32_MAX);
    assert(host_frame_bridge_take(&s)==0 && s.unmapped==1);
    assert(host_frame_bridge_receive(&s,"HF1 300"));
    assert(host_frame_bridge_receive(&s,"HF1 301"));
    assert(s.overwritten==1 && host_frame_bridge_take(&s)==301);
    assert(s.rejected==4 && s.received==3);
    puts("frame bridge validates limits, malformed input, overwritten and unmapped frames");
}

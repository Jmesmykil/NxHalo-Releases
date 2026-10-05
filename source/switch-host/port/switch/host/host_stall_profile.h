#ifndef HOST_STALL_PROFILE_H
#define HOST_STALL_PROFILE_H
#include <stdint.h>
#include <string.h>
#include "host_phase_profile.h"
struct host_stall_state { char packet[3][451];uint32_t sample[3],pending,consumed,count;uint64_t rejected,mismatched; };
static inline int host_stall_receive(struct host_stall_state *s,const char *text)
{
 uint64_t values[20];size_t length=0,at=4;
 if(!text)goto reject;
 while(length<512 && text[length])length++;
 if(length>450 || length<5 || memcmp(text,"HP3 ",4))goto reject;
 for(unsigned i=0;i<20;i++){
  uint64_t v=0;if(at>=length || text[at]<'0'||text[at]>'9')goto reject;
  do{unsigned digit=(unsigned)(text[at]-'0');if(v>(UINT64_MAX-digit)/10)goto reject;v=v*10+digit;at++;}while(at<length && text[at]>='0'&&text[at]<='9');
  values[i]=v;if(i<19){if(at>=length || text[at++]!=' ')goto reject;}else if(at!=length)goto reject;
 }
 if(!values[0] || values[0]>UINT32_MAX || values[0]%300 || values[0]<=s->consumed || !values[1] || values[1]>values[0] || values[1]<=values[0]-300 || values[3]>15)goto reject;
 if(s->pending!=(uint32_t)values[0]){s->pending=(uint32_t)values[0];s->count=0;}
 for(unsigned i=0;i<s->count;i++)if(s->sample[i]==values[1])goto reject;
 if(s->count>=3)goto reject;
 s->sample[s->count]=(uint32_t)values[1];memcpy(s->packet[s->count++],text,length+1);return 1;
reject:host_phase_increment(&s->rejected);return 0;
}
static inline unsigned host_stall_take(struct host_stall_state *s,uint32_t frame)
{
 if(!s->pending)return 0;
 if(s->pending!=frame){host_phase_increment(&s->mismatched);if(s->pending<frame){s->pending=0;s->count=0;}return 0;}
 unsigned count=s->count;s->pending=0;s->count=0;s->consumed=frame;return count;
}
void host_stall_log_stats(uint32_t frame);
#endif

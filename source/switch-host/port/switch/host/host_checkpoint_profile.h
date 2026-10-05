#ifndef HOST_CHECKPOINT_PROFILE_H
#define HOST_CHECKPOINT_PROFILE_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "host_phase_profile.h"
#define HOST_CHECKPOINT_CAPACITY 8
#define HOST_CHECKPOINT_TEXT 128
/* HC1 seq:u32>0 guestframe:u64 op storage bytes<=16MiB duration_us:u64 result:bool.
 * Caller serializes receive/take. Stable FIFO copies never borrow guest strings.
 * Drain count describes delivery to async producer, not durable disk delivery. */
struct host_checkpoint_state {
 char packet[HOST_CHECKPOINT_CAPACITY][HOST_CHECKPOINT_TEXT];
 unsigned head,count;
 uint64_t accepted,drained,dropped,rejected;
};
struct host_checkpoint_batch {
 char packet[HOST_CHECKPOINT_CAPACITY][HOST_CHECKPOINT_TEXT];
 unsigned count;
 uint64_t accepted,drained,dropped,rejected;
};
static inline int host_checkpoint_word(const char *p,size_t n,const char *word)
{ return strlen(word)==n&&!memcmp(p,word,n); }
static inline int host_checkpoint_receive(struct host_checkpoint_state *s,const char *text)
{
 size_t length=0,at=4,start[7],size[7];uint64_t v[7]={0};
 if(!text)goto reject;
 while(length<HOST_CHECKPOINT_TEXT&&text[length])length++;
 if(length>=HOST_CHECKPOINT_TEXT||length<5||memcmp(text,"HC1 ",4))goto reject;
 for(unsigned i=0;i<7;i++){
  start[i]=at;while(at<length&&text[at]!=' ')at++;size[i]=at-start[i];
  if(!size[i])goto reject;
  if(i<6){if(at>=length||text[at++]!=' ')goto reject;}else if(at!=length)goto reject;
 }
 for(unsigned i=0;i<7;i++){
  if(i==2||i==3)continue;
  for(size_t j=0;j<size[i];j++){
   unsigned char c=(unsigned char)text[start[i]+j];
   if(c<'0'||c>'9'||v[i]>(UINT64_MAX-(c-'0'))/10)goto reject;
   v[i]=v[i]*10+c-'0';
  }
 }
 if(!v[0]||v[0]>UINT32_MAX||v[4]>16*1024*1024||v[6]>1)goto reject;
 if(!host_checkpoint_word(text+start[2],size[2],"save")&&!host_checkpoint_word(text+start[2],size[2],"restore"))goto reject;
 if(!host_checkpoint_word(text+start[3],size[3],"ram")&&!host_checkpoint_word(text+start[3],size[3],"legacy")&&!host_checkpoint_word(text+start[3],size[3],"bounds")&&!host_checkpoint_word(text+start[3],size[3],"invalid"))goto reject;
 host_phase_increment(&s->accepted);
 if(s->count==HOST_CHECKPOINT_CAPACITY){host_phase_increment(&s->dropped);return 0;}
 memcpy(s->packet[(s->head+s->count)%HOST_CHECKPOINT_CAPACITY],text,length+1);s->count++;return 1;
reject:host_phase_increment(&s->rejected);return 0;
}
static inline void host_checkpoint_take(struct host_checkpoint_state *s,struct host_checkpoint_batch *out)
{
 out->count=s->count;
 for(unsigned i=0;i<out->count;i++)memcpy(out->packet[i],s->packet[(s->head+i)%HOST_CHECKPOINT_CAPACITY],HOST_CHECKPOINT_TEXT);
 s->head=(s->head+s->count)%HOST_CHECKPOINT_CAPACITY;s->count=0;
 for(unsigned i=0;i<out->count;i++)host_phase_increment(&s->drained);
 out->accepted=s->accepted;out->drained=s->drained;out->dropped=s->dropped;out->rejected=s->rejected;
}
#endif

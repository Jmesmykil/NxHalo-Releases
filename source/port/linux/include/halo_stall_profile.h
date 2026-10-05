#ifndef HALO_STALL_PROFILE_H
#define HALO_STALL_PROFILE_H
#include "halo_phase_profile.h"
#define HALO_STALL_COUNT 8
#define HALO_STALL_STATE 0
#define HALO_STALL_SAVE 1
#define HALO_STALL_INPUT 2
#define HALO_STALL_TIME_CONTROL 3
#define HALO_STALL_DIRECTOR 4
#define HALO_STALL_QUEUE 5
#define HALO_STALL_TEXTURE 6
#define HALO_STALL_THROTTLE 7
struct halo_stall_record { uint64_t frame,elapsed,flags,us[8],calls[8]; };
struct halo_stall_profile { uint64_t baseline,last_frame,generation; unsigned enabled,valid[3]; struct halo_stall_record current,top[3]; };
struct halo_stall_ticket { uint64_t begin,generation;unsigned active; };
extern _Thread_local struct halo_stall_profile halo_guest_stalls;
static inline uint64_t halo_stall_sum(uint64_t a,uint64_t b,uint64_t *flags)
{if(UINT64_MAX-a<b){*flags|=1;return UINT64_MAX;}return a+b;}
static inline struct halo_stall_ticket halo_stall_begin(void)
{if(!halo_guest_stalls.enabled)return (struct halo_stall_ticket){0};
 return (struct halo_stall_ticket){halo_profile_now_us(),halo_guest_stalls.generation,1};}
static inline void halo_stall_complete(struct halo_stall_profile *p,unsigned scope,struct halo_stall_ticket ticket,uint64_t end)
{
 if(scope>=8 || !ticket.active)return;
 if(end<ticket.begin){p->current.flags|=4;return;}
 if(ticket.generation!=p->generation)p->current.flags|=8;
 p->current.us[scope]=halo_stall_sum(p->current.us[scope],end-ticket.begin,&p->current.flags);
 p->current.calls[scope]=halo_stall_sum(p->current.calls[scope],1,&p->current.flags);
}
static inline void halo_stall_end(unsigned scope,struct halo_stall_ticket ticket)
{if(!ticket.active || !halo_guest_stalls.enabled)return;
 halo_stall_complete(&halo_guest_stalls,scope,ticket,halo_profile_now_us());}
static inline void halo_stall_pre(struct halo_stall_profile *p,uint64_t frame,uint64_t now)
{
 struct halo_stall_record r=p->current;
 if(p->enabled && now>=p->baseline){
  r.frame=frame;r.elapsed=now-p->baseline;
  if(frame!=p->last_frame+1)r.flags|=2;
  unsigned slot=0;for(unsigned i=0;i<3;i++){if(!p->valid[i]){slot=i;break;}if(p->top[i].elapsed<p->top[slot].elapsed)slot=i;}
  if(!p->valid[slot] || r.elapsed>p->top[slot].elapsed){p->top[slot]=r;p->valid[slot]=1;}
 }
 memset(&p->current,0,sizeof(p->current));p->last_frame=frame;p->generation++;
}
/* Called immediately after platform_video_swap returns. Post-swap fences,
 * waits and main-loop tail therefore belong to the NEXT pre-swap interval. */
static inline void halo_stall_post(struct halo_stall_profile *p,uint64_t now)
{p->baseline=now;p->enabled=1;}
static inline int halo_stall_format(char *out,size_t n,uint64_t report,const struct halo_stall_record *r)
{
#define SU(v) ((unsigned long long)(v))
 int length=snprintf(out,n,"HP3 %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
 SU(report),SU(r->frame),SU(r->elapsed),SU(r->flags),SU(r->us[0]),SU(r->us[1]),SU(r->us[2]),SU(r->us[3]),SU(r->us[4]),SU(r->us[5]),SU(r->us[6]),SU(r->us[7]),SU(r->calls[0]),SU(r->calls[1]),SU(r->calls[2]),SU(r->calls[3]),SU(r->calls[4]),SU(r->calls[5]),SU(r->calls[6]),SU(r->calls[7]));
#undef SU
 return length>=0 && (size_t)length<n && length<=450 ? length:-1;
}
#ifdef HALO_ANDROID
#define STALL_BEGIN(name) struct halo_stall_ticket name=halo_stall_begin()
#define STALL_END(scope,name) halo_stall_end(scope,name)
#else
#define STALL_BEGIN(name) ((void)0)
#define STALL_END(scope,name) ((void)0)
#endif
#endif

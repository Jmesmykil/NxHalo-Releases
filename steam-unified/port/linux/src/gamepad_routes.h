/* Stable gamepad identity ordering, independent of SDL enumeration order. */
#ifndef HALO_GAMEPAD_ROUTES_H
#define HALO_GAMEPAD_ROUTES_H
#include <stdint.h>
#define HALO_ROUTE_PORTS 4
struct halo_route_candidate { uint32_t id; int rank; };
static int halo_route_has(const uint32_t *out, int n, uint32_t id) {
    int i; for(i=0;i<n;i++) if(out[i]==id) return 1; return 0;
}
static int halo_route_present(const struct halo_route_candidate *in,int n,uint32_t id) {
 int i; for(i=0;i<n;i++) if(id && in[i].id==id) return 1; return 0;
}
static int halo_route_assign(const struct halo_route_candidate *in, int n,
 const uint32_t previous[HALO_ROUTE_PORTS], uint32_t out[HALO_ROUTE_PORTS]) {
 int i,p,rank,best=4,previous_rank=4,span=0; uint32_t primary=0;
 for(p=0;p<HALO_ROUTE_PORTS;p++) out[p]=0;
 for(i=0;i<n;i++) if(in[i].id) {
  if(in[i].rank<best) { best=in[i].rank; primary=in[i].id; }
  if(in[i].id==previous[0]) previous_rank=in[i].rank;
 }
 if(previous_rank==best && previous_rank<4) primary=previous[0];
 out[0]=primary;
 /* Preserve existing split-player identities in their ports. */
 for(p=1;p<HALO_ROUTE_PORTS;p++)
  if(previous[p]!=primary && halo_route_present(in,n,previous[p]) && !halo_route_has(out,p,previous[p])) out[p]=previous[p];
 /* A superseded primary becomes the first unoccupied secondary. */
 if(previous[0]!=primary && halo_route_present(in,n,previous[0]) && !halo_route_has(out,HALO_ROUTE_PORTS,previous[0]))
  for(p=1;p<HALO_ROUTE_PORTS;p++) if(!out[p]) { out[p]=previous[0]; break; }
 for(rank=0;rank<4;rank++) for(i=0;i<n;i++)
  if(in[i].id && in[i].rank==rank && !halo_route_has(out,HALO_ROUTE_PORTS,in[i].id))
   for(p=1;p<HALO_ROUTE_PORTS;p++) if(!out[p]) { out[p]=in[i].id; break; }
 for(p=0;p<HALO_ROUTE_PORTS;p++) if(out[p]) span=p+1;
 return span;
}
#endif

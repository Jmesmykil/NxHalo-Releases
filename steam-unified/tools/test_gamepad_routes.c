#include "gamepad_routes.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void run(const struct halo_route_candidate *c,int n,uint32_t old[4],uint32_t a,uint32_t b,uint32_t d,uint32_t e) {
 uint32_t out[4],expect[4]={a,b,d,e}; int count=halo_route_assign(c,n,old,out);
 assert(count==(e?4:d?3:b?2:a?1:0)); assert(!memcmp(out,expect,sizeof(out))); memcpy(old,out,sizeof(out));
}
int main(void) {
 uint32_t old[4]={0};
 struct halo_route_candidate initial[]={{2,3},{3,0}}, reordered[]={{3,0},{2,3}}, late[]={{9,2},{2,3},{3,0}}, peer[]={{10,0},{9,2},{2,3},{3,0}}, gone[]={{9,2},{2,3}}, returned[]={{11,0},{2,3},{9,2}}, duplicates[]={{11,0},{11,0},{2,3}};
 run(initial,2,old,3,2,0,0);run(reordered,2,old,3,2,0,0);run(late,3,old,3,2,9,0);run(peer,4,old,3,2,9,10);run(gone,2,old,9,2,0,0);run(returned,3,old,11,2,9,0);run(duplicates,3,old,11,2,0,0);run(NULL,0,old,0,0,0,0);
 puts("PASS: stable reorder, delayed virtual, same-rank retention, disconnect/reconnect, dedup, no-device"); return 0;
}

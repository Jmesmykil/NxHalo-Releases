#include "../port/linux/src/directory_filter.h"
#include "../port/linux/src/directory_cache_state.h"
#include <assert.h>
#include <string.h>
int main(void) {
 assert(directory_filter_accepts(SERVER_VIEW_ALL,24,1,1));
 assert(directory_filter_accepts(SERVER_VIEW_ALL,65535,1,0));
 assert(directory_filter_accepts(SERVER_VIEW_NATIVE_V24,24,1,1));
 assert(!directory_filter_accepts(SERVER_VIEW_NATIVE_V24,21,1,1));
 assert(!directory_filter_accepts(SERVER_VIEW_NATIVE_V24,-2,3,1));

 int version, engine, source, count=0;
 for(version=-3;version<=22;version++) for(engine=0;engine<=5;engine++) for(source=0;source<=3;source++) {
  assert(directory_filter_accepts(SERVER_VIEW_ALL,version,source,engine));
  assert(directory_filter_accepts(SERVER_VIEW_CLASSIC_CE,version,source,engine)==(version==-2));
  assert(directory_filter_accepts(SERVER_VIEW_CLASSIC_PC,version,source,engine)==(version==-3));
  assert(directory_filter_accepts(SERVER_VIEW_CAMPAIGN,version,source,engine)==(version>=0&&engine==0));
  assert(directory_filter_accepts(SERVER_VIEW_MULTIPLAYER,version,source,engine)==(version>=0&&engine!=0));
  assert(directory_filter_accepts(SERVER_VIEW_CURRENT,version,source,engine)==(version==21));
  assert(directory_filter_accepts(SERVER_VIEW_LEGACY,version,source,engine)==(version>=11&&version<=20));
  assert(directory_filter_accepts(SERVER_VIEW_ANNOUNCED,version,source,engine)==(version>=0&&source==2));
  assert(directory_filter_accepts(SERVER_VIEW_BROKER,version,source,engine)==(version>=0&&source==1));
  count+=9;
 }
 { struct directory_cache_state s={0}; char out[128], tiny[1]={'x'};
  directory_cache_status(&s,0,out,sizeof(out));assert(strstr(out,"not been checked"));
  s.fetching=1;directory_cache_status(&s,0,out,sizeof(out));assert(strstr(out,"Checking"));
  s.fetching=0;s.failed=1;directory_cache_status(&s,0,out,sizeof(out));assert(strstr(out,"no cached"));
  s.have_snapshot=1;s.failed=0;s.updated_ms=1000;
  directory_cache_status(&s,6000,out,sizeof(out));assert(strstr(out,"no games")&&strstr(out,"5s"));
  s.count=4;s.total=4;directory_cache_status(&s,6000,out,sizeof(out));assert(strstr(out,"4 games"));
  s.failed=1;directory_cache_status(&s,6000,out,sizeof(out));assert(strstr(out,"unavailable")&&strstr(out,"cached 4"));
  s.failed=0;s.fetching=1;directory_cache_status(&s,6000,out,sizeof(out));assert(strstr(out,"Refreshing"));
  s.fetching=0;s.total=513;s.count=512;directory_cache_status(&s,6000,out,sizeof(out));assert(strstr(out,"512 of 513")&&strstr(out,"limit"));
  directory_cache_status(&s,0,tiny,sizeof(tiny));assert(tiny[0]==0);
  directory_cache_status(&s,0,tiny,0);assert(tiny[0]==0);
  directory_snapshot_status("CE",0,1,0,0,0,out,sizeof(out));assert(strstr(out,"unavailable"));
  directory_snapshot_status("CE",1,1,2,2,0,out,sizeof(out));assert(strstr(out,"2 cached")&&strstr(out,"read failed"));
  directory_snapshot_status("PC",1,0,0,0,7200,out,sizeof(out));assert(strstr(out,"0 file")&&strstr(out,"2h"));
  directory_snapshot_status("CE",1,0,512,514,3600,out,sizeof(out));assert(strstr(out,"512/514")&&strstr(out,"1h"));

 }
 printf("PASS directory filter %d assertions and freshness/empty/error/capacity states\n",count);
 return 0;
}

/* Private native lab only. Disabled unless explicitly enabled on a loopback host.
 * Synthetic idle bipeds are not network players or a real battle workload. */
#include <stdlib.h>
int config_boolean(const char *name);
#include "render/render_cameras.h"
#include "physics/collisions.h"
#include "objects/object_definitions.h"
#include "items/projectiles.h"
#include "game/game_globals.h"
#include "scenario/scenario.h"
#include "halo_phase_profile.h"
static struct {
 int checked, enabled, ready, phase, requested, spawned, frames, fill_phase;
 uint64_t next_refill;
 long objects[128], model;
 real_point3d center, cameras[3];
 uint64_t since, render_us, render_max, histogram[1001], lod[5], shadows, nodes;
} crowd_bench;
int halo_private_corpse_unbounded(void)
{
 const char *v=getenv("HALO_PRIVATE_CORPSE_UNBOUNDED");
 return crowd_bench.enabled && v && !strcmp(v,"1");
}
void crowd_benchmark_model(long model, int lod, int shadow, int nodes)
{
 if (!crowd_bench.ready || model != crowd_bench.model ||
     halo_profile_now_us()-crowd_bench.since < 4000000) return;
 if (shadow) crowd_bench.shadows++;
 else if (lod>=0 && lod<5) crowd_bench.lod[lod]++;
 crowd_bench.nodes += nodes;
}
void crowd_benchmark_render_time(uint64_t elapsed)
{
 if (!crowd_bench.ready || halo_profile_now_us()-crowd_bench.since < 4000000) return;
 crowd_bench.frames++; crowd_bench.render_us+=elapsed;
 if(elapsed>crowd_bench.render_max)crowd_bench.render_max=elapsed;
 crowd_bench.histogram[MIN(elapsed/100,1000)]++;
}
void crowd_benchmark_camera(struct render_window *window)
{
 if (!crowd_bench.ready) return;
 int mode=crowd_bench.phase%3;
 struct render_camera *c=&window->render_camera;
 c->position=crowd_bench.cameras[mode];
 c->forward.i=crowd_bench.center.x-c->position.x;
 c->forward.j=crowd_bench.center.y-c->position.y;
 c->forward.k=crowd_bench.center.z+1.0f-c->position.z;
 normalize3d(&c->forward);
 if(mode==2){c->forward.i=-c->forward.i;c->forward.j=-c->forward.j;c->forward.k=-c->forward.k;}
 observer_up_from_forward(&c->forward,&c->up);
 window->rasterizer_camera=*c;
}
static void crowd_benchmark_reset(void)
{
 crowd_bench.frames=0;crowd_bench.render_us=0;crowd_bench.render_max=0;
 memset(crowd_bench.histogram,0,sizeof(crowd_bench.histogram));
 memset(crowd_bench.lod,0,sizeof(crowd_bench.lod));
 crowd_bench.shadows=0;crowd_bench.nodes=0;crowd_bench.since=halo_profile_now_us();
}
static void crowd_benchmark_update(boolean menu)
{
 if(!crowd_bench.checked){
  const char *enable=getenv("HALO_PRIVATE_CROWD_BENCH");
  crowd_bench.checked=1;
  crowd_bench.enabled=enable && !strcmp(enable,"1") && network_test.mode==_network_test_host &&
   !strncmp(config_string("network.address"),"127.",4) &&
   !strcmp(network_test.map_name,"bloodgulch") && !config_boolean("network.online") &&
   !config_boolean("network.list_hosted_games") && !*config_string("network.browser_url");
  if(enable && !crowd_bench.enabled)platform_log("[crowd_bench] REFUSED: requires explicit loopback test host");
 }
 if(!crowd_bench.enabled || menu || !game_in_progress() || game_time_get()<90)return;
 if(!crowd_bench.ready){
  long player=local_player_get_player_index(0);
  if(player==NONE || player_get(player)->unit_index==NONE)return;
  struct object_datum *unit=object_get(player_get(player)->unit_index);
  crowd_bench.center=unit->object.position;
  /* Blood Gulch private fixture: central valley, away from spawn cliffs. */
  crowd_bench.center.x=67.0f;crowd_bench.center.y=-121.0f;crowd_bench.center.z=100.0f;
  {struct collision_result ground;real_vector3d down={0,0,-300.0f};
   if(!collision_test_vector(FLAG(_collision_test_structure_bit)|FLAG(_collision_test_front_facing_surfaces_bit),&crowd_bench.center,&down,NONE,&ground)){
    platform_log("[crowd_bench] FAILED camera floor");crowd_bench.enabled=0;return;
   }
   crowd_bench.center=ground.point;
  }
  for(int mode=0;mode<3;mode++){
   real_point3d above=crowd_bench.center;struct collision_result ground;real_vector3d down={0,0,-300.0f};
   above.x-=mode==1?40.0f:12.0f;above.z=100.0f;
   if(!collision_test_vector(FLAG(_collision_test_structure_bit)|FLAG(_collision_test_front_facing_surfaces_bit),&above,&down,NONE,&ground)){
    platform_log("[crowd_bench] FAILED camera floor mode=%d",mode);crowd_bench.enabled=0;return;
   }
   crowd_bench.cameras[mode]=ground.point;crowd_bench.cameras[mode].z+=2.0f;
   platform_log("[crowd_bench] camera mode=%d xyz=(%.2f,%.2f,%.2f) clearance=2.0",mode,ground.point.x,ground.point.y,ground.point.z+2.0f);
  }
  crowd_bench.model=object_definition_get(unit->definition_index)->object.model.index;
  for(int i=0;i<128;i++)crowd_bench.objects[i]=NONE;
  crowd_bench.ready=1;
  crowd_benchmark_reset();
 }
 if(halo_profile_now_us()-crowd_bench.since>=12000000){
  unsigned cumulative=0,p95=100000;
  for(int i=0;i<=1000;i++){cumulative+=crowd_bench.histogram[i];if(cumulative*100>=(unsigned)crowd_bench.frames*95){p95=(i+1)*100;break;}}
  int live=0;
  for(int j=0;j<crowd_bench.spawned;j++)if(object_try_and_get(crowd_bench.objects[j]))live++;
  platform_log("[crowd_bench] result phase=%d count=%d live=%d mode=%d frames=%d render_mean_us=%llu render_p95_upper_us=%u render_max_us=%llu lod=%llu,%llu,%llu,%llu,%llu shadows=%llu nodes=%llu",
   crowd_bench.phase,crowd_bench.spawned,live,crowd_bench.phase%3,crowd_bench.frames,
   (unsigned long long)(crowd_bench.frames?crowd_bench.render_us/crowd_bench.frames:0),p95,(unsigned long long)crowd_bench.render_max,
   (unsigned long long)crowd_bench.lod[0],(unsigned long long)crowd_bench.lod[1],(unsigned long long)crowd_bench.lod[2],(unsigned long long)crowd_bench.lod[3],(unsigned long long)crowd_bench.lod[4],(unsigned long long)crowd_bench.shadows,(unsigned long long)crowd_bench.nodes);
  if(++crowd_bench.phase>=12){platform_log("[crowd_bench] COMPLETE");crowd_bench.enabled=0;crowd_bench.ready=0;return;}
  crowd_benchmark_reset();
 }
 if(getenv("HALO_PRIVATE_CROWD_DEAD") && !strcmp(getenv("HALO_PRIVATE_CROWD_DEAD"),"1")) {
  int keep=0;
  for(int i=0;i<crowd_bench.spawned;i++)
   if(object_try_and_get(crowd_bench.objects[i]))crowd_bench.objects[keep++]=crowd_bench.objects[i];
  crowd_bench.spawned=keep;
 }
 /* Repeat real grenade projectiles in the isolated fixture, including their
  * normal damage/effects path. Never enabled on a public/online host. */
 if(getenv("HALO_PRIVATE_CROWD_EXPLOSIONS") && !strcmp(getenv("HALO_PRIVATE_CROWD_EXPLOSIONS"),"1") && crowd_bench.phase>=3) {
  static long last_burst=-1; static unsigned launched;
  long tick=game_time_get();
  if(tick!=last_burst && tick%15==0){
   last_burst=tick;
   struct game_globals *globals=scenario_get_game_globals();
   if(globals->grenades.count){
    struct game_globals_grenade *g=TAG_BLOCK_GET_ELEMENT(&globals->grenades,0,struct game_globals_grenade);
    if(g->projectile.index!=NONE)for(int i=0;i<4;i++){
     struct object_placement_data p;object_placement_data_new(&p,g->projectile.index,NONE);
     p.position=crowd_bench.center;p.position.x+=(launched%8)*1.1f;p.position.y+=(int)(launched%16)-7.5f;p.position.z+=1.0f;
     p.forward=*global_forward3d;p.up=*global_up3d;
     long idx=object_new(&p);
     if(idx!=NONE){struct projectile_datum *v=projectile_get(idx);v->projectile.detonation_timer=1.0f;v->projectile.arming_time=1.0f;launched++;}
    }
   }
   if(tick%150==0)platform_log("[crowd_bench] grenades_launched=%u tick=%ld",launched,tick);
  }
 }
 int counts[]={0,16,64,128};
 crowd_bench.requested=counts[crowd_bench.phase/3];
 if(crowd_bench.spawned<crowd_bench.requested){
  uint64_t now=halo_profile_now_us();
  int allowance=2;
  if(crowd_bench.fill_phase!=crowd_bench.phase/3){allowance=128;crowd_bench.fill_phase=crowd_bench.phase/3;}
  else if(now<crowd_bench.next_refill)return;
  crowd_bench.next_refill=now+1000000;

  long player=local_player_get_player_index(0);if(player==NONE || player_get(player)->unit_index==NONE)return;
  long definition=object_get(player_get(player)->unit_index)->definition_index;
  while(crowd_bench.spawned<crowd_bench.requested && allowance-->0){
   int i=crowd_bench.spawned;struct object_placement_data p;struct collision_result hit;
   object_placement_data_new(&p,definition,NONE);
   p.position=crowd_bench.center;p.position.x+=(i/16)*1.1f;p.position.y+=(i%16-7.5f)*1.1f;p.position.z=100.0f;
   real_vector3d down={0,0,-300.0f};
   if(!collision_test_vector(FLAG(_collision_test_structure_bit)|FLAG(_collision_test_front_facing_surfaces_bit),&p.position,&down,NONE,&hit)){
    platform_log("[crowd_bench] FAILED floor index=%d",i);crowd_bench.enabled=0;crowd_bench.ready=0;return;
   }
   p.position=hit.point;p.position.z+=0.15f;p.forward=*global_forward3d;p.up=*global_up3d;
   long object=object_new(&p);
   if(object==NONE){platform_log("[crowd_bench] FAILED spawn index=%d",i);crowd_bench.enabled=0;crowd_bench.ready=0;return;}
   crowd_bench.objects[crowd_bench.spawned++]=object;
   if(getenv("HALO_PRIVATE_CROWD_DEAD") && !strcmp(getenv("HALO_PRIVATE_CROWD_DEAD"),"1"))damage_kill_object_for_player(object,player);
  }
  platform_log("[crowd_bench] spawned=%d center=(%.2f,%.2f,%.2f)",crowd_bench.spawned,crowd_bench.center.x,crowd_bench.center.y,crowd_bench.center.z);
 }
}

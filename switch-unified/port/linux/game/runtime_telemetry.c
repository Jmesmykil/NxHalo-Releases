/* Native performance and match records for actual play, independent of network_test. */
#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "memory/data.h"
#include "scenario/scenario.h"
#include "tag_files/tag_files.h"
#include "halo_network_profile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

boolean config_boolean(char const *name);
void platform_log(char const *format, ...);
void network_distributed_statistics(long *, long *, long *);
void network_damage_statistics(long *, long *, long *, long *);

enum { SAMPLE_LIMIT = 2048 };
static struct {
    int checked, enabled, count;
    unsigned long frames;
    double last_present, began, wall_ms[SAMPLE_LIMIT], present_ms[SAMPLE_LIMIT];
    double sort[SAMPLE_LIMIT];
    double last_match_sample;
    long last_tick;
    unsigned long match_id;
    int active, ended;
} telemetry;

int halo_telemetry_enabled(void)
{
    if (!telemetry.checked) {
        telemetry.checked = 1;
        telemetry.enabled = config_boolean("debug.performance");
        if (telemetry.enabled)
            platform_log("telemetry: enabled unix=%ld rolling_samples=%d report_seconds=10; frame wall includes pacing; present wall includes swap; GPU busy is device utilization, not GPU frame time", (long)time(NULL), SAMPLE_LIMIT);
    }
    return telemetry.enabled;
}

double halo_telemetry_time_ms(void);

static int compare_double(void const *a, void const *b)
{
    double x=*(double const *)a, y=*(double const *)b;
    return x < y ? -1 : x > y;
}

static double percentile(double const *samples, int count, int percent)
{
    int index;
    if (!count) return 0;
    memcpy(telemetry.sort, samples, (size_t)count*sizeof(double));
    qsort(telemetry.sort, (size_t)count, sizeof(double), compare_double);
    index=((count-1)*percent+99)/100;
    return telemetry.sort[index];
}

void halo_telemetry_report_resources(double seconds);

static void log_roster(char const *event)
{
    struct data_iterator iterator;
    struct player_datum *player;
    long best=-2147483647L, best_index=NONE;
    int winners=0;
    boolean teams=game_engine_has_teams();
    if (!player_data || !game_engine) return;
    data_iterator_new(&iterator,player_data);
    while((player=(struct player_datum *)data_iterator_next(&iterator))!=NULL) {
        char name[13]; int j;
        long outcome=!strcmp(event,"final")?game_engine_did_player_win(iterator.datum_index):-2;
        char const *result=outcome==-2?"pending":outcome==NONE?"tie":outcome?"win":"loss";
        long score=game_engine->get_player_score?game_engine->get_player_score(iterator.datum_index,_get_score_individual):0;
        for(j=0;j<12 && player->name[j];j++)name[j]=player->name[j]>=32 && player->name[j]<127?(char)player->name[j]:'?';
        name[j]=0;
        platform_log("match_player: unix=%ld match=%lu event=%s tick=%ld slot=%ld name=\"%s\" team=%d local=%d departed=%d result=%s score=%ld kills=%d deaths=%d assists=%d betrayals=%d suicides=%d shots_fired=%ld shots_hit=%ld",
            (long)time(NULL),telemetry.match_id,event,game_time_get(),(long)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index)+1,name,
            player->team_index,player->local_player_index!=NONE,player->quit_out_of_game,result,score,player->statistics.kills[0],player->statistics.deaths,
            player->statistics.assists[0],player->statistics.friendly_fire_kills,player->statistics.suicides,player->statistics.shots_fired,player->statistics.shots_hit);
        if (!teams && !player->quit_out_of_game) {
            if(score>best) {best=score;best_index=DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index)+1;winners=1;}
            else if(score==best)winners++;
        }
    }
    if (teams) {
        /* Halo team engine supports red/blue; retain scores even where special end rules differ. */
        long red=game_engine_get_team_score(0),blue=game_engine_get_team_score(1);
        platform_log("match_teams: unix=%ld match=%lu event=%s red=%ld blue=%ld score_leader=%s",(long)time(NULL),telemetry.match_id,event,red,blue,red==blue?"tie":red>blue?"red":"blue");
    } else {
        platform_log("match_leader: unix=%ld match=%lu event=%s slot=%ld score=%ld tied=%d",(long)time(NULL),telemetry.match_id,event,best_index,best,winners>1);
    }
}

static void match_sample(double now)
{
    long tick;
    int active=game_in_progress() && game_engine_running() && player_data && players_in_game()>0;
    if (!active) {
        if(telemetry.active && !telemetry.ended)
            platform_log("match_end: unix=%ld match=%lu reason=left_or_unloaded result=unconfirmed",(long)time(NULL),telemetry.match_id);
        telemetry.active=0;return;
    }
    tick=game_time_get();
    if(!telemetry.active || tick<telemetry.last_tick) {
        struct game_variant *variant=game_engine_get_variant();
        char mode[33];int j;
        telemetry.match_id++;telemetry.ended=0;telemetry.active=1;
        for(j=0;j<32 && variant->human_readable_game_description[j];j++)
            mode[j]=variant->human_readable_game_description[j]>=32 && variant->human_readable_game_description[j]<127?(char)variant->human_readable_game_description[j]:'?';
        mode[j]=0;
        platform_log("match_begin: unix=%ld match=%lu map=\"%s\" mode=\"%s\" engine=%d teams=%d",(long)time(NULL),telemetry.match_id,
            global_scenario_index!=NONE?tag_get_name(global_scenario_index):"unknown",mode,variant->game_engine_index,game_engine_has_teams());
        log_roster("begin");telemetry.last_match_sample=now;
    }
    telemetry.last_tick=tick;
    if(!telemetry.ended && !game_engine_can_score()) {
        telemetry.ended=1;
        platform_log("match_end: unix=%ld match=%lu reason=engine_postgame result=recorded tick=%ld",(long)time(NULL),telemetry.match_id,tick);
        log_roster("final");
    } else if(!telemetry.ended && now-telemetry.last_match_sample>=5000.0) {
        log_roster("sample");telemetry.last_match_sample=now;
    }
}

void halo_telemetry_present(double entered_ms, double completed_ms, unsigned long frame)
{
    double now,seconds;
    int index,n;
    long sent,received,corrections,hits,dealt,rejected,replayed;
    if(!halo_telemetry_enabled())return;
    now=halo_telemetry_time_ms();
    if(!telemetry.last_present) {
        telemetry.last_present=now;telemetry.began=now;
        halo_telemetry_report_resources(0.0);
        return;
    }
    index=(int)(telemetry.frames%SAMPLE_LIMIT);
    telemetry.wall_ms[index]=now-telemetry.last_present;
    telemetry.present_ms[index]=completed_ms>=entered_ms?completed_ms-entered_ms:0;
    telemetry.last_present=now;telemetry.frames++;
    if(telemetry.count<SAMPLE_LIMIT)telemetry.count++;
    match_sample(now);
    if(now-telemetry.began<10000.0)return;
    seconds=(now-telemetry.began)/1000.0;n=telemetry.count;
    platform_log("perf_frame: unix=%ld frame=%lu seconds=%.3f frames=%lu samples=%d fps=%.2f wall_ms_p50=%.3f wall_ms_p95=%.3f wall_ms_p99=%.3f present_ms_p50=%.3f present_ms_p95=%.3f present_ms_p99=%.3f",
        (long)time(NULL),frame,seconds,telemetry.frames,n,telemetry.frames/seconds,
        percentile(telemetry.wall_ms,n,50),percentile(telemetry.wall_ms,n,95),percentile(telemetry.wall_ms,n,99),
        percentile(telemetry.present_ms,n,50),percentile(telemetry.present_ms,n,95),percentile(telemetry.present_ms,n,99));
    halo_telemetry_report_resources(seconds);
    network_distributed_statistics(&sent,&received,&corrections);
    network_damage_statistics(&hits,&dealt,&rejected,&replayed);
    platform_log("perf_network: unix=%ld match=%lu tick=%ld sent=%ld received=%ld corrections=%ld hit_reports=%ld dealt=%ld rejected=%ld replayed=%ld",
        (long)time(NULL),telemetry.match_id,telemetry.active?game_time_get():0,sent,received,corrections,hits,dealt,rejected,replayed);
    telemetry.began=now;telemetry.frames=0;telemetry.count=0;
}

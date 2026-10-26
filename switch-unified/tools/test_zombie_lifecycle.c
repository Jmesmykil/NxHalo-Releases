/* Focused actual-runtime lifecycle test.
   run_test.py in content-tools/zombies-solo-lifecycle-20261009 extracts the named
   production functions into zombie_lifecycle_runtime.inc, then builds this file
   with that scratch include path. Roster/network/object dependencies are mocks;
   role initialization, prespawn, postspawn, death and identity policy are actual
   production code, not duplicated test implementations. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int boolean;
typedef float real;
#define TRUE 1
#define FALSE 0
#define NONE (-1)
#define HALO_PORT_MAXIMUM_NETWORK_PLAYERS 4
#define DATUM_INDEX_TO_ABSOLUTE_INDEX(x) ((unsigned long)(x) & 0xffffUL)
enum { MATCH_RULES_PRESET_STANDARD, MATCH_RULES_PRESET_FACTION, MATCH_RULES_PRESET_ZOMBIES,
    MATCH_RULES_PRESET_SWAT, MATCH_RULES_PRESET_TOWER_OF_POWER, MATCH_RULES_PRESET_NATIVE_RACE,
    MATCH_RULES_PRESET_DODGEBALL, MATCH_RULES_PRESET_GUN_GAME };
enum { _game_connection_network_client = 2 };
enum { _unit_grenade_human_fragmentation = 0, _unit_grenade_covenant_plasma = 1 };
struct vector { real i,j,k; };
struct player_datum { long unit_index; short team_index; };
struct unit_datum { struct { int grenade_counts[4]; int desired_grenade_index; } unit; };
static boolean zombies_initialized, zombies_ready;
static boolean zombies_infected[4], zombies_initial_roster[4];
static long zombies_tracked_player[4];
static long lunge_tracked_player[4],lunge_last_tick[4],lunge_started_tick[4];
static boolean lunge_button_held[4];
static struct vector lunge_direction[4],zero_vector;
static struct vector *global_zero_vector3d=&zero_vector;
static struct player_datum players[4];
static struct unit_datum units[4];
static long roster_mock[4],roster_size;
static int preset=MATCH_RULES_PRESET_ZOMBIES,connection,prunes[4],grants[4],checks;
static unsigned long seed_mock;
static char message[256];
#define CHECK(x) do { checks++;if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);} }while(0)
static int game_connection(void) { return connection; }
static int network_game_distributed_client(void) { return 0; }
static short match_rules_preset_get(void) { return preset; }
static void set_message(char const *text) { snprintf(message,sizeof(message),"%s",text); }
static long active_roster(long *roster,short capacity)
{ long n=roster_size<capacity?roster_size:capacity;memcpy(roster,roster_mock,n*sizeof(*roster));return n; }
static struct player_datum *player_get(long datum) { return &players[DATUM_INDEX_TO_ABSOLUTE_INDEX(datum)]; }
static struct unit_datum *unit_try_and_get(long index) { return index>=0&&index<4?&units[index]:NULL; }
static unsigned long mix_seed(unsigned long seed) { (void)seed;return seed_mock; }
static unsigned long match_seed(void) { return 0; }
static void update_host_team(struct player_datum *player,long datum,short team)
{ (void)datum;player->team_index=team; }
static long match_rules_zombie_melee_weapon_definition(void) { return 17; }
static boolean nxhalo_give_zombie_melee_weapon(long unit) { grants[unit]++;return TRUE; }
static void match_rules_host_prune_infected_inventory(long unit,long melee) { CHECK(melee==17);prunes[unit]++; }
static boolean gun_game_equip_player_stage(long datum) { (void)datum;return TRUE; }

#include "zombie_lifecycle_runtime.inc"

static void reset_round(void)
{
    match_rules_zombie_state_reset(&zombies_initialized,&zombies_ready,zombies_initial_roster,
        zombies_infected,zombies_tracked_player,4);
    memset(prunes,0,sizeof(prunes));memset(grants,0,sizeof(grants));
    memset(units,0,sizeof(units));
    for(int i=0;i<4;i++){players[i].unit_index=NONE;players[i].team_index=0;}
    roster_size=0;connection=0;preset=MATCH_RULES_PRESET_ZOMBIES;seed_mock=0;
}
int main(void)
{
    long human=0x10000,infected=0x10001,late=0x10002,reused=0x20000;
    reset_round();
    roster_mock[roster_size++]=human;
    match_rules_host_prespawn_player(human);
    CHECK(!zombies_initialized&&!zombies_ready);CHECK(players[0].team_index==0);
    players[0].unit_index=0;
    match_rules_host_postspawn_player(human);
    CHECK(!zombies_infected[0]);CHECK(zombies_tracked_player[0]==NONE);
    CHECK(grants[0]==0&&prunes[0]==0);

    /* Second joins after solo unit exists: only its unspawned datum is eligible. */
    roster_mock[roster_size++]=infected;
    match_rules_host_prespawn_player(infected);
    CHECK(zombies_initialized&&zombies_ready);CHECK(players[0].team_index==0&&players[1].team_index==1);
    CHECK(!zombies_infected[0]&&zombies_infected[1]);
    CHECK(zombies_initial_roster[0]&&zombies_initial_roster[1]);
    CHECK(zombies_tracked_player[0]==human&&zombies_tracked_player[1]==infected);
    players[1].unit_index=1;
    match_rules_host_postspawn_player(infected);
    CHECK(grants[1]==1&&prunes[1]==1);
    match_rules_host_postspawn_player(human);
    CHECK(grants[0]==0&&prunes[0]==0);CHECK(!zombies_player_is_infected(human));

    /* Late join is infected, not a newly minted survivor. */
    roster_mock[roster_size++]=late;
    match_rules_host_prespawn_player(late);players[2].unit_index=2;
    match_rules_host_postspawn_player(late);
    CHECK(players[2].team_index==1&&zombies_infected[2]);CHECK(!zombies_initial_roster[2]);
    CHECK(prunes[2]==1&&grants[2]==1);

    /* Conversion only after death, delivered on the next prespawn. */
    match_rules_host_player_killed(human);
    CHECK(zombies_infected[0]);CHECK(players[0].team_index==0);
    players[0].unit_index=NONE;match_rules_host_prespawn_player(human);
    CHECK(players[0].team_index==1);

    /* Recycled slot identity is late infected, never inherits old human role. */
    zombies_infected[0]=FALSE;players[0].unit_index=NONE;roster_mock[0]=reused;
    match_rules_host_prespawn_player(reused);
    CHECK(zombies_tracked_player[0]==reused&&zombies_infected[0]&&!zombies_initial_roster[0]);

    /* Restart resets roles; preexisting infected bits cannot survive initial assignment. */
    reset_round();roster_mock[0]=human;roster_mock[1]=infected;roster_size=2;seed_mock=1;
    CHECK(!zombies_initialized&&!zombies_ready);
    CHECK(zombies_tracked_player[0]==NONE&&zombies_tracked_player[1]==NONE);
    zombies_infected[0]=zombies_infected[1]=TRUE; /* explicitly exercise role overwrite */
    match_rules_host_prespawn_player(human);
    CHECK(!zombies_infected[0]&&zombies_infected[1]);
    CHECK(players[0].team_index==0&&players[1].team_index==1);
    CHECK(zombies_initial_roster[0]&&zombies_initial_roster[1]);

    /* No role mutation on an unchanged client, nor when Standard is active. */
    reset_round();roster_mock[0]=human;roster_size=1;connection=_game_connection_network_client;
    match_rules_host_prespawn_player(human);players[0].unit_index=0;
    match_rules_host_postspawn_player(human);
    CHECK(!zombies_ready&&zombies_tracked_player[0]==NONE);CHECK(!prunes[0]&&!grants[0]);
    connection=0;preset=MATCH_RULES_PRESET_STANDARD;
    match_rules_host_prespawn_player(human);match_rules_host_postspawn_player(human);
    CHECK(!zombies_ready&&zombies_tracked_player[0]==NONE);CHECK(!prunes[0]&&!grants[0]);
    printf("PASS %d lifecycle checks (actual initializer/prespawn/postspawn/death policy; mocked runtime dependencies)\n",checks);
    return 0;
}

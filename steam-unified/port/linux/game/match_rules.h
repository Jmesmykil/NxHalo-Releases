#ifndef MATCH_RULES_H
#define MATCH_RULES_H
#include "cseries.h"
struct game_variant;
struct game_variant_options;
struct unit_control_data;
enum match_rules_preset {
 MATCH_RULES_PRESET_STANDARD=0, MATCH_RULES_PRESET_FACTION, MATCH_RULES_PRESET_SWAT,
 MATCH_RULES_PRESET_TOWER_OF_POWER, MATCH_RULES_PRESET_DODGEBALL,
 MATCH_RULES_PRESET_ZOMBIES, MATCH_RULES_PRESET_RACING, MATCH_RULES_PRESET_GUN_GAME, MATCH_RULES_PRESET_COUNT
};
enum match_rules_matchup {
 MATCH_RULES_MATCHUP_COVENANT_USMC=0, MATCH_RULES_MATCHUP_USMC_FLOOD,
 MATCH_RULES_MATCHUP_FLOOD_COVENANT, MATCH_RULES_MATCHUP_COUNT
};
short match_rules_preset_count(void);
short match_rules_matchup_count(void);
char const *match_rules_preset_name(short preset);
char const *match_rules_matchup_name(short matchup);
boolean match_rules_preset_supported(short preset);
boolean match_rules_preset_available(short preset, char *reason, int reason_size);
boolean match_rules_validate_loaded_map(void);
void match_rules_update_map_notice(void);
short match_rules_preset_get(void);
boolean match_rules_preset_set(short preset);
short match_rules_matchup_get(void);
boolean match_rules_matchup_set(short matchup);
char const *match_rules_status(void);
void match_rules_reset(void);
boolean match_rules_apply_variant_preset(short preset, struct game_variant *variant, struct game_variant_options *options);
long match_rules_host_spawn_definition(long player_index, long fallback_definition);
void match_rules_host_prespawn_player(long player_index);
void match_rules_host_player_killed(long player_index);
void match_rules_host_player_scored_kill(long killer_player_index, long dead_player_index, boolean credited_player_kill);
short match_rules_host_required_spawn_weapons(long player_index, long *definitions, short capacity);
void match_rules_host_note_loadout_fallback(void);
void match_rules_host_postspawn_player(long player_index);
void match_rules_host_apply_infected_lunge(long player_index, struct unit_control_data *control, boolean equipment_action_consumed);
boolean match_rules_player_lunge_active(long player_index);
boolean match_rules_player_melee_only(long player_index);
boolean match_rules_weapon_allowed_for_unit(long unit_index, long weapon_definition_index);
boolean match_rules_grenade_pickup_allowed(long unit_index);
/* -1: use ordinary variant settings; 0: excluded; 1: authored turret. */
short match_rules_vehicle_policy(long definition_index);
void match_rules_filter_player_control(long player_index, struct unit_control_data *control);
boolean match_rules_should_end_game(void);
#endif

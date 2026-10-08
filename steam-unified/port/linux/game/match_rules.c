/*
 * Host-authoritative Halo match presets. All faction and infection bipeds are
 * resolved from tags already loaded by the selected map. This adds no wire
 * fields and does not deliver assets.
 */
#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/game_engine_slayer.h"
#include "game/game_globals.h"
#include "game/players.h"
#include "cutscene/cinematics.h"
#include "interface/hud_messaging.h"
#include "interface/player_ui.h"
#include "math/real_math.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "networking/network_server_manager_internal.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "units/units.h"
#include "units/vehicles.h"
#include "units/bipeds.h"
#include "units/unit_definitions.h"
#include "units/unit_control_data.h"
#include "items/weapon_definitions.h"
#include "tag_files/tag_groups.h"
#include "physics/collisions.h"
#include "physics/physics_definitions.h"
#include "objects/objects.h"
#include "../src/port_config.h"
#include "match_rules.h"
#include "match_rules_melee.h"
#include "gun_game_progression.h"
#include "network_distributed.h"
#include "nxhalo_custom_content.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void platform_log(char const *format, ...);

enum { _faction_none, _faction_covenant, _faction_usmc, _faction_flood };
static char match_rules_message[192];
static unsigned long cached_local_seed;
static boolean local_seed_valid;
static boolean race_warning_pending;
static short race_warning_last_wait_reason = -1;
static boolean zombies_initialized;
static boolean zombies_ready;
static boolean zombies_infected[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
static boolean zombies_initial_roster[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
static long zombies_tracked_player[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
static long lunge_tracked_player[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
static boolean lunge_button_held[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
static long lunge_last_tick[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
static long lunge_started_tick[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
static real_vector3d lunge_direction[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
static struct gun_game_progression gun_game_players[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
static boolean match_rules_variant_is_gun_game(void);
static short const gun_game_weapon_list_indices[GUN_GAME_WEAPON_STAGE_COUNT] = { 4, 0, 6, 3, 8, 9, 7 }; /* pistol, AR, plasma rifle, needler, shotgun, sniper, rocket */

static char const * const match_preset_names[MATCH_RULES_PRESET_COUNT] = {
    "Standard", "Faction match", "SWAT", "Tower of Power",
    "Grenade Dodgeball", "Zombies", "Native Race", "Gun Game"
};
static char const * const matchup_names[MATCH_RULES_MATCHUP_COUNT] = {
    "Covenant vs USMC", "USMC vs Flood", "Flood vs Covenant"
};

static void set_message(char const *message)
{
    snprintf(match_rules_message, sizeof(match_rules_message), "%s", message ? message : "");
}

short match_rules_preset_count(void) { return MATCH_RULES_PRESET_COUNT; }
short match_rules_matchup_count(void) { return MATCH_RULES_MATCHUP_COUNT; }

char const *match_rules_preset_name(short preset)
{
    return preset >= 0 && preset < MATCH_RULES_PRESET_COUNT ? match_preset_names[preset] : "Unknown";
}

char const *match_rules_matchup_name(short matchup)
{
    return matchup >= 0 && matchup < MATCH_RULES_MATCHUP_COUNT ? matchup_names[matchup] : "Unknown";
}

boolean match_rules_preset_supported(short preset)
{
    return preset >= MATCH_RULES_PRESET_STANDARD && preset < MATCH_RULES_PRESET_COUNT;
}

short match_rules_preset_get(void)
{
    long preset = config_integer("mods.match_preset");
    return preset >= 0 && preset < MATCH_RULES_PRESET_COUNT ? (short)preset : MATCH_RULES_PRESET_STANDARD;
}

boolean match_rules_preset_set(short preset)
{
    char value[16];
    if (!match_rules_preset_supported(preset))
    {
        set_message("Unknown preset.");
        return FALSE;
    }
    if (game_connection() == _game_connection_network_client)
    {
        set_message("Only the host can change the match preset.");
        return FALSE;
    }
    csprintf(value, "%d", preset);
    set_message("");
    return config_write("mods.match_preset", value);
}

short match_rules_matchup_get(void)
{
    long matchup = config_integer("mods.faction_matchup");
    return matchup >= 0 && matchup < MATCH_RULES_MATCHUP_COUNT ? (short)matchup : MATCH_RULES_MATCHUP_COVENANT_USMC;
}

boolean match_rules_matchup_set(short matchup)
{
    char value[16];
    if (matchup < 0 || matchup >= MATCH_RULES_MATCHUP_COUNT)
        return FALSE;
    csprintf(value, "%d", matchup);
    return config_write("mods.faction_matchup", value);
}

char const *match_rules_status(void) { return match_rules_message; }

void match_rules_reset(void)
{
    set_message("");
    race_warning_pending = FALSE;
    race_warning_last_wait_reason = -1;
    local_seed_valid = FALSE;
    cached_local_seed = 0;
    zombies_initialized = FALSE;
    zombies_ready = FALSE;
    short index;
    csmemset(zombies_infected, 0, sizeof(zombies_infected));
    csmemset(zombies_initial_roster, 0, sizeof(zombies_initial_roster));
    for (index = 0; index < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; index++)
    {
        zombies_tracked_player[index] = NONE;
        gun_game_progression_clear(&gun_game_players[index]);
        lunge_tracked_player[index] = NONE;
        lunge_button_held[index] = FALSE;
        lunge_last_tick[index] = NONE;
        lunge_started_tick[index] = NONE;
        lunge_direction[index] = *global_zero_vector3d;
    }
}

static short faction_for_team(short matchup, long team)
{
    static short const factions[MATCH_RULES_MATCHUP_COUNT][2] = {
        { NXHALO_FACTION_COVENANT, NXHALO_FACTION_USMC },
        { NXHALO_FACTION_USMC, NXHALO_FACTION_FLOOD },
        { NXHALO_FACTION_FLOOD, NXHALO_FACTION_COVENANT }
    };
    if (matchup < 0 || matchup >= MATCH_RULES_MATCHUP_COUNT || team < 0 || team > 1)
        return _faction_none;
    return factions[matchup][team];
}

static unsigned long match_seed(void)
{
    if (network_game_is_active())
        return (unsigned long)network_game_get_random_seed();
    if (!local_seed_valid)
    {
        cached_local_seed = *get_global_random_seed_address();
        local_seed_valid = TRUE;
    }
    return cached_local_seed;
}

static unsigned long mix_seed(unsigned long value)
{
    value ^= value >> 16;
    value *= 0x7feb352dUL;
    value ^= value >> 15;
    value *= 0x846ca68bUL;
    value ^= value >> 16;
    return value;
}

static short faction_bipeds(short faction, long *definitions, short capacity)
{
    return nxhalo_character_faction_bipeds(faction, definitions, capacity);
}

static long gun_game_weapon_definition(int stage)
{
    if (stage < 0 || stage >= GUN_GAME_WEAPON_STAGE_COUNT)
        return NONE;
    return list_index_to_weapon_definition_index(gun_game_weapon_list_indices[stage]);
}

static int gun_game_bind_player(long player_index)
{
    short absolute;
    if (player_index == NONE)
        return -1;
    absolute = (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
    if (absolute < 0 || absolute >= HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
        return -1;
    gun_game_progression_bind(&gun_game_players[absolute], player_index);
    return absolute;
}

static long active_roster(long *roster, short capacity);

static short count_race_track_markers(void)
{
    struct scenario *scenario = global_scenario_try_and_get();
    long index;
    short count = 0;
    if (!scenario)
        return 0;
    for (index = 0; index < scenario->netgame_flags.count; index++)
    {
        struct scenario_netgame_flag *flag = TAG_BLOCK_GET_ELEMENT(
            &scenario->netgame_flags, index, struct scenario_netgame_flag);
        if (flag->type == _netgame_flag_race_track)
            count++;
    }
    return count;
}

boolean match_rules_preset_available(short preset, char *reason, int reason_size)
{
    long definitions[8];
    short first, second, matchup;
    long roster[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
    int available = TRUE;
    char const *message = "";
    if (!match_rules_preset_supported(preset))
        return FALSE;

    switch (preset)
    {
    case MATCH_RULES_PRESET_FACTION:
        matchup = match_rules_matchup_get();
        first = faction_bipeds(faction_for_team(matchup, 0), definitions, 8);
        second = faction_bipeds(faction_for_team(matchup, 1), definitions, 8);
        if (!first || !second)
        {
            available = FALSE;
            message = "Faction matches require biped assets already loaded by the selected map; assets are not auto-delivered.";
        }
        break;
    case MATCH_RULES_PRESET_TOWER_OF_POWER:
        if (list_index_to_weapon_definition_index(8) == NONE)
        {
            available = FALSE;
            message = "Tower of Power requires a shotgun tag loaded by the map and a map-authored mounted turret; nothing is generated.";
        }
        else
            message = "Tower of Power requires a loaded shotgun and map-authored mounted turret; turret placement is not generated.";
        break;
    case MATCH_RULES_PRESET_RACING:
        if (count_race_track_markers() < 2)
        {
            available = FALSE;
            message = "Native Race requires a map with at least two Race Track netgame markers; track geometry is not generated.";
        }
        break;
    case MATCH_RULES_PRESET_GUN_GAME:
        for (short stage = 0; stage < GUN_GAME_WEAPON_STAGE_COUNT; stage++)
        {
            if (gun_game_weapon_definition(stage) == NONE)
            {
                available = FALSE;
                message = "Gun Game requires all seven stock ladder weapons to be loaded by the selected map.";
                break;
            }
        }
        if (available)
            message = "Gun Game uses pistol, assault rifle, plasma rifle, needler, shotgun, sniper rifle, then rocket launcher.";
        break;
    case MATCH_RULES_PRESET_ZOMBIES:
        if (list_index_to_weapon_definition_index(8) == NONE)
        {
            available = FALSE;
            message = "Zombies requires a shotgun tag loaded by the map for survivors; infected use melee only.";
        }
        else if (players_in_game() < 2)
            message = "Zombies waits for two connected players. Survivors use shotguns; infected use melee only.";
        else if (!faction_bipeds(NXHALO_FACTION_FLOOD, definitions, 8))
            message = "No Flood models loaded: infected keep the normal body and melee weapon; survivors use shotguns.";
        else
            message = "Survivors use shotguns; infected use melee only and cannot throw grenades.";
        break;
    default:
        break;
    }
    if (reason && reason_size > 0)
        snprintf(reason, (size_t)reason_size, "%s", message);
    return available;
}

static void variant_template(struct game_variant *variant, char const *name, char const *label)
{
    struct game_variant base;
    short i;
    game_engine_get_variant_by_name(&base, name);
    variant->game_engine_index = base.game_engine_index;
    variant->game_engine_variant = base.game_engine_variant;
    for (i = 0; i < NUMBEROF(variant->human_readable_game_description) - 1 && label[i]; i++)
        variant->human_readable_game_description[i] = (wchar_t)label[i];
    variant->human_readable_game_description[i] = 0;
}

boolean match_rules_validate_loaded_map(void)
{
    char reason[160];
    struct game_variant *variant = game_engine_get_variant();
    boolean host_authority = game_connection() != _game_connection_network_client &&
        !network_game_distributed_client();
    short preset = match_rules_preset_get();

    race_warning_pending = FALSE;
    race_warning_last_wait_reason = -1;
    /* Validate the running variant: host setup can override the saved preset. */
    if (variant && variant->game_engine_index == game_engine_race)
    {
        if (count_race_track_markers() < 2)
        {
            if (host_authority)
            {
                set_message("Race needs a map with at least two Race Track checkpoints.");
                race_warning_pending = TRUE;
            }
            return FALSE;
        }
        if (host_authority)
            set_message("Native Race map requirements are satisfied.");
        return TRUE;
    }
    if (match_rules_variant_is_gun_game())
    {
        short stage;
        /* An edited profile may carry arbitrary Slayer options. The active
         * Gun Game variant always remains FFA and ends at its seven-kill ladder. */
        if (host_authority)
        {
            variant->universal_variant.teams = FALSE;
            variant->universal_variant.score_to_win = GUN_GAME_STAGE_COUNT;
            variant->game_engine_variant.slayer.kill_in_order = FALSE;
        }
        for (stage = 0; stage < GUN_GAME_WEAPON_STAGE_COUNT; stage++)
        {
            if (gun_game_weapon_definition(stage) == NONE)
            {
                if (host_authority)
                    set_message("Gun Game requires all seven stock ladder weapon tags in the loaded map.");
                return FALSE;
            }
        }
        if (host_authority)
            set_message("Gun Game map requirements are satisfied.");
        return TRUE;
    }
    if (preset == MATCH_RULES_PRESET_STANDARD)
    {
        set_message("Standard preset selected.");
        return TRUE;
    }
    if (!match_rules_preset_available(preset, reason, sizeof(reason)))
    {
        set_message(reason[0] ? reason : "The selected map does not meet this preset's requirements.");
        return FALSE;
    }
    if (reason[0])
        set_message(reason);
    else
        set_message("Selected map loaded; preset requirements are satisfied.");
    return TRUE;
}

/* Publish only when the local HUD can accept and render this transient notice. */
void match_rules_update_map_notice(void)
{
    char const *trace = getenv("HALO_NETWORK_TEST_TRACE");
    boolean trace_enabled = trace && !strcmp(trace, "1");
    short local;
    short wait_reason = -1;
    short chosen_local = NONE;
    long chosen_player = NONE;
    const char *wait_name = NULL;

    if (!race_warning_pending)
        return;
    if (game_time_get() < TICKS_PER_SECOND)
        return;
    if (cinematic_in_progress())
    {
        wait_reason = 1;
        wait_name = "cinematic";
    }
    else
    {
        for (local = local_player_get_next(NONE); local != NONE;
            local = local_player_get_next(local))
        {
            long player = local_player_get_player_index(local);
            if (player == NONE)
                continue;
            if (game_engine_hud_draw_messages(player))
            {
                chosen_local = local;
                chosen_player = player;
                break;
            }
            wait_reason = 2;
            wait_name = "hud-masked";
        }
        if (chosen_local == NONE && wait_reason == -1)
        {
            wait_reason = 0;
            wait_name = "no-local-player";
        }
    }

    if (chosen_local == NONE)
    {
        if (trace_enabled && race_warning_last_wait_reason != wait_reason)
            platform_log("network trace: race_hud_notice state=waiting reason=%s tick=%ld",
                wait_name ? wait_name : "unknown", game_time_get());
        race_warning_last_wait_reason = wait_reason;
        return;
    }

    hud_print_message(chosen_local,
        L"Race needs 2 checkpoints. Return to lobby and change map.");
    if (trace_enabled)
        platform_log("network trace: race_hud_notice state=enqueued local=%d player=%ld tick=%ld",
            chosen_local, chosen_player, game_time_get());
    race_warning_pending = FALSE;
    race_warning_last_wait_reason = -1;
}

static void match_rules_clear_special_label(struct game_variant *variant)
{
    static char const *const base_names[] = { "", "ctf", "slayer", "oddball", "king", "race" };
    wchar_t const *label;
    struct game_variant base;
    if (!variant || variant->game_engine_index <= game_engine_none ||
        variant->game_engine_index > game_engine_race)
        return;
    label = variant->human_readable_game_description;
    if (ustrncmp(label, L"Gun Game", 8) && ustrncmp(label, L"Zombies", 7) &&
        ustrncmp(label, L"Infection", 9) && ustrncmp(label, L"Tower Power", 11) &&
        ustrncmp(label, L"SWAT", 4) && ustrncmp(label, L"Dodgeball", 9) &&
        ustrncmp(label, L"Native Race", 11) && ustrncmp(label, L"COV vs", 6) &&
        ustrncmp(label, L"USMC vs", 7) && ustrncmp(label, L"Flood vs", 8))
        return;
    game_engine_get_variant_by_name(&base, base_names[variant->game_engine_index]);
    csmemcpy(variant->human_readable_game_description,
        base.human_readable_game_description, sizeof(variant->human_readable_game_description));
}

boolean match_rules_apply_variant_preset(short preset, struct game_variant *variant,
    struct game_variant_options *options)
{
    short team, vehicle;
    if (!variant || !options || !match_rules_preset_supported(preset))
        return FALSE;

    switch (preset)
    {
    case MATCH_RULES_PRESET_STANDARD:
        match_rules_clear_special_label(variant);
        set_message("Standard preset applied.");
        return TRUE;
    case MATCH_RULES_PRESET_FACTION:
        variant->universal_variant.teams = TRUE;
        {
            static char const * const labels[] = { "COV vs USMC", "USMC vs Flood", "Flood vs Cov" };
            short matchup = match_rules_matchup_get();
            short i;
            char const *label = labels[matchup];
            for (i = 0; i < NUMBEROF(variant->human_readable_game_description) - 1 && label[i]; i++)
                variant->human_readable_game_description[i] = (wchar_t)label[i];
            variant->human_readable_game_description[i] = 0;
        }
        set_message("Faction template applied; faction assets will be checked after the selected map loads.");
        return TRUE;
    case MATCH_RULES_PRESET_SWAT:
        variant_template(variant, "team_slayer", "SWAT");
        variant->universal_variant.teams = TRUE;
        SET_FLAG(variant->universal_variant.flags, _game_variant_no_shields_bit, TRUE);
        variant->universal_variant.vehicle_set = 1; /* _game_engine_vehicles_none */
        options->loadout = _loadout_custom;
        options->primary_weapon = _loadout_weapon_pistol;
        options->secondary_weapon = _loadout_weapon_none;
        options->no_map_weapons = TRUE;
        break;
    case MATCH_RULES_PRESET_TOWER_OF_POWER:
        variant_template(variant, "team_slayer", "Tower Power");
        variant->universal_variant.vehicle_set = 1;
        variant->universal_variant.teams = TRUE;
        SET_FLAG(variant->universal_variant.flags, _game_variant_no_shields_bit, TRUE);
        variant->universal_variant.goal_radar = 2; /* _radar_none */
        variant->universal_variant.weapon_set = 10; /* _game_engine_weapons_no_grenades */
        options->radar_players = 2; /* _radar_players_none */
        options->loadout = _loadout_custom;
        options->primary_weapon = _loadout_weapon_shotgun;
        options->secondary_weapon = _loadout_weapon_none;
        options->no_map_weapons = TRUE;
        set_message("Tower of Power: shotgun only, no shields or radar. Map must provide the mounted turret; turret placement is not generated.");
        break;
    case MATCH_RULES_PRESET_DODGEBALL:
        variant_template(variant, "team_slayer", "Dodgeball");
        variant->universal_variant.teams = TRUE;
        SET_FLAG(variant->universal_variant.flags, _game_variant_no_shields_bit, TRUE);
        SET_FLAG(variant->universal_variant.flags, _game_variant_infinite_grenades_bit, TRUE);
        variant->universal_variant.vehicle_set = 1;
        options->loadout = _loadout_custom;
        options->primary_weapon = _loadout_weapon_none;
        options->secondary_weapon = _loadout_weapon_none;
        options->no_map_weapons = TRUE;
        break;
    case MATCH_RULES_PRESET_ZOMBIES:
        variant_template(variant, "team_slayer", "Zombies");
        variant->universal_variant.vehicle_set = 1;
        variant->universal_variant.teams = TRUE;
        variant->universal_variant.weapon_set = 10; /* _game_engine_weapons_no_grenades */
        options->loadout = _loadout_custom;
        options->primary_weapon = _loadout_weapon_shotgun;
        options->secondary_weapon = _loadout_weapon_none;
        options->no_map_weapons = TRUE;
        options->auto_team_balance = FALSE;
        break;
    case MATCH_RULES_PRESET_RACING:
        variant_template(variant, "team_race", "Native Race");
        variant->universal_variant.teams = TRUE;
        break;
    case MATCH_RULES_PRESET_GUN_GAME:
        variant_template(variant, "slayer", "Gun Game");
        variant->universal_variant.teams = FALSE;
        variant->universal_variant.score_to_win = GUN_GAME_STAGE_COUNT;
        variant->game_engine_variant.slayer.kill_in_order = FALSE;
        variant->universal_variant.weapon_set = 10; /* no grenades */
        variant->universal_variant.vehicle_set = 1; /* none */
        options->loadout = _loadout_custom;
        options->primary_weapon = _loadout_weapon_none;
        options->secondary_weapon = _loadout_weapon_none;
        options->no_map_weapons = TRUE;
        options->auto_team_balance = FALSE;
        for (team = 0; team < 2; team++)
        {
            options->vehicle_set[team] = 1;
            for (vehicle = 0; vehicle < NUMBER_OF_VARIANT_VEHICLES; vehicle++)
                options->vehicle_counts[team][vehicle] = 0;
        }
        set_message("Gun Game: earn one credited enemy kill per stage; deaths keep your stage. The next credited kill after reaching rockets wins.");
        return TRUE;
    default:
        return FALSE;
    }

    if (preset == MATCH_RULES_PRESET_SWAT || preset == MATCH_RULES_PRESET_DODGEBALL ||
        preset == MATCH_RULES_PRESET_TOWER_OF_POWER || preset == MATCH_RULES_PRESET_ZOMBIES)
    {
        for (team = 0; team < 2; team++)
        {
            options->vehicle_set[team] = 1;
            for (vehicle = 0; vehicle < NUMBER_OF_VARIANT_VEHICLES; vehicle++)
                options->vehicle_counts[team][vehicle] = 0;
        }
    }
    set_message(preset == MATCH_RULES_PRESET_TOWER_OF_POWER ?
        "Tower of Power: shotguns and mounted turret only; no shields, radar or grenades. Choose a map with a tower turret." :
        "Rules applied. Map compatibility is checked when loading.");
    return TRUE;
}

static void update_host_team(struct player_datum *player, long player_index, short team)
{
    struct network_game_server *server;
    struct network_game *game;
    long list_index;
    short absolute = (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
    if (absolute < 0 || absolute >= HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
        return;
    /* Mirror the stock auto-team-balance path. The next spawned unit carries
    owner_team_index in the existing reliable object-create delta; clients apply
    it in network_player_attach_unit, so no custom player wire field is needed. */
    player->team_index = team;
    player->network_player_data.team_index = (char)team;
    server = global_network_game_server_get();
    game = server ? network_game_server_get_game(server) : NULL;
    list_index = player->network_player_data.player_list_index;
    if (game && list_index >= 0 && list_index < HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
        game->players[list_index].team_index = (char)team;
}

static boolean zombies_player_is_infected(long player_index)
{
    short absolute;
    if (player_index == NONE)
        return FALSE;
    absolute = (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
    if (absolute < 0 || absolute >= HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
        return FALSE;
    if (zombies_tracked_player[absolute] != player_index)
    {
        /* A different datum identifier means this is a late joiner or a recycled slot. */
        zombies_tracked_player[absolute] = player_index;
        zombies_initial_roster[absolute] = FALSE;
        zombies_infected[absolute] = TRUE;
    }
    return zombies_infected[absolute];
}

static long active_roster(long *roster, short capacity)
{
    struct data_iterator iterator;
    struct player_datum *player;
    short count = 0;
    data_iterator_new(&iterator, player_data);
    while ((player = data_iterator_next(&iterator)) != NULL)
    {
        if (!player->quit_out_of_game && count < capacity)
            roster[count++] = iterator.datum_index;
    }
    return count;
}

static boolean zombies_initialize(void)
{
    long roster[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
    long spawn_candidates[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
    long roster_count, candidate_count = 0, index, chosen;
    short absolute;
    if (zombies_initialized)
        return zombies_ready;
    roster_count = active_roster(roster, HALO_PORT_MAXIMUM_NETWORK_PLAYERS);
    if (roster_count < 2)
    {
        zombies_ready = FALSE;
        set_message("Zombies waits for a second connected player; infection rules will initialize at the next prespawn.");
        return FALSE;
    }
    /* Prefer players whose first unit has not been created. A solo match may
    have spawned its first player before a second joins; that existing unit
    already went over the reliable object-create channel as a human. */
    for (index = 0; index < roster_count; index++)
    {
        struct player_datum *candidate = player_get(roster[index]);
        if (candidate->unit_index == NONE)
            spawn_candidates[candidate_count++] = roster[index];
    }
    if (!candidate_count)
    {
        set_message("Zombies waits for a player prespawn before assigning the initial infected.");
        return FALSE;
    }
    zombies_initialized = TRUE;
    chosen = (long)(mix_seed(match_seed() ^ 0x5a4f4d42UL) % (unsigned long)candidate_count);
    chosen = spawn_candidates[chosen];
    for (index = 0; index < roster_count; index++)
    {
        struct player_datum *player = player_get(roster[index]);
        absolute = (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(roster[index]);
        if (absolute >= 0 && absolute < HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
        {
            zombies_initial_roster[absolute] = TRUE;
            zombies_tracked_player[absolute] = roster[index];
        }
        update_host_team(player, roster[index], roster[index] == chosen ? 1 : 0);
        if (roster[index] == chosen && absolute >= 0 && absolute < HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
            zombies_infected[absolute] = TRUE;
    }
    zombies_ready = TRUE;
    set_message("Zombies: one infected chosen; infected use melee and cannot fire or throw grenades.");
    return TRUE;
}

void match_rules_host_prespawn_player(long player_index)
{
    struct player_datum *player;
    short absolute;
    if (match_rules_preset_get() != MATCH_RULES_PRESET_ZOMBIES ||
        game_connection() == _game_connection_network_client ||
        network_game_distributed_client() || player_index == NONE)
        return;
    if (!zombies_initialize())
        return;
    absolute = (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
    if (absolute < 0 || absolute >= HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
        return;
    lunge_tracked_player[absolute] = player_index;
    lunge_button_held[absolute] = FALSE;
    lunge_last_tick[absolute] = NONE;
    lunge_started_tick[absolute] = NONE;
    lunge_direction[absolute] = *global_zero_vector3d;
    player = player_get(player_index);
    update_host_team(player, player_index, zombies_player_is_infected(player_index) ? 1 : 0);
}

void match_rules_host_player_killed(long dead_player_index)
{
    short absolute;
    if (match_rules_preset_get() != MATCH_RULES_PRESET_ZOMBIES ||
        game_connection() == _game_connection_network_client ||
        network_game_distributed_client() || dead_player_index == NONE || !zombies_ready)
        return;
    absolute = (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(dead_player_index);
    if (absolute < 0 || absolute >= HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
        return;
    zombies_infected[absolute] = TRUE;
    /* Keep the eliminated player on the team replicated with their current
       unit through postgame. The infected team is assigned at the next
       prespawn, where the stock object-create delta delivers it to peers. */
    set_message("Zombies: this player is infected and will respawn with melee only.");
}

static boolean gun_game_equip_player_stage(long player_index)
{
    struct player_datum *player;
    struct unit_datum *unit;
    long unit_index;
    long definition_index;
    long weapon_index;
    long old_weapon_index;
    int absolute;
    if (player_index == NONE || game_connection() == _game_connection_network_client ||
        network_game_distributed_client())
        return FALSE;
    absolute = gun_game_bind_player(player_index);
    if (absolute < 0)
        return FALSE;
    player = player_get(player_index);
    unit_index = player->unit_index;
    unit = unit_try_and_get(unit_index);
    if (!unit)
        return TRUE; /* A credited projectile kill can outlive its owner's unit; respawn equips this stage. */
    unit->unit.grenade_counts[_unit_grenade_human_fragmentation] = 0;
    unit->unit.grenade_counts[_unit_grenade_covenant_plasma] = 0;
    unit->unit.desired_grenade_index = NONE;
    old_weapon_index = unit->unit.current_weapon_index >= 0 &&
        unit->unit.current_weapon_index < MAXIMUM_WEAPONS_PER_UNIT ?
        unit->unit.weapon_object_indices[unit->unit.current_weapon_index] : NONE;
    definition_index = gun_game_weapon_definition(gun_game_progression_stage(&gun_game_players[absolute]));
    if (definition_index == NONE)
    {
        set_message("Gun Game needs the stock weapon tags loaded by this map; switch maps to continue.");
        return FALSE;
    }
    {
        struct object_placement_data placement_data;
        object_placement_data_new(&placement_data, definition_index, NONE);
        weapon_index = object_new(&placement_data);
    }
    if (weapon_index == NONE)
    {
        set_message("Gun Game could not create the next stage weapon; progression was held.");
        return FALSE;
    }
    /* Replace only after the new object is valid and passes the normal inventory
       gate, so an allocation or compatibility failure keeps the current weapon. */
    if (!unit_add_weapon_to_inventory(unit_index, weapon_index, _unit_add_weapon_replace))
    {
        object_delete(weapon_index);
        set_message("Gun Game could not equip the next stage weapon; progression was held.");
        return FALSE;
    }
    if (old_weapon_index != NONE && old_weapon_index != weapon_index)
    {
        if (!unit_drop_current_weapon(unit_index, TRUE))
        {
            /* The next weapon is in a noncurrent slot. Remove it and keep the
               current stage weapon if the engine cannot put that weapon away. */
            unit_delete_all_weapons(unit_index);
            set_message("Gun Game could not switch weapons; the current stage is retained.");
            return FALSE;
        }
        if (object_try_and_get(old_weapon_index))
            object_delete(old_weapon_index);
    }
    return TRUE;
}

void match_rules_host_player_scored_kill(long killer_player_index, long dead_player_index,
    boolean credited_player_kill)
{
    int killer_absolute = -1;
    int dead_absolute = -1;
    boolean host = game_connection() != _game_connection_network_client &&
        !network_game_distributed_client();
    if (!match_rules_variant_is_gun_game() || !host)
        return;

    if (killer_player_index != NONE && player_try_and_get(killer_player_index))
    {
        killer_absolute = gun_game_bind_player(killer_player_index);
        if (credited_player_kill && killer_absolute >= 0)
        {
            int previous_stage = gun_game_progression_stage(&gun_game_players[killer_absolute]);
            gun_game_progression_credit_kill(&gun_game_players[killer_absolute], TRUE);
            if (!gun_game_progression_complete(&gun_game_players[killer_absolute]) &&
                !gun_game_equip_player_stage(killer_player_index))
                gun_game_players[killer_absolute].stage = previous_stage;
            if (gun_game_progression_stage(&gun_game_players[killer_absolute]) == GUN_GAME_STAGE_COUNT - 1 &&
                !gun_game_progression_complete(&gun_game_players[killer_absolute]))
                set_message("Gun Game: rocket stage reached; the next credited enemy kill wins.");
        }
    }

    if (dead_player_index != NONE && player_try_and_get(dead_player_index))
    {
        dead_absolute = gun_game_bind_player(dead_player_index);
        if (dead_absolute >= 0)
            game_engine_slayer_set_player_score(dead_player_index,
                gun_game_progression_score(&gun_game_players[dead_absolute]));
    }
    if (killer_player_index != NONE && killer_player_index != dead_player_index &&
        killer_absolute >= 0)
        game_engine_slayer_set_player_score(killer_player_index,
            gun_game_progression_score(&gun_game_players[killer_absolute]));
}

static long match_rules_zombie_melee_weapon_definition(void)
{
    static char const * const candidates[] = {
        "weapons\\nxhalo_zombie_sword\\nxhalo_zombie_sword",
        "weapons\\ball\\ball",
        "weapons\\flag\\flag",
        "weapons\\pistol\\pistol"
    };
    short index;
    for (index = 0; index < NUMBEROF(candidates); index++)
    {
        long definition = tag_loaded(WEAPON_DEFINITION_TAG, candidates[index]);
        if (definition != NONE && nxhalo_zombie_melee_weapon_allowed(definition))
            return definition;
    }
    return NONE;
}

short match_rules_host_required_spawn_weapons(long player_index, long *definitions, short capacity)
{
    short index, needed;
    long required;
    if (player_index == NONE || !definitions ||
        game_connection() == _game_connection_network_client || network_game_distributed_client())
        return 0;
    if (match_rules_variant_is_gun_game())
    {
        needed = GUN_GAME_WEAPON_STAGE_COUNT;
        if (capacity < needed)
            return 0;
        for (index = 0; index < needed; index++)
        {
            definitions[index] = gun_game_weapon_definition(index);
            if (definitions[index] == NONE)
                return 0;
        }
        return needed;
    }
    if (match_rules_preset_get() == MATCH_RULES_PRESET_ZOMBIES)
    {
        if (capacity < 1)
            return 0;
        if (zombies_player_is_infected(player_index))
            required = match_rules_zombie_melee_weapon_definition();
        else
            required = list_index_to_weapon_definition_index(8);
        if (required == NONE)
            return 0;
        definitions[0] = required;
        return 1;
    }
    if (match_rules_preset_get() == MATCH_RULES_PRESET_SWAT)
        required = list_index_to_weapon_definition_index(4);
    else if (match_rules_preset_get() == MATCH_RULES_PRESET_TOWER_OF_POWER)
        required = list_index_to_weapon_definition_index(8);
    else
        return 0;
    if (required == NONE || capacity < 1)
        return 0;
    definitions[0] = required;
    return 1;
}

void match_rules_host_note_loadout_fallback(void)
{
    if (game_connection() != _game_connection_network_client && !network_game_distributed_client())
        set_message("The selected character cannot use this mode's weapon loadout; a compatible map character was selected.");
}

long match_rules_host_spawn_definition(long player_index, long fallback_definition)
{
    struct player_datum *player;
    long candidates[8];
    short preset, matchup, faction, count;
    unsigned long seed, deaths;
    short absolute;

    if (game_connection() == _game_connection_network_client ||
        network_game_distributed_client() || !game_engine_running() ||
        !game_engine_has_teams() || player_index == NONE)
        return fallback_definition;

    preset = match_rules_preset_get();
    player = player_get(player_index);
    if (preset == MATCH_RULES_PRESET_ZOMBIES)
    {
        if (!zombies_initialize())
            return fallback_definition;
        absolute = (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
        if (absolute < 0 || absolute >= HALO_PORT_MAXIMUM_NETWORK_PLAYERS || !zombies_player_is_infected(player_index))
            return fallback_definition;
        faction = NXHALO_FACTION_FLOOD;
    }
    else if (preset == MATCH_RULES_PRESET_FACTION)
    {
        matchup = match_rules_matchup_get();
        faction = faction_for_team(matchup, player->team_index);
    }
    else
        return fallback_definition;

    count = faction_bipeds(faction, candidates, 8);
    if (count <= 0)
    {
        set_message(preset == MATCH_RULES_PRESET_ZOMBIES ?
            "This map has no Flood models; infected use the normal character." :
            "Faction matches require faction biped assets loaded by the selected map; assets are not auto-delivered, so the standard biped is retained.");
        return fallback_definition;
    }

    seed = match_seed();
    deaths = (unsigned long)(unsigned short)player->statistics.deaths;
    seed ^= ((unsigned long)(unsigned short)player_index + 1UL) * 0x9e3779b9UL;
    seed ^= (deaths + 1UL) * 0x85ebca6bUL;
    seed ^= ((unsigned long)faction + 1UL) * 0xc2b2ae35UL;
    return candidates[mix_seed(seed) % (unsigned long)count];
}

boolean match_rules_should_end_game(void)
{
    long roster[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
    long count, index, human_count = 0;
    short preset = match_rules_preset_get();
    if (preset == MATCH_RULES_PRESET_GUN_GAME && match_rules_variant_is_gun_game())
    {
        count = active_roster(roster, HALO_PORT_MAXIMUM_NETWORK_PLAYERS);
        for (index = 0; index < count; index++)
        {
            int absolute = gun_game_bind_player(roster[index]);
            if (absolute >= 0 && gun_game_progression_complete(&gun_game_players[absolute]))
            {
                set_message("Gun Game: a player completed the seven-weapon ladder.");
                return TRUE;
            }
        }
        return FALSE;
    }
    if (preset != MATCH_RULES_PRESET_ZOMBIES || !zombies_ready)
        return FALSE;
    count = active_roster(roster, HALO_PORT_MAXIMUM_NETWORK_PLAYERS);
    if (count < 2)
    {
        set_message("Zombies ended because fewer than two active players remain.");
        return TRUE;
    }
    for (index = 0; index < count; index++)
    {
        long player_index = roster[index];
        short absolute = (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
        if (absolute >= 0 && absolute < HALO_PORT_MAXIMUM_NETWORK_PLAYERS &&
            !zombies_player_is_infected(player_index))
            human_count++;
    }
    if (human_count == 0)
    {
        set_message("Zombies ended because no uninfected players remain.");
        return TRUE;
    }
    return FALSE;
}

static boolean match_rules_variant_is_zombies(void)
{
	struct game_variant *variant = game_engine_get_variant();
	wchar_t const *description;
	if (!variant || variant->game_engine_index != game_engine_slayer)
		return FALSE;
	description = variant->human_readable_game_description;
	return (description[0] == L'Z' && description[1] == L'o' && description[2] == L'm' &&
		description[3] == L'b' && description[4] == L'i' && description[5] == L'e' &&
		description[6] == L's') ||
		(description[0] == L'I' && description[1] == L'n' && description[2] == L'f' &&
		description[3] == L'e' && description[4] == L'c' && description[5] == L't' &&
		description[6] == L'i' && description[7] == L'o' && description[8] == L'n');
}

static boolean match_rules_variant_is_tower_of_power(void)
{
    struct game_variant *variant = game_engine_get_variant();
    wchar_t const *description;
    if (!variant || variant->game_engine_index != game_engine_slayer)
        return FALSE;
    description = variant->human_readable_game_description;
    return description[0] == L'T' && description[1] == L'o' && description[2] == L'w' &&
        description[3] == L'e' && description[4] == L'r' && description[5] == L' ' &&
        description[6] == L'P' && description[7] == L'o' && description[8] == L'w' &&
        description[9] == L'e' && description[10] == L'r';
}

short match_rules_vehicle_policy(long definition_index)
{
    if (match_rules_variant_is_zombies())
        return 0;
    if (match_rules_variant_is_tower_of_power())
        return vehicle_definition_is_turret(definition_index) ? 1 : 0;
    return -1;
}

static boolean match_rules_variant_is_gun_game(void)
{
    struct game_variant *variant = game_engine_get_variant();
    wchar_t const *description;
    if (!variant || variant->game_engine_index != game_engine_slayer)
        return FALSE;
    description = variant->human_readable_game_description;
    return description[0] == L'G' && description[1] == L'u' && description[2] == L'n' &&
        description[3] == L' ' && description[4] == L'G' && description[5] == L'a' &&
        description[6] == L'm' && description[7] == L'e';
}

boolean match_rules_weapon_allowed_for_unit(long unit_index, long weapon_definition_index)
{
    boolean gun_game = match_rules_variant_is_gun_game();
    boolean zombies = match_rules_variant_is_zombies();
    boolean tower = match_rules_variant_is_tower_of_power();
    struct unit_datum *vehicle_unit = tower && unit_index != NONE ? vehicle_try_and_get(unit_index) : NULL;
    boolean vehicle = vehicle_unit && vehicle_definition_is_turret(vehicle_unit->definition_index);
    long player_index;
    long shotgun_definition;
    boolean infected;
    boolean melee_weapon;

    if (gun_game)
    {
        long owner = unit_index != NONE ? player_index_from_unit_index(unit_index) : NONE;
        int absolute;
        int stage;
        /* Clients receive the host inventory over the existing weapon-object
           messages; only the host decides whether a pickup is permitted. */
        if (game_connection() == _game_connection_network_client || network_game_distributed_client())
            return TRUE;
        if (owner == NONE)
            return FALSE;
        absolute = gun_game_bind_player(owner);
        if (absolute < 0)
            return FALSE;
        stage = gun_game_progression_stage(&gun_game_players[absolute]);
        return !gun_game_progression_complete(&gun_game_players[absolute]) &&
            stage < GUN_GAME_WEAPON_STAGE_COUNT && weapon_definition_index == gun_game_weapon_definition(stage);
    }
    if (weapon_definition_index == NONE)
        return !zombies && !tower;
    player_index = unit_index != NONE ? player_index_from_unit_index(unit_index) : NONE;
    infected = zombies && player_index != NONE && match_rules_player_melee_only(player_index);
    melee_weapon = infected && nxhalo_zombie_melee_weapon_allowed(weapon_definition_index);
    /* Global weapon-list index 8 is the stock shotgun entry. Missing tags
       resolve to NONE and therefore do not silently allow a player weapon. */
    shotgun_definition = list_index_to_weapon_definition_index(8);
    return match_rules_restricted_weapon_allowed(zombies, tower, vehicle, infected,
        melee_weapon, shotgun_definition != NONE && weapon_definition_index == shotgun_definition);
}

boolean match_rules_grenade_pickup_allowed(long unit_index)
{
    (void)unit_index;
    return !match_rules_variant_is_zombies() && !match_rules_variant_is_tower_of_power() &&
        !match_rules_variant_is_gun_game();
}

boolean match_rules_player_melee_only(long player_index)
{
	struct player_datum *player;
	short absolute;
	if (player_index == NONE || !game_engine_running() || !game_engine_has_teams() ||
		!match_rules_variant_is_zombies())
		return FALSE;
	player = player_try_and_get(player_index);
	if (!player || player->team_index != 1)
		return FALSE;
	if (game_connection() == _game_connection_network_client || network_game_distributed_client())
		return TRUE;
	absolute = (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
	return absolute >= 0 && absolute < HALO_PORT_MAXIMUM_NETWORK_PLAYERS &&
		zombies_ready && zombies_player_is_infected(player_index);
}

boolean match_rules_player_lunge_active(long player_index)
{
    short absolute;
    unsigned long duration;
    long now;

    if (player_index == NONE || TICKS_PER_SECOND <= 0)
        return FALSE;
    absolute = (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
    if (absolute < 0 || absolute >= HALO_PORT_MAXIMUM_NETWORK_PLAYERS ||
        lunge_tracked_player[absolute] != player_index || lunge_started_tick[absolute] == NONE)
        return FALSE;
    duration = (unsigned long)((TICKS_PER_SECOND + 9) / 10);
    now = game_time_get();
    return now >= lunge_started_tick[absolute] &&
        (unsigned long)(now - lunge_started_tick[absolute]) < duration;
}

void match_rules_host_apply_infected_lunge(long player_index, struct unit_control_data *control,
    boolean equipment_action_consumed)
{
    struct player_datum *player;
    struct biped_datum *attacker;
    struct object_iterator iterator;
    struct biped_datum *target;
    long target_index = NONE;
    short absolute;
    boolean pressed;
    real best_distance = 2.8f;
    real_vector3d best_direction = { 0.f, 0.f, 0.f };
    long now;
    unsigned long lunge_ticks = (unsigned long)((TICKS_PER_SECOND + 9) / 10);
    real lunge_speed_per_tick = 9.f / (real)TICKS_PER_SECOND;

    if (!control || player_index == NONE || TICKS_PER_SECOND <= 0)
        return;
    absolute = (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
    if (absolute < 0 || absolute >= HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
        return;
    player = player_try_and_get(player_index);
    if (lunge_tracked_player[absolute] != player_index)
    {
        lunge_tracked_player[absolute] = player_index;
        lunge_button_held[absolute] = FALSE;
        lunge_last_tick[absolute] = NONE;
        lunge_started_tick[absolute] = NONE;
        lunge_direction[absolute] = *global_zero_vector3d;
    }
    if (!match_rules_player_melee_only(player_index) ||
        !network_distributed_player_lunge_supported(player_index) ||
        !player || player->unit_index == NONE)
    {
        lunge_button_held[absolute] = FALSE;
        lunge_started_tick[absolute] = NONE;
        return;
    }

    now = game_time_get();
    attacker = biped_try_and_get(player->unit_index);
    if (!attacker || attacker->object.parent_object_index != NONE ||
        attacker->object.body_vitality <= 0.f || TEST_FLAG(attacker->object.damage_flags, _object_dead_bit) ||
        TEST_FLAG(attacker->biped.flags, _biped_airborne_bit))
    {
        lunge_started_tick[absolute] = NONE;
        lunge_button_held[absolute] =
            TEST_FLAG(control->control_flags, _unit_control_use_equipment_bit);
        return;
    }

    /* Biped velocity is world units per tick. Native biped physics consumes
       it and runs its ordinary swept collision/slide resolution. */
    if (lunge_started_tick[absolute] != NONE &&
        (unsigned long)(now - lunge_started_tick[absolute]) < lunge_ticks)
    {
        attacker->object.translational_velocity.i =
            lunge_direction[absolute].i * lunge_speed_per_tick;
        attacker->object.translational_velocity.j =
            lunge_direction[absolute].j * lunge_speed_per_tick;
        return;
    }
    lunge_started_tick[absolute] = NONE;

    pressed = TEST_FLAG(control->control_flags, _unit_control_use_equipment_bit);
    if (!pressed)
    {
        lunge_button_held[absolute] = FALSE;
        return;
    }
    if (lunge_button_held[absolute])
        return;
    lunge_button_held[absolute] = TRUE;
    if (equipment_action_consumed)
        return;
    if (lunge_last_tick[absolute] != NONE &&
        (unsigned long)(now - lunge_last_tick[absolute]) < (unsigned long)((TICKS_PER_SECOND * 3 + 3) / 4))
        return;

    object_iterator_new(&iterator, _object_mask_biped, 0);
    while ((target = object_iterator_next(&iterator)) != NULL)
    {
        struct player_datum *target_player;
        struct collision_result collision;
        real_point3d start, end;
        real_vector3d toward;
        real distance, forward_dot;
        long candidate_index = iterator.index;
        if (candidate_index == player->unit_index || target->unit.player_index == NONE ||
            target->object.parent_object_index != NONE || target->object.body_vitality <= 0.f ||
            TEST_FLAG(target->object.damage_flags, _object_dead_bit) ||
            TEST_FLAG(target->biped.flags, _biped_airborne_bit))
            continue;
        target_player = player_try_and_get(target->unit.player_index);
        if (!target_player || target_player->quit_out_of_game || target_player->team_index != 0)
            continue;

        toward.i = target->object.position.x - attacker->object.position.x;
        toward.j = target->object.position.y - attacker->object.position.y;
        toward.k = target->object.position.z - attacker->object.position.z;
        distance = magnitude3d(&toward);
        if (distance < 0.65f || distance > best_distance)
            continue;
        forward_dot = dot_product3d(&toward, &control->facing_vector) / distance;
        if (forward_dot < 0.70f)
            continue;

        start = attacker->object.position;
        end = target->object.position;
        start.z += 0.35f;
        end.z += 0.35f;
        toward.i = end.x - start.x;
        toward.j = end.y - start.y;
        toward.k = end.z - start.z;
        if (collision_test_vector(_collision_test_for_line_of_sight_flags | FLAG(_collision_test_objects_bipeds_bit), &start, &toward,
                player->unit_index, &collision) &&
            (collision.type != _collision_result_object || collision.object_index != candidate_index))
            continue;

        toward.i = target->object.position.x - attacker->object.position.x;
        toward.j = target->object.position.y - attacker->object.position.y;
        toward.k = 0.f;
        if (normalize3d(&toward) == 0.f)
            continue;
        if (!match_rules_infected_lunge_candidate_allowed(TRUE,
                attacker->object.body_vitality > 0.f && !TEST_FLAG(attacker->object.damage_flags, _object_dead_bit),
                attacker->object.parent_object_index == NONE && !TEST_FLAG(attacker->biped.flags, _biped_airborne_bit),
                target->object.body_vitality > 0.f && !TEST_FLAG(target->object.damage_flags, _object_dead_bit),
                target->object.parent_object_index == NONE && !TEST_FLAG(target->biped.flags, _biped_airborne_bit),
                TRUE, TRUE, distance, forward_dot))
            continue;
        best_distance = distance;
        best_direction = toward;
        target_index = candidate_index;
    }
    if (target_index == NONE)
        return;

    lunge_last_tick[absolute] = now;
    lunge_started_tick[absolute] = now;
    lunge_direction[absolute] = best_direction;
    attacker->object.translational_velocity.i = best_direction.i * lunge_speed_per_tick;
    attacker->object.translational_velocity.j = best_direction.j * lunge_speed_per_tick;
}

void match_rules_filter_player_control(long player_index, struct unit_control_data *control)
{
	struct player_datum *player;
	struct unit_datum *unit;
	if (!control || !match_rules_player_melee_only(player_index))
		return;
	player = player_get(player_index);
	unit = unit_try_and_get(player->unit_index);
	if (!unit)
		return;
	match_rules_filter_infected_control(TRUE, unit->unit.current_weapon_index, control);
}

/* Called after standard loadout and grenades have been assigned. */
void match_rules_host_postspawn_player(long player_index)
{
    struct player_datum *player;
    struct unit_datum *unit;
    short absolute;
    long unit_index;
    short preset = match_rules_preset_get();
    if (game_connection() == _game_connection_network_client ||
        network_game_distributed_client() || player_index == NONE)
        return;
    player = player_get(player_index);
    unit_index = player->unit_index;
    unit = unit_try_and_get(unit_index);
    if (!unit)
        return;
    if (preset == MATCH_RULES_PRESET_SWAT)
    {
        unit->unit.grenade_counts[_unit_grenade_human_fragmentation] = 0;
        unit->unit.grenade_counts[_unit_grenade_covenant_plasma] = 0;
        unit->unit.desired_grenade_index = NONE;
    }
    else if (preset == MATCH_RULES_PRESET_ZOMBIES)
    {
        absolute = (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
        unit->unit.grenade_counts[_unit_grenade_human_fragmentation] = 0;
        unit->unit.grenade_counts[_unit_grenade_covenant_plasma] = 0;
        unit->unit.desired_grenade_index = NONE;
        if (absolute >= 0 && absolute < HALO_PORT_MAXIMUM_NETWORK_PLAYERS && zombies_player_is_infected(player_index))
        {
            if (!nxhalo_give_zombie_melee_weapon(unit_index))
                set_message("Melee weapon could not be equipped; infected still cannot fire or throw grenades.");
        }
    }
    else if (preset == MATCH_RULES_PRESET_GUN_GAME)
    {
        gun_game_equip_player_stage(player_index);
    }
    else if (preset == MATCH_RULES_PRESET_TOWER_OF_POWER)
    {
        unit->unit.grenade_counts[_unit_grenade_human_fragmentation] = 0;
        unit->unit.grenade_counts[_unit_grenade_covenant_plasma] = 0;
        unit->unit.desired_grenade_index = NONE;
    }
}

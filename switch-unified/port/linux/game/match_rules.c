/*
 * Host-authoritative Halo match presets. All faction and infection bipeds are
 * resolved from tags already loaded by the selected map. This adds no wire
 * fields and does not deliver assets.
 */
#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/game_globals.h"
#include "game/players.h"
#include "math/real_math.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "networking/network_server_manager_internal.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "units/units.h"
#include "../src/port_config.h"
#include "match_rules.h"
#include "nxhalo_custom_content.h"

#include <stdio.h>

enum { _faction_none, _faction_covenant, _faction_usmc, _faction_flood };
static char match_rules_message[192];
static unsigned long cached_local_seed;
static boolean local_seed_valid;
static boolean zombies_initialized;
static boolean zombies_ready;
static boolean zombies_infected[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
static boolean zombies_initial_roster[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
static long zombies_tracked_player[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];

static char const * const match_preset_names[MATCH_RULES_PRESET_COUNT] = {
    "Standard", "Faction match", "SWAT", "Tower of Power",
    "Grenade Dodgeball", "Zombies", "Native Race"
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
    local_seed_valid = FALSE;
    cached_local_seed = 0;
    zombies_initialized = FALSE;
    zombies_ready = FALSE;
    short index;
    csmemset(zombies_infected, 0, sizeof(zombies_infected));
    csmemset(zombies_initial_roster, 0, sizeof(zombies_initial_roster));
    for (index = 0; index < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; index++)
        zombies_tracked_player[index] = NONE;
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
        message = "The selected map must supply Tower of Power geometry and placements.";
        break;
    case MATCH_RULES_PRESET_RACING:
        if (count_race_track_markers() < 2)
        {
            available = FALSE;
            message = "Native Race requires a map with at least two Race Track netgame markers; track geometry is not generated.";
        }
        break;
    case MATCH_RULES_PRESET_ZOMBIES:
        if (players_in_game() < 2)
            message = "Zombies waits for two connected players. Flood biped tags are optional; without them, infected retain the standard appearance.";
        else if (!faction_bipeds(NXHALO_FACTION_FLOOD, definitions, 8))
            message = "Zombies is playable with standard infected appearance; Flood biped tags are optional and are not auto-delivered.";
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
    short preset = match_rules_preset_get();
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

boolean match_rules_apply_variant_preset(short preset, struct game_variant *variant,
    struct game_variant_options *options)
{
    short team, vehicle;
    if (!variant || !options || !match_rules_preset_supported(preset))
        return FALSE;

    switch (preset)
    {
    case MATCH_RULES_PRESET_STANDARD:
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
        variant->universal_variant.teams = TRUE;
        options->loadout = _loadout_custom;
        options->primary_weapon = _loadout_weapon_shotgun;
        options->secondary_weapon = _loadout_weapon_none;
        set_message("Tower of Power template applied; map-owned tower geometry and placements are checked after map load.");
        return TRUE;
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
        variant->universal_variant.teams = TRUE;
        options->auto_team_balance = FALSE;
        break;
    case MATCH_RULES_PRESET_RACING:
        variant_template(variant, "team_race", "Native Race");
        variant->universal_variant.teams = TRUE;
        break;
    default:
        return FALSE;
    }

    if (preset == MATCH_RULES_PRESET_SWAT || preset == MATCH_RULES_PRESET_DODGEBALL)
    {
        for (team = 0; team < 2; team++)
        {
            options->vehicle_set[team] = 1;
            for (vehicle = 0; vehicle < NUMBER_OF_VARIANT_VEHICLES; vehicle++)
                options->vehicle_counts[team][vehicle] = 0;
        }
    }
    set_message("Preset template applied; map requirements will be checked after the selected map loads.");
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
    set_message("Zombies: one initial infected chosen; infected players return as Flood and use melee only.");
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
    player = player_get(player_index);
    update_host_team(player, player_index, zombies_player_is_infected(player_index) ? 1 : 0);
}

void match_rules_host_player_killed(long dead_player_index)
{
    struct player_datum *player;
    short absolute;
    if (match_rules_preset_get() != MATCH_RULES_PRESET_ZOMBIES ||
        game_connection() == _game_connection_network_client ||
        network_game_distributed_client() || dead_player_index == NONE || !zombies_ready)
        return;
    absolute = (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(dead_player_index);
    if (absolute < 0 || absolute >= HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
        return;
    zombies_infected[absolute] = TRUE;
    player = player_get(dead_player_index);
    update_host_team(player, dead_player_index, 1);
    set_message("Zombies: the host converted the player on death; infected players spawn with melee only.");
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
            "Zombies is active; Flood biped tags are unavailable on this map, so infected retain the standard appearance." :
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
    if (match_rules_preset_get() != MATCH_RULES_PRESET_ZOMBIES || !zombies_ready)
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
        if (absolute >= 0 && absolute < HALO_PORT_MAXIMUM_NETWORK_PLAYERS && zombies_player_is_infected(player_index))
        {
            unit_delete_all_weapons(unit_index);
            unit->unit.grenade_counts[_unit_grenade_human_fragmentation] = 0;
            unit->unit.grenade_counts[_unit_grenade_covenant_plasma] = 0;
            unit->unit.desired_grenade_index = NONE;
        }
    }
}

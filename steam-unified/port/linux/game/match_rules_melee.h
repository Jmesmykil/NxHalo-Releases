#ifndef MATCH_RULES_MELEE_H
#define MATCH_RULES_MELEE_H

#include "cseries.h"
#include "units/units.h"
#include "units/unit_control_data.h"

#include "match_rules_action.h"

/* Shared by the host control path, damage-report validator, and regression harness. */
static void match_rules_filter_infected_control(
    boolean infected,
    short current_weapon_index,
    struct unit_control_data *control)
{
    if (!infected || !control)
        return;
    SET_FLAG(control->control_flags, _unit_control_weapon_primary_trigger_bit, FALSE);
    SET_FLAG(control->control_flags, _unit_control_weapon_secondary_trigger_bit, FALSE);
    SET_FLAG(control->control_flags, _unit_control_throw_grenade_bit, FALSE);
    SET_FLAG(control->control_flags, _unit_control_weapon_reload_bit, FALSE);
    SET_FLAG(control->control_flags, _unit_control_swap_weapons_bit, FALSE);
    control->primary_trigger = 0.0f;
    control->grenade_index = NONE;
    control->weapon_index = current_weapon_index;
}

static boolean match_rules_infected_damage_allowed(boolean infected, boolean melee)
{
    return !infected || melee;
}

/* An infected player without a loaded melee tag must be truly unarmed. */
static boolean match_rules_infected_inventory_weapon_allowed(
    boolean infected, long weapon_definition, long melee_definition)
{
    return !infected || (melee_definition != NONE && weapon_definition == melee_definition);
}

static boolean match_rules_mode_grenade_pickup_allowed(boolean zombies, boolean tower, boolean gun_game)
{
    return !zombies && !tower && !gun_game;
}

/* A new datum in a roster slot is a late join/reuse and starts infected. */
static boolean match_rules_zombie_player_is_infected(
    long *tracked_players, boolean *initial_roster, boolean *infected,
    short capacity, short slot, long player_datum)
{
    if (!tracked_players || !initial_roster || !infected || slot < 0 ||
        slot >= capacity || player_datum == NONE)
        return FALSE;
    if (tracked_players[slot] != player_datum)
    {
        tracked_players[slot] = player_datum;
        initial_roster[slot] = FALSE;
        infected[slot] = TRUE;
    }
    return infected[slot];
}

static void match_rules_zombie_state_reset(
    boolean *initialized, boolean *ready, boolean *initial_roster,
    boolean *infected, long *tracked_players, short capacity)
{
    short slot;
    if (initialized) *initialized = FALSE;
    if (ready) *ready = FALSE;
    for (slot = 0; slot < capacity; slot++)
    {
        if (initial_roster) initial_roster[slot] = FALSE;
        if (infected) infected[slot] = FALSE;
        if (tracked_players) tracked_players[slot] = NONE;
    }
}

/* Pure inventory policy shared by the runtime and focused regression.
   Tower vehicles retain their authored mounted weapons; player bipeds stay
   restricted to the selected mode's loadout. */
static boolean match_rules_restricted_weapon_allowed(
    boolean zombies,
    boolean tower,
    boolean vehicle,
    boolean infected,
    boolean melee_weapon,
    boolean shotgun)
{
    if (!zombies && !tower)
        return TRUE;
    if (tower && vehicle)
        return TRUE;
    if (zombies && infected)
        return melee_weapon;
    return shotgun;
}

/* Pure safety gate for the host's bounded lunge candidate. */
static boolean match_rules_infected_lunge_candidate_allowed(
    boolean infected,
    boolean attacker_alive,
    boolean attacker_on_foot,
    boolean target_alive,
    boolean target_on_foot,
    boolean line_of_sight,
    boolean cooldown_ready,
    real distance,
    real forward_dot)
{
    return infected && attacker_alive && attacker_on_foot && target_alive && target_on_foot &&
        line_of_sight && cooldown_ready && distance >= 0.65f && distance <= 2.8f &&
        forward_dot >= 0.70f;
}

#endif

#ifndef MATCH_RULES_MELEE_H
#define MATCH_RULES_MELEE_H

#include "cseries.h"
#include "units/units.h"
#include "units/unit_control_data.h"

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

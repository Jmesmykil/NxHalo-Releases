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

#endif

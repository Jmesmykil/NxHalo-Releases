#ifndef MATCH_RULES_ACTION_H
#define MATCH_RULES_ACTION_H

#include "cseries.h"
#include "units/units.h"

/* Infected players use their ordinary attack button for melee. Apply this
   before player action processing, which otherwise drops objective weapons
   on primary/secondary fire before the later unit-control filter runs. */
static unsigned long match_rules_infected_action_flags(
    boolean infected,
    unsigned long flags)
{
    if (!infected)
        return flags;
    if (TEST_FLAG(flags, _unit_control_weapon_primary_trigger_bit) ||
        TEST_FLAG(flags, _unit_control_weapon_secondary_trigger_bit))
        SET_FLAG(flags, _unit_control_use_equipment_bit, TRUE);
    SET_FLAG(flags, _unit_control_weapon_primary_trigger_bit, FALSE);
    SET_FLAG(flags, _unit_control_weapon_secondary_trigger_bit, FALSE);
    SET_FLAG(flags, _unit_control_throw_grenade_bit, FALSE);
    SET_FLAG(flags, _unit_control_weapon_reload_bit, FALSE);
    SET_FLAG(flags, _unit_control_swap_weapons_bit, FALSE);
    return flags;
}

#endif

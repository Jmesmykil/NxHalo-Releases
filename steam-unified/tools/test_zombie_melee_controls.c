#include "../port/linux/game/match_rules_melee.h"

int main(void)
{
    struct unit_control_data control;
    control.control_flags = 0xFFFF;
    control.primary_trigger = 1.0f;
    control.grenade_index = 1;
    control.weapon_index = 7;
    match_rules_filter_infected_control(TRUE, 3, &control);
    if (TEST_FLAG(control.control_flags, _unit_control_weapon_primary_trigger_bit) ||
        TEST_FLAG(control.control_flags, _unit_control_weapon_secondary_trigger_bit) ||
        TEST_FLAG(control.control_flags, _unit_control_throw_grenade_bit) ||
        TEST_FLAG(control.control_flags, _unit_control_weapon_reload_bit) ||
        TEST_FLAG(control.control_flags, _unit_control_swap_weapons_bit) ||
        !TEST_FLAG(control.control_flags, _unit_control_use_equipment_bit) ||
        control.primary_trigger != 0.0f || control.grenade_index != NONE ||
        control.weapon_index != 3)
        return 1;
    if (!match_rules_restricted_weapon_allowed(FALSE, TRUE, TRUE, FALSE, FALSE, FALSE) ||
        !match_rules_restricted_weapon_allowed(FALSE, TRUE, FALSE, FALSE, FALSE, TRUE) ||
        match_rules_restricted_weapon_allowed(FALSE, TRUE, FALSE, FALSE, FALSE, FALSE) ||
        !match_rules_restricted_weapon_allowed(TRUE, FALSE, FALSE, FALSE, FALSE, TRUE) ||
        !match_rules_restricted_weapon_allowed(TRUE, FALSE, FALSE, TRUE, TRUE, FALSE) ||
        match_rules_restricted_weapon_allowed(TRUE, FALSE, FALSE, TRUE, FALSE, TRUE))
        return 4;
    if (!match_rules_infected_damage_allowed(FALSE, FALSE) ||
        !match_rules_infected_damage_allowed(TRUE, TRUE) ||
        match_rules_infected_damage_allowed(TRUE, FALSE))
        return 2;
    if (!match_rules_infected_lunge_candidate_allowed(TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, 1.5f, 0.9f) ||
        match_rules_infected_lunge_candidate_allowed(FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, 1.5f, 0.9f) ||
        match_rules_infected_lunge_candidate_allowed(TRUE, FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, 1.5f, 0.9f) ||
        match_rules_infected_lunge_candidate_allowed(TRUE, TRUE, FALSE, TRUE, TRUE, TRUE, TRUE, 1.5f, 0.9f) ||
        match_rules_infected_lunge_candidate_allowed(TRUE, TRUE, TRUE, FALSE, TRUE, TRUE, TRUE, 1.5f, 0.9f) ||
        match_rules_infected_lunge_candidate_allowed(TRUE, TRUE, TRUE, TRUE, FALSE, TRUE, TRUE, 1.5f, 0.9f) ||
        match_rules_infected_lunge_candidate_allowed(TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, TRUE, 1.5f, 0.9f) ||
        match_rules_infected_lunge_candidate_allowed(TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, 1.5f, 0.9f) ||
        match_rules_infected_lunge_candidate_allowed(TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, 2.9f, 0.9f) ||
        match_rules_infected_lunge_candidate_allowed(TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, 1.5f, 0.6f))
        return 3;
    return 0;
}

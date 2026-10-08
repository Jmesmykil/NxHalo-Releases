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
    if (!match_rules_infected_damage_allowed(FALSE, FALSE) ||
        !match_rules_infected_damage_allowed(TRUE, TRUE) ||
        match_rules_infected_damage_allowed(TRUE, FALSE))
        return 2;
    return 0;
}

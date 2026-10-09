#include "../port/linux/game/match_rules_melee.h"

/* units.h exposes unrelated inline random helpers; keep this policy test
   linkable without pulling the game runtime into the focused harness. */
unsigned long *get_global_random_seed_address(void)
{
    static unsigned long seed;
    return &seed;
}

unsigned short seed_random(unsigned long *seed)
{
    *seed = *seed * 1664525UL + 1013904223UL;
    return (unsigned short)(*seed >> 16);
}

int main(void)
{
    struct unit_control_data control;
    unsigned long flags, extra = FLAG(_unit_control_use_equipment_bit) | (1UL << 20);
    if (match_rules_infected_action_flags(FALSE, 0xFFFFFFFFUL) != 0xFFFFFFFFUL ||
        match_rules_infected_action_flags(TRUE, 0) != 0 ||
        match_rules_infected_action_flags(TRUE, extra) != extra)
        return 5;
    flags = match_rules_infected_action_flags(TRUE,
        FLAG(_unit_control_weapon_primary_trigger_bit) |
        FLAG(_unit_control_throw_grenade_bit) |
        FLAG(_unit_control_weapon_reload_bit) |
        FLAG(_unit_control_swap_weapons_bit) | (1UL << 20));
    if (flags != (FLAG(_unit_control_use_equipment_bit) | (1UL << 20)))
        return 6;
    if (match_rules_infected_action_flags(TRUE, FLAG(_unit_control_weapon_secondary_trigger_bit)) !=
        FLAG(_unit_control_use_equipment_bit))
        return 7;

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
    /* Infected retain only the exact loaded melee tag; unavailable means empty inventory. */
    if (!match_rules_infected_inventory_weapon_allowed(TRUE, 17, 17) ||
        match_rules_infected_inventory_weapon_allowed(TRUE, 18, 17) ||
        match_rules_infected_inventory_weapon_allowed(TRUE, 18, NONE) ||
        !match_rules_infected_inventory_weapon_allowed(FALSE, 18, NONE))
        return 8;
    /* Zombies, Tower, and Gun Game reject grenade pickups; Standard permits them. */
    if (match_rules_mode_grenade_pickup_allowed(TRUE, FALSE, FALSE) ||
        match_rules_mode_grenade_pickup_allowed(FALSE, TRUE, FALSE) ||
        match_rules_mode_grenade_pickup_allowed(FALSE, FALSE, TRUE) ||
        !match_rules_mode_grenade_pickup_allowed(FALSE, FALSE, FALSE))
        return 9;
    /* Initial human role persists through death; late join/reused datum is infected; reset clears roles. */
    {
        boolean initialized = TRUE, ready = TRUE, initial[2] = { TRUE, TRUE };
        boolean infected[2] = { FALSE, TRUE };
        long tracked[2] = { 0x10001, 0x20001 };
        if (match_rules_zombie_player_is_infected(tracked, initial, infected, 2, 0, 0x10001) ||
            !match_rules_zombie_player_is_infected(tracked, initial, infected, 2, 1, 0x30001) ||
            initial[1] ||
            match_rules_zombie_player_is_infected(tracked, initial, infected, 2, 0, 0x10001))
            return 11;
        match_rules_zombie_state_reset(&initialized, &ready, initial, infected, tracked, 2);
        if (initialized || ready || initial[0] || initial[1] || infected[0] || infected[1] ||
            tracked[0] != NONE || tracked[1] != NONE)
            return 12;
        if (!match_rules_zombie_player_is_infected(tracked, initial, infected, 2, 0, 0x40001) ||
            initial[0])
            return 13;
    }
    /* Survivors keep only the stock shotgun, while infected keep only melee. */
    if (!match_rules_restricted_weapon_allowed(TRUE, FALSE, FALSE, FALSE, FALSE, TRUE) ||
        match_rules_restricted_weapon_allowed(TRUE, FALSE, FALSE, FALSE, FALSE, FALSE) ||
        !match_rules_restricted_weapon_allowed(TRUE, FALSE, FALSE, TRUE, TRUE, FALSE) ||
        match_rules_restricted_weapon_allowed(TRUE, FALSE, FALSE, TRUE, FALSE, TRUE))
        return 10;
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

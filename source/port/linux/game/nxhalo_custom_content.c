/* Campaign character swapping adapted from fucktrevor/HCE-Mobile custom_content.c.
 * Native controls and safety guards: NxHalo experimental15. See provenance.json. */
#include "cseries.h"
#include "ai/actor_definitions.h"
#include "cutscene/cinematics.h"
#include "camera/director.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/game_globals.h"
#include "game/players.h"
#include "interface/hud_messaging.h"
#include "input/input.h"
#include "objects/object_definitions.h"
#include "objects/object_types.h"
#include "objects/objects.h"
#include "scenario/scenario.h"
#include "tag_files/tag_files.h"
#include "units/biped_definitions.h"
#include "units/bipeds.h"
#include "units/units.h"
#include "../src/port_config.h"
#include "scenario/scenario_definitions.h"
#include "cache/cache_files.h"
void platform_log(char const *format, ...);

static long selected_character;
static boolean third_person;
static boolean initialized;
static unsigned long settings_serial;
static long chief_definition=NONE;
static boolean chord_held;

static long native_character(void) { return selected_character; }
static void native_status(long status) { (void)status; }
static void native_message(char const *text)
{
    wchar_t wide[128];
    unsigned i;
    short local;
    for (i=0; i<127 && text[i]; ++i) wide[i]=(unsigned char)text[i];
    wide[i]=0;
    platform_log("[character] %s", text);
    for(local=local_player_get_next(NONE); local!=NONE; local=local_player_get_next(local))
        hud_print_message(local,wide);
}

enum
{
	_character_default,
	_character_master_chief,
	_character_marine,
	_character_grunt,
	_character_jackal,
	_character_elite,
	_character_hunter,
	_character_flood_human,
	_character_flood_elite,
	_character_infection_form,
	_character_sentinel,
	_character_monitor,
	_character_keyes,
	NUMBER_OF_CHARACTERS
};

enum
{
	_character_status_none,
	_character_status_playing,
	_character_status_not_here,
};

/* ---------- constants */

#define LOW_GRAVITY_SCALE 0.35f
#define SPEED_BOOST_SCALE 1.6f
#define BIG_HEAD_SCALE 2.5f
#define MAXIMUM_HEAD_NODES 32

/* the characters' biped tags, by the end of their names (the level's own
tags: each campaign level loads the characters it has) */
static struct
{
	char const *name;
	char const *tags[3];
} const characters[NUMBER_OF_CHARACTERS] =
{
	{ NULL, { NULL } },
	{ "Master Chief", { NULL } },
	{ "Marine", { "\\marine\\marine", "\\marine_armored\\marine_armored", NULL } },
	{ "Grunt", { "\\grunt\\grunt", NULL } },
	{ "Jackal", { "\\jackal\\jackal", NULL } },
	{ "Elite", { "\\elite\\elite", NULL } },
	{ "Hunter", { "\\hunter\\hunter", NULL } },
	{ "Flood combat form", { "\\floodcombat_human\\floodcombat_human", NULL } },
	{ "Flood Elite", { "\\floodcombat elite\\floodcombat elite", "\\floodcombat_elite\\floodcombat_elite", NULL } },
	{ "Infection form", { "\\flood_infection\\flood_infection", NULL } },
	{ "Sentinel", { "\\sentinel\\sentinel", NULL } },
	{ "343 Guilty Spark", { "\\monitor\\monitor", NULL } },
	{ "Captain Keyes", { "\\captain\\captain", NULL } },
};

/* ---------- globals */

static struct
{
	long applied_rules;
	boolean applied_invincible;
	real base_gravity;
	boolean base_gravity_known;
	/* this map's biped for the character asked for, looked up once */
	long looked_up_character;
	long character_definition_index;
	long reported_character;
	long status;
	/* the player plays another character on this map (so that choosing the
	level's own again brings the Master Chief back) */
	boolean swapped;
} custom_globals = { 0, FALSE, 0.f, FALSE, NONE, NONE, NONE, _character_status_none, FALSE };

/* ---------- private code */

static boolean string_ends_with(
	char const *string,
	char const *end)
{
	size_t string_length = strlen(string);
	size_t end_length = strlen(end);

	return string_length >= end_length && !_stricmp(string + string_length - end_length, end);
}

static long character_biped_definition(
	long character)
{
	if (character != custom_globals.looked_up_character)
	{
		long definition_index = NONE;

		if (character == _character_master_chief)
		{
			struct game_globals_player_information *player_information = TAG_BLOCK_GET_ELEMENT(
				&scenario_get_game_globals()->player_information,
				0,
				struct game_globals_player_information);

			definition_index = player_information->player_unit.index;
		}
		else if (character > _character_master_chief && character < NUMBER_OF_CHARACTERS)
		{
			short tag_index;

			for (tag_index = 0; definition_index == NONE && characters[character].tags[tag_index]; tag_index++)
			{
				struct tag_iterator iterator;
				long biped_definition_index;

				tag_iterator_new(&iterator, BIPED_DEFINITION_TAG);
				while ((biped_definition_index = tag_iterator_next(&iterator)) != NONE)
				{
					if (string_ends_with(tag_get_name(biped_definition_index), characters[character].tags[tag_index]))
					{
						definition_index = biped_definition_index;
						break;
					}
				}
			}
		}

		custom_globals.looked_up_character = character;
		custom_globals.character_definition_index = definition_index;
	}

	return custom_globals.character_definition_index;
}

/* the actor variant of the level that uses a biped, for its weapon */
static struct actor_variant_definition *biped_actor_variant(
	long biped_definition_index)
{
	struct tag_iterator iterator;
	long variant_index;
	struct actor_variant_definition *unarmed = NULL;

	tag_iterator_new(&iterator, ACTOR_VARIANT_DEFINITION_TAG);
	while ((variant_index = tag_iterator_next(&iterator)) != NONE)
	{
		struct actor_variant_definition *variant = actor_variant_definition_get(variant_index);

		if (variant->unit_reference.index == biped_definition_index)
		{
			if (variant->ranged_combat.reference.index != NONE)
				return variant;
			if (!unarmed)
				unarmed = variant;
		}
	}

	return unarmed;
}

static void arm_unit(
	long unit_index,
	long biped_definition_index,
	boolean master_chief)
{
	if (master_chief)
	{
		if (global_scenario_get()->scenario_starting_equipment.count > 0)
			player_add_equipment(unit_index, 0, TRUE);
	}
	else
	{
		struct actor_variant_definition *variant = biped_actor_variant(biped_definition_index);

		if (variant && variant->ranged_combat.reference.index != NONE)
		{
			struct object_placement_data placement_data;
			long weapon_index;

			object_placement_data_new(&placement_data, variant->ranged_combat.reference.index, unit_index);
			weapon_index = object_new(&placement_data);
			if (weapon_index != NONE &&
				!unit_add_weapon_to_inventory(unit_index, weapon_index, _unit_add_weapon_replace))
			{
				object_delete(weapon_index);
			}
		}
		if (variant && variant->grenade_combat.grenade_type != NONE)
			unit_add_grenade_type_to_inventory(unit_index, variant->grenade_combat.grenade_type, 2);
	}

	return;
}

/* the player's unit becomes a new one of another biped, where it stands */
static boolean swap_player_unit(
	short local_player_index,
	long old_unit_index,
	long biped_definition_index,
	boolean master_chief)
{
	struct unit_datum *old_unit = unit_get(old_unit_index);
	long player_index = local_player_get_player_index(local_player_index);
	struct player_datum *player = player_get(player_index);
	struct object_placement_data placement_data;
	long unit_index;

	object_placement_data_new(&placement_data, biped_definition_index, NONE);
	placement_data.position = old_unit->object.position;
	placement_data.forward = old_unit->object.forward;
	placement_data.up = *global_up3d;
	placement_data.forward.k = 0.f;
	if (normalize3d(&placement_data.forward) == 0.f)
		placement_data.forward = *global_forward3d;
	unit_index = object_new(&placement_data);
	if (unit_index == NONE)
		return FALSE;

	{
		struct unit_datum *unit = unit_get(unit_index);

		unit->object.owner_player_index = player_index;
		unit->object.owner_team_index = (short)player->team_index;
	}
	unit_delete_all_weapons(old_unit_index);
	players_set_local_player_unit(local_player_index, unit_index);
	object_delete(old_unit_index);
	arm_unit(unit_index, biped_definition_index, master_chief);

	return TRUE;
}

static void report_character(
	long character,
	long status)
{
	if (status != custom_globals.status || character != custom_globals.reported_character)
	{
		custom_globals.status = status;
		custom_globals.reported_character = character;
		native_status(status);
		if (status == _character_status_not_here)
		{
			char text[128];

			csprintf(text, "This level has no %s; keeping your current character.",
				characters[character].name);
			native_message(text);
		}
	}

	return;
}

static void update_character(
	void)
{
	long character = native_character();
	boolean back_to_master_chief = FALSE;
	short local_player_index;

	if (character <= _character_default && custom_globals.swapped)
	{
		character = _character_master_chief;
		back_to_master_chief = TRUE;
	}
	if (character <= _character_default || character >= NUMBER_OF_CHARACTERS ||
		(game_engine_running() || game_connection() != _game_connection_local))
	{
		report_character(character, _character_status_none);
		return;
	}

	for (local_player_index = local_player_get_next(NONE);
		local_player_index != NONE;
		local_player_index = local_player_get_next(local_player_index))
	{
		long unit_index = player_control_get_unit_index(local_player_index);
		long definition_index;
		struct unit_datum *unit;

		if (unit_index == NONE)
			continue;
		definition_index = character_biped_definition(character);
		if (definition_index == NONE)
		{
			report_character(character, _character_status_not_here);
			continue;
		}
		unit = unit_get(unit_index);
		if (unit->definition_index == definition_index)
		{
			if (character == _character_master_chief)
				custom_globals.swapped = FALSE;
			report_character(back_to_master_chief ? _character_default : character,
				back_to_master_chief ? _character_status_none : _character_status_playing);
			continue;
		}
		/* on foot, alive, and not in a cutscene or the level's opening */
		if (unit->object.type != _object_type_biped ||
			unit->object.parent_object_index != NONE ||
			TEST_FLAG(unit->object.damage_flags, _object_dead_bit) ||
			cinematic_in_progress())
		{
			continue;
		}
		{
			boolean swapped_now = swap_player_unit(local_player_index, unit_index, definition_index, character == _character_master_chief);
			long new_unit_index = player_control_get_unit_index(local_player_index);

			platform_log("[character] swap to %s (%s): %s, unit %lx -> %lx", characters[character].name,
				tag_get_name(definition_index), swapped_now ? "done" : "FAILED to create the unit", unit_index, new_unit_index);
			if (swapped_now && character != _character_master_chief)
				custom_globals.swapped = TRUE;
		}
	}

	return;
}


/* Minus + left stick click: D-pad left/right selects, up toggles camera.
 * Changes are campaign-only, deferred while dead, seated or cinematic. */
void nxhalo_custom_reset(void)
{
    chief_definition=NONE;
    custom_globals.looked_up_character=NONE;
    custom_globals.character_definition_index=NONE;
    custom_globals.reported_character=NONE;
    custom_globals.status=_character_status_none;
    custom_globals.swapped=FALSE;
    chord_held=FALSE;
}
void nxhalo_custom_update(void)
{
    struct gamepad_state const *pad;
    boolean chord;
    char text[128];
    if (!initialized || settings_serial != config_changes()) {
        selected_character=config_integer("mods.character");
        if (selected_character<0 || selected_character>=NUMBER_OF_CHARACTERS) selected_character=0;
        third_person=config_boolean("mods.third_person");
        initialized=TRUE;
        settings_serial=config_changes();
    }
    if (game_engine_running() || game_connection()!=_game_connection_local) return;
    pad=input_get_gamepad_state(0);
    chord=pad && pad->buttons[_gamepad_binary_button_back]>0 && pad->buttons[_gamepad_binary_button_left_thumb]>0;
    if (chord) {
        director_inhibit_input(0);
        if (!chord_held) native_message("Characters: hold Minus + L-stick; D-pad left/right selects, up changes camera.");
        if(pad->buttons[_gamepad_binary_button_dpad_left]==1 || pad->buttons[_gamepad_binary_button_dpad_right]==1) {
            selected_character=(selected_character+NUMBER_OF_CHARACTERS+(pad->buttons[_gamepad_binary_button_dpad_right]==1 ? 1 : -1))%NUMBER_OF_CHARACTERS;
            csprintf(text,"Selected: %s",selected_character ? characters[selected_character].name : "Original character");
            native_message(text);
            csprintf(text,"%ld",selected_character);
            config_write("mods.character",text);
            settings_serial=config_changes();
        }
        if(pad->buttons[_gamepad_binary_button_dpad_up]==1) {
            third_person=!third_person;
            config_write_boolean("mods.third_person",third_person);
            settings_serial=config_changes();
            native_message(third_person ? "Third-person camera enabled" : "Third-person camera disabled");
        }
    }
    chord_held=chord;
    update_character();
}
boolean nxhalo_custom_following(long unit_index)
{
    short local;
    if (game_engine_running() || game_connection()!=_game_connection_local || cinematic_in_progress()) return FALSE;
    for(local=local_player_get_next(NONE); local!=NONE; local=local_player_get_next(local))
        if(player_control_get_unit_index(local)==unit_index)
            {
            if(chief_definition==NONE)chief_definition=character_biped_definition(_character_master_chief);
            return third_person || (custom_globals.swapped && unit_get(unit_index)->definition_index!=chief_definition);
        }
    return FALSE;
}

#ifdef HALO_GAME_BROWSER
#include "interface/event_manager.h"
#include "cseries/cseries_windows.h"
#include "../src/ui_overlay.h"
static boolean character_menu_active, character_menu_camera, character_menu_save_failed;
static long character_menu_choice;
static short character_menu_last_button=NONE;
static unsigned long character_menu_button_time;
static boolean character_menu_debug_opened;
boolean nxhalo_character_menu_active(void) { return character_menu_active; }
boolean nxhalo_character_menu_process(boolean available)
{
    struct event_record event;
    struct gamepad_state const *pad=input_get_gamepad_state(0);
    boolean debug_open=!character_menu_debug_opened &&
        !strcmp(config_string("debug.menu_open"), "nxhalo/characters");
    if (!character_menu_active && (debug_open ||
        (available && pad && pad->buttons[_gamepad_analog_button_x]==1))) {
        character_menu_choice=config_integer("mods.character");
        if(character_menu_choice<0 || character_menu_choice>=NUMBER_OF_CHARACTERS) character_menu_choice=0;
        character_menu_camera=config_boolean("mods.third_person");
        character_menu_active=TRUE; character_menu_debug_opened=TRUE;
        character_menu_last_button=NONE; character_menu_save_failed=FALSE;
        event_manager_flush();
        platform_log("[character_menu] opened style=xbox selected=%ld",character_menu_choice);
        return TRUE;
    }
    if (!character_menu_active) return FALSE;
    while(character_menu_active && get_next_event(&event,NONE)) {
        long delta=0;
        if(event.type==1) {
            if(event.data.stick.y==SHORT_MAX) delta=-1;
            else if(event.data.stick.y==SHORT_MIN) delta=1;
        } else if(event.type==3) {
            unsigned long now=system_milliseconds();
            if(event.data.button.index==character_menu_last_button && now-character_menu_button_time<250) continue;
            character_menu_last_button=event.data.button.index; character_menu_button_time=now;
            switch(event.data.button.index) {
            case _gamepad_binary_button_dpad_up: delta=-1; break;
            case _gamepad_binary_button_dpad_down: delta=1; break;
            case _gamepad_binary_button_dpad_left: delta=-7; break;
            case _gamepad_binary_button_dpad_right: delta=7; break;
            case _gamepad_analog_button_y: character_menu_camera=!character_menu_camera; break;
            case _gamepad_analog_button_a:
            case _gamepad_binary_button_start: {
                char value[16];
                csprintf(value,"%ld",character_menu_choice);
                if (!config_write("mods.character",value) ||
                    !config_write_boolean("mods.third_person",character_menu_camera)) {
                    character_menu_save_failed=TRUE;
                    platform_log("[character_menu] save failed");
                    break;
                }
                platform_log("[character_menu] saved character=%ld third_person=%d",character_menu_choice,character_menu_camera);
                character_menu_active=FALSE; break;
            }
            case _gamepad_analog_button_b:
            case _gamepad_binary_button_back:
                character_menu_active=FALSE; platform_log("[character_menu] cancelled"); break;
            default: break;
            }
        }
        if(delta) character_menu_choice=(character_menu_choice+NUMBER_OF_CHARACTERS+delta)%NUMBER_OF_CHARACTERS;
    }
    event_manager_flush();
    return TRUE;
}
void nxhalo_character_menu_render(boolean available)
{
    short i;
    if(!ui_overlay_available())return;
    if(!character_menu_active) {
        if(available) {
            ui_overlay_rect(155,438,330,31,5,0x07162DF0);
            ui_overlay_button(UI_BUTTON_X,18,171,445,0xFFFFFFFF);
            ui_overlay_text(UI_FONT_BOLD,13,197,445,UI_ALIGN_LEFT,0xD9EAFFFF,"Campaign character / camera");
        }
        return;
    }
    ui_overlay_gradient(-160,0,960,480,0,0x06152EFF,0x020813FF);
    ui_overlay_text(UI_FONT_BOLD,26,34,26,UI_ALIGN_LEFT,0xC4E3FFFF,"CAMPAIGN CHARACTER");
    ui_overlay_text(UI_FONT_REGULAR,12,36,65,UI_ALIGN_LEFT,0xB7C7DFFF,"Choose your character for campaign gameplay.");
    for(i=0;i<NUMBER_OF_CHARACTERS;i++) {
        float x=36+(i/7)*286, y=102+(i%7)*35;
        boolean chosen=i==character_menu_choice;
        ui_overlay_rect(x,y,270,30,4,chosen?0x2467ACFF:0x10233CFF);
        ui_overlay_text(chosen?UI_FONT_BOLD:UI_FONT_REGULAR,14,x+12,y+7,UI_ALIGN_LEFT,
            chosen?0xFFFFFFFF:0xC8D5E8FF,i?characters[i].name:"Original character");
    }
    ui_overlay_text(UI_FONT_REGULAR,11,36,360,UI_ALIGN_LEFT,0xB7C7DFFF,"The chosen character must exist in the loaded campaign level.");
    ui_overlay_text(UI_FONT_REGULAR,11,36,379,UI_ALIGN_LEFT,0xB7C7DFFF,"Online and System Link keep the standard multiplayer character.");
    ui_overlay_button(UI_BUTTON_Y,16,36,409,0xFFFFFFFF);
    ui_overlay_text(UI_FONT_BOLD,12,60,411,UI_ALIGN_LEFT,0xD9EAFFFF,
        character_menu_camera?"Third-person camera: ON":"Third-person camera: OFF");
    ui_overlay_button(UI_BUTTON_A,17,36,447,0xFFFFFFFF);
    ui_overlay_text(UI_FONT_BOLD,12,60,450,UI_ALIGN_LEFT,0xD9EAFFFF,"Save");
    ui_overlay_button(UI_BUTTON_B,17,153,447,0xFFFFFFFF);
    ui_overlay_text(UI_FONT_BOLD,12,177,450,UI_ALIGN_LEFT,0xD9EAFFFF,"Cancel");
    ui_overlay_text(UI_FONT_REGULAR,12,602,450,UI_ALIGN_RIGHT,0xD9EAFFFF,character_menu_save_failed?"Could not save. Try again.":"D-pad / left stick: select");
}
#endif

/*
PLAYLIST_PROFILE.H

header included in hcex build.
*/

#ifndef __PLAYLIST_PROFILE_H
#define __PLAYLIST_PROFILE_H
#pragma once

/* ---------- headers */

#include "cseries/cseries.h"

/* ---------- constants */

/* ---------- macros */

/* ---------- structures */

struct game_variant;

/* ---------- prototypes/PLAYLIST_PROFILE.C */

void playlist_profiles_initialize(
	void);
void playlist_profiles_dispose(
	void);
word playlist_profile_number_of_default_profiles_on_disk(
	void);
boolean playlist_profile_get(
	long playlist_profile_index,
	struct game_variant *variant);
long playlist_profile_new(
	short local_player_index,
	wchar_t *name);
void playlist_profile_save(
	long playlist_profile_index,
	struct game_variant *variant);
/* port: a gametype's PC options (game_engine.h), and a gametype saved with
them */
struct game_variant_options;
boolean playlist_profile_get_options(
	long playlist_profile_index,
	struct game_variant_options *options);
/* Optional local-only mode metadata; old profiles default to Standard. */
boolean playlist_profile_get_native_mode(
	long playlist_profile_index, short *preset, short *matchup);
void playlist_profile_save_with_options_and_native_mode(
	long playlist_profile_index, struct game_variant *variant,
	struct game_variant_options const *options, short preset, short matchup);
void playlist_profile_save_with_options(
	long playlist_profile_index,
	struct game_variant *variant,
	struct game_variant_options const *options);
void playlist_profile_delete(
	long playlist_profile_index);
boolean playlist_profile_get_from_path(
	char *full_path,
	struct game_variant *variant);
boolean playlist_profile_get_display_name(
	long playlist_profile_index,
	wchar_t *display_name);
void playlist_profiles_enumerate_available_to_local_player_index(
	short local_player_index,
	word *number_of_profiles,
	long *playlist_profile_indices);

/* ---------- globals */

/* ---------- public code */

#endif // __PLAYLIST_PROFILE_H

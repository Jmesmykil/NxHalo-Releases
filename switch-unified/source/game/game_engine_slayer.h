/*
GAME_ENGINE_SLAYER.H
*/

#ifndef __GAME_ENGINE_SLAYER_H
#define __GAME_ENGINE_SLAYER_H
#pragma once

/* ---------- headers */

#include "game/game_engine.h"

/* ---------- prototypes/GAME_ENGINE_SLAYER.C */

void update_speed_for_score(
	long dead_player_index,
	long killing_player_index);
/* Native host-only mode integration: set a Free-for-All Slayer scoreboard value. */
void game_engine_slayer_set_player_score(long player_index, long score);

/* ---------- globals */

extern struct game_engine slayer_engine;

#endif // __GAME_ENGINE_SLAYER_H

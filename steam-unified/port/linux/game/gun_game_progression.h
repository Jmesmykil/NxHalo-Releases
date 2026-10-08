#ifndef GUN_GAME_PROGRESSION_H
#define GUN_GAME_PROGRESSION_H

/* Pure per-player progression state. The final stock weapon is rocket
   launcher; the next credited kill on that stage completes the ladder. */
enum
{
    GUN_GAME_WEAPON_STAGE_COUNT = 7,
    GUN_GAME_STAGE_COUNT = 7
};

struct gun_game_progression
{
    long player_datum;
    int stage;
    int won;
};

static inline void gun_game_progression_clear(struct gun_game_progression *state)
{
    state->player_datum = -1;
    state->stage = 0;
    state->won = 0;
}

static inline void gun_game_progression_bind(struct gun_game_progression *state, long player_datum)
{
    if (state->player_datum != player_datum)
    {
        state->player_datum = player_datum;
        state->stage = 0;
        state->won = 0;
    }
}

static inline int gun_game_progression_stage(const struct gun_game_progression *state)
{
    return state->stage;
}

static inline int gun_game_progression_complete(const struct gun_game_progression *state)
{
    return state->won;
}

static inline int gun_game_progression_score(const struct gun_game_progression *state)
{
    return state->stage + state->won;
}

static inline int gun_game_progression_credit_kill(struct gun_game_progression *state, int credited)
{
    if (!credited || state->won)
        return 0;
    if (state->stage < GUN_GAME_STAGE_COUNT - 1)
        state->stage++;
    else
        state->won = 1;
    return 1;
}

#endif

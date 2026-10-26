#ifndef HALO_LUNGE_CAPABILITY_STATE_H
#define HALO_LUNGE_CAPABILITY_STATE_H

/* Small transport-independent state used by network_distributed.c and its
focused lifecycle test. The no_player argument is the engine's NONE value. */
static int lunge_correction_within_limit(float dx, float dy, float dz, float maximum_distance)
{
	return maximum_distance >= 0.0f &&
		dx * dx + dy * dy + dz * dz <= maximum_distance * maximum_distance;
}

struct lunge_capability_host_state
{
	int valid;
	long player_index;
	long machine_index;
};

struct lunge_capability_client_state
{
	long player_index;
	long last_sent_tick;
	unsigned char attempts;
	int acknowledged;
};

static void lunge_capability_host_clear(struct lunge_capability_host_state *state, long none)
{
	state->valid = 0;
	state->player_index = none;
	state->machine_index = none;
}

static int lunge_capability_host_grant(struct lunge_capability_host_state *state,
	long player, long machine, int count, long no_player, int authenticated_stream,
	int current_machine_member, int remote_player, int not_quit)
{
	if (!state || count != 1 || player == no_player || machine < 0 ||
		!authenticated_stream || !current_machine_member || !remote_player || !not_quit)
		return 0;
	state->valid = 1;
	state->player_index = player;
	state->machine_index = machine;
	return 1;
}

static void lunge_capability_host_prune(struct lunge_capability_host_state *state,
	long machine, long current_player, int current_machine_member, long none)
{
	if (state && state->valid && state->machine_index == machine &&
		(!current_machine_member || current_player != state->player_index))
		lunge_capability_host_clear(state, none);
}

static int lunge_capability_host_supports(struct lunge_capability_host_state const *state,
	long player, long machine, long current_machine, int current_machine_member,
	int local_player, int quit_out_of_game)
{
	if (quit_out_of_game)
		return 0;
	if (local_player)
		return 1;
	return state && state->valid && state->player_index == player &&
		state->machine_index == machine && current_machine == machine &&
		current_machine_member;
}

static void lunge_capability_client_set_player(struct lunge_capability_client_state *state,
	long player, long none_tick)
{
	if (state->player_index != player)
	{
		state->player_index = player;
		state->last_sent_tick = none_tick;
		state->attempts = 0;
		state->acknowledged = 0;
	}
}

static int lunge_capability_client_should_send(struct lunge_capability_client_state const *state,
	long player, long now, long interval, unsigned char maximum_attempts, long no_player)
{
	if (!state || player == no_player || state->player_index != player ||
		state->acknowledged || state->attempts >= maximum_attempts)
		return 0;
	return state->last_sent_tick == no_player || now - state->last_sent_tick >= interval;
}

static void lunge_capability_client_mark_sent(struct lunge_capability_client_state *state, long now)
{
	state->last_sent_tick = now;
	if (state->attempts < 255)
		state->attempts++;
}

static int lunge_capability_client_ack(struct lunge_capability_client_state *state,
	long player, int authenticated_stream)
{
	if (!state || !authenticated_stream || state->player_index != player ||
		state->attempts == 0)
		return 0;
	state->acknowledged = 1;
	return 1;
}

static int lunge_capability_client_supports(struct lunge_capability_client_state const *state,
	long player, int local_player, int quit_out_of_game)
{
	return state && !quit_out_of_game && local_player &&
		state->player_index == player && state->acknowledged;
}

#endif

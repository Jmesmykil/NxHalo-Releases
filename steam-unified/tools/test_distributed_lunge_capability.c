#include <assert.h>
#include <stdio.h>
#include "../port/linux/game/lunge_capability_state.h"

#define NONE (-1L)

static void test_authenticated_host_grant_and_queries(void)
{
	struct lunge_capability_host_state state = { 0, NONE, NONE };

	assert(!lunge_capability_host_grant(&state, 0x1001, 2, 0, NONE, 1, 1, 1, 1));
	assert(!lunge_capability_host_grant(&state, NONE, 2, 1, NONE, 1, 1, 1, 1));
	assert(!lunge_capability_host_grant(&state, 0x1001, 2, 1, NONE, 0, 1, 1, 1));
	assert(!lunge_capability_host_grant(&state, 0x1001, 2, 1, NONE, 1, 0, 1, 1));
	assert(!lunge_capability_host_grant(&state, 0x1001, 2, 1, NONE, 1, 1, 0, 1));
	assert(!lunge_capability_host_grant(&state, 0x1001, 2, 1, NONE, 1, 1, 1, 0));
	assert(!state.valid);

	assert(lunge_capability_host_grant(&state, 0x1001, 2, 1, NONE, 1, 1, 1, 1));
	assert(lunge_capability_host_supports(&state, 0x1001, 2, 2, 1, 0, 0));
	assert(!lunge_capability_host_supports(&state, 0x1002, 2, 2, 1, 0, 0));
	assert(!lunge_capability_host_supports(&state, 0x1001, 3, 3, 1, 0, 0));
	assert(!lunge_capability_host_supports(&state, 0x1001, 2, 3, 1, 0, 0));
	assert(!lunge_capability_host_supports(&state, 0x1001, 2, 2, 0, 0, 0));
	assert(lunge_capability_host_supports(&state, 0x2001, NONE, NONE, 0, 1, 0));
	assert(!lunge_capability_host_supports(&state, 0x2001, NONE, NONE, 0, 1, 1));

	/* A removed or replaced full datum is cleared; unrelated machines are untouched. */
	lunge_capability_host_prune(&state, 3, 0x1002, 1, NONE);
	assert(state.valid);
	lunge_capability_host_prune(&state, 2, 0x1002, 1, NONE);
	assert(!state.valid && state.player_index == NONE);
	assert(lunge_capability_host_grant(&state, 0x1001, 2, 1, NONE, 1, 1, 1, 1));
	lunge_capability_host_prune(&state, 2, 0x1001, 0, NONE);
	assert(!state.valid);
}

static void test_client_retry_ack_and_lifecycle(void)
{
	struct lunge_capability_client_state state = { NONE, NONE, 0, 0 };

	lunge_capability_client_set_player(&state, 0x2001, NONE);
	assert(lunge_capability_client_should_send(&state, 0x2001, 0, 30, 8, NONE));
	lunge_capability_client_mark_sent(&state, 0);
	assert(!lunge_capability_client_should_send(&state, 0x2001, 29, 30, 8, NONE));
	assert(lunge_capability_client_should_send(&state, 0x2001, 30, 30, 8, NONE));
	assert(!lunge_capability_client_ack(&state, 0x2001, 0));
	assert(!lunge_capability_client_ack(&state, 0x2002, 1));
	assert(lunge_capability_client_ack(&state, 0x2001, 1));
	assert(!lunge_capability_client_should_send(&state, 0x2001, 60, 30, 8, NONE));
	assert(lunge_capability_client_supports(&state, 0x2001, 1, 0));
	assert(!lunge_capability_client_supports(&state, 0x2001, 0, 0));
	assert(!lunge_capability_client_supports(&state, 0x2001, 1, 1));

	/* Local player slot replacement clears prior acknowledgement and retry budget. */
	lunge_capability_client_set_player(&state, 0x3001, NONE);
	assert(state.attempts == 0 && !state.acknowledged && state.last_sent_tick == NONE);
	assert(!lunge_capability_client_supports(&state, 0x3001, 1, 0));
	assert(lunge_capability_client_should_send(&state, 0x3001, 100, 30, 8, NONE));
	lunge_capability_client_mark_sent(&state, 100);
	for (int i = 1; i < 8; i++)
	{
		assert(lunge_capability_client_should_send(&state, 0x3001, 100 + 30 * i, 30, 8, NONE));
		lunge_capability_client_mark_sent(&state, 100 + 30 * i);
	}
	assert(state.attempts == 8);
	assert(!lunge_capability_client_should_send(&state, 0x3001, 400, 30, 8, NONE));
	lunge_capability_client_set_player(&state, NONE, NONE);
	assert(!state.acknowledged && state.attempts == 0);
}

static void test_bounded_owner_correction(void)
{
	assert(lunge_correction_within_limit(3.0f, 0.0f, 0.0f, 3.0f));
	assert(lunge_correction_within_limit(1.0f, 2.0f, 2.0f, 3.0f));
	assert(!lunge_correction_within_limit(3.01f, 0.0f, 0.0f, 3.0f));
	assert(!lunge_correction_within_limit(0.0f, 0.0f, 0.0f, -1.0f));
}

int main(void)
{
	test_bounded_owner_correction();
	test_authenticated_host_grant_and_queries();
	test_client_retry_ack_and_lifecycle();
	puts("distributed lunge capability state: PASS");
	return 0;
}

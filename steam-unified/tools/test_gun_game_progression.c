#include <assert.h>
#include "../port/linux/game/gun_game_progression.h"

int main(void)
{
    struct gun_game_progression a, reused, late;
    int kill;
    gun_game_progression_clear(&a);
    gun_game_progression_bind(&a, 0x10001);
    assert(gun_game_progression_stage(&a) == 0);
    assert(gun_game_progression_score(&a) == 0);

    assert(!gun_game_progression_credit_kill(&a, 0));
    assert(gun_game_progression_stage(&a) == 0);
    assert(gun_game_progression_score(&a) == 0);
    for (kill = 1; kill <= 6; kill++)
    {
        assert(gun_game_progression_credit_kill(&a, 1));
        assert(gun_game_progression_stage(&a) == kill);
        assert(gun_game_progression_score(&a) == kill);
        assert(!gun_game_progression_complete(&a));
    }
    assert(gun_game_progression_stage(&a) == GUN_GAME_WEAPON_STAGE_COUNT - 1);
    assert(gun_game_progression_credit_kill(&a, 1));
    assert(gun_game_progression_complete(&a));
    assert(gun_game_progression_score(&a) == 7);
    assert(!gun_game_progression_credit_kill(&a, 1));

    /* Deaths do not touch this state; rebinding the same datum preserves it. */
    gun_game_progression_bind(&a, 0x10001);
    assert(gun_game_progression_complete(&a));

    /* A late join starts at pistol, and a recycled slot with a new datum resets. */
    gun_game_progression_clear(&late);
    gun_game_progression_bind(&late, 0x20001);
    assert(gun_game_progression_stage(&late) == 0);
    reused = a;
    gun_game_progression_bind(&reused, 0x30001);
    assert(gun_game_progression_stage(&reused) == 0);
    assert(!gun_game_progression_complete(&reused));

    /* Sequential multikills each advance exactly once. */
    for (kill = 1; kill <= 3; kill++)
        assert(gun_game_progression_credit_kill(&late, 1));
    assert(gun_game_progression_stage(&late) == 3);
    assert(gun_game_progression_score(&late) == 3);
    return 0;
}

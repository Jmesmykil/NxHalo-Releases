#ifndef HALO_NATIVE_VOICE_RUNTIME_H
#define HALO_NATIVE_VOICE_RUNTIME_H

#include <stddef.h>

/* Called on the game/input thread. PTT is explicit and menus always gate capture. */
void native_voice_runtime_update(int ptt_pressed, int menus_active);
/* Called by the SDL output mixer; adds only bounded current-session voice frames. */
void native_voice_runtime_mix(float *stereo, size_t frames);
/* Session/config teardown: close capture and purge all buffered audio. */
void native_voice_runtime_reset(void);

#endif

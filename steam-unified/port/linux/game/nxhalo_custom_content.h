#ifndef NXHALO_CUSTOM_CONTENT_H
#define NXHALO_CUSTOM_CONTENT_H
#include "cseries.h"
/* Host spawn selection: zero preserves the map standard. Resolved against loaded map tags only. */
long nxhalo_campaign_character_definition(long fallback_definition);
long nxhalo_multiplayer_character_definition(long fallback_definition);
/* Separate campaign and host selectors for the native overlay. */
short nxhalo_character_count(void);
char const *nxhalo_character_name(short choice);
long nxhalo_character_choice(boolean host_context);
boolean nxhalo_character_choice_set(boolean host_context, long choice);
void nxhalo_custom_reset(void);
void nxhalo_custom_update(void);
boolean nxhalo_custom_following(long unit_index);
#ifdef HALO_GAME_BROWSER
boolean nxhalo_character_menu_active(void);
void nxhalo_character_menu_opencontext(boolean host_context);
boolean nxhalo_character_menu_process(boolean available, boolean host_context);
void nxhalo_character_menu_render(boolean available, boolean host_context);
#endif
#endif

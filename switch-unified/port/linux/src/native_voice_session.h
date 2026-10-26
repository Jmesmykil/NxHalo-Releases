#ifndef HALO_NATIVE_VOICE_SESSION_H
#define HALO_NATIVE_VOICE_SESSION_H

/* Scalar-only bridge into the game ABI. Addresses and player slots are
 * copied from the active host roster; no game structures cross this boundary. */
int network_game_server_port_voice_roster(unsigned long *addresses, int *slots, int capacity);
int network_game_server_port_voice_sender_slot(unsigned long address, int controller, int *slot);
int network_game_port_voice_spatial(int speaker_slot, float listener[3], float right[3], float speaker[3]);
unsigned long network_game_port_voice_map_token(void);
/* True only during a live network game with a valid local player unit. */
int network_game_port_voice_session_active(void);
/* An admitted session, including its pregame and postgame lobby. */
int network_game_port_voice_session_connected(void);

#endif

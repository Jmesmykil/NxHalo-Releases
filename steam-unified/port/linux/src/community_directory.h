#ifndef HALO_LINUX_COMMUNITY_DIRECTORY_H
#define HALO_LINUX_COMMUNITY_DIRECTORY_H
#include "p2p.h"
struct community_directory_game { struct p2p_listing listing; int version; int source; char mode[32]; };
void community_directory_poll(void);
int community_directory_games(struct community_directory_game *out, int capacity);
const char *community_directory_status(void);
/* Cached offline snapshots of the Halo CE and Halo PC master lists. */
int community_directory_classic_games(struct community_directory_game *out, int capacity);
#endif

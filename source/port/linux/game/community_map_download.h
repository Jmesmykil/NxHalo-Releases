#ifndef COMMUNITY_MAP_DOWNLOAD_H
#define COMMUNITY_MAP_DOWNLOAD_H

enum community_map_download_state
{
	COMMUNITY_MAP_DOWNLOAD_IDLE,
	COMMUNITY_MAP_DOWNLOAD_RUNNING,
	COMMUNITY_MAP_DOWNLOAD_READY,
	COMMUNITY_MAP_DOWNLOAD_FAILED
};

int community_map_download_start(char const *name, char const *target);
int community_map_download_status(char *message, int size);
void community_map_download_clear(void);

#endif

#ifndef TEXTURE_PACK_H
#define TEXTURE_PACK_H

#include <stddef.h>

typedef void (*texture_pack_list_callback)(const char *name, void *context);

/* Import a directory of relative-tag-keyed PNG/TGA/DDS replacements. */
int texture_pack_install(const char *name, const char *source_directory);
int texture_pack_list(texture_pack_list_callback callback, void *context);
int texture_pack_select(const char *name);
int texture_pack_set_enabled(int enabled);
int texture_pack_enabled(void);
const char *texture_pack_selected(void);

/* Native renderer hook. 0 means use the game's original bitmap. */
unsigned int texture_pack_override(const char *tag_path, long bitmap_index,
	unsigned long *levels);

#endif

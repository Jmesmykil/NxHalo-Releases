/*
HALO_UI_MAP_LIST.H

The menus' list of multiplayer maps on the native builds
(port/linux/game/ui_map_list.c): the Xbox's thirteen, as they were, then the
Custom Edition maps, named with [CE], with Halo PC's names, descriptions and
pictures of them, then HaloMD's maps, named with [MD] (halo_map_families.h). The multiplayer map list, its rows and the
lobby (source/interface) ask it in place of the game's fixed thirteen.
*/

#ifndef HALO_UI_MAP_LIST_H
#define HALO_UI_MAP_LIST_H

struct bitmap_data;

/* a row's strings (ui_map_list_string_index) */
enum
{
	/* its name in the map list's narrow boxes: two lines, the name then [CE]
	or [MD] */
	_ui_map_list_string_name,
	_ui_map_list_string_description,
	/* its name on one line, for the lobby */
	_ui_map_list_string_lobby_name,
	NUMBER_OF_UI_MAP_LIST_STRINGS
};

/* Refresh disk inventory and playable picker. xbox_maps retains the original
thirteen string/picture indices even when some stock files are absent. */
void ui_map_list_refresh(char *const *xbox_maps);
long ui_map_list_count(void);
/* the rows' map names, for the list widget's items */
char **ui_map_list_names(void);
/* the row of a map's name, or NONE */
long ui_map_list_find(char const *map_name);
/* as ui_map_list_find, the list filled anew for a name it lacks
(ui_widget_event_handler_functions.c, which has the Xbox's names) */
long ui_map_list_lookup(char const *map_name);
/* a row's string list index for one of its strings: an Xbox map's own in
ui.map, or one of this list's (ui_map_list_text) */
short ui_map_list_string_index(long row, short kind);
/* a row's bitmap frame: an Xbox map's in ui.map, or one of this list's
(ui_map_list_picture) */
short ui_map_list_picture_index(long row);
/* the bitmap of a frame of this list's, or NULL for ui.map's */
struct bitmap_data *ui_map_list_picture(short frame_index);
/* the text of a string list index of this list's, or NULL for ui.map's */
wchar_t const *ui_map_list_text(short string_list_index);

/* a Custom Edition or HaloMD map (a family of halo_map_families.h), by its
file's name (the server browser's): its name (Halo PC's own, HaloMD's mod
list's, or the file's made readable; every build), Halo PC's picture of it
(or NULL), and whether its family's folders have it (builds with
HALO_CUSTOM_EDITION) */
void ui_map_list_family_name(short family, char const *file, wchar_t *name, long size);
struct bitmap_data *ui_map_list_family_picture(short family, char const *file);
boolean ui_map_list_preflight(char const *map_name);
boolean ui_map_list_family_present(short family, char const *file);


/* Installed files are independent of the online catalogue. Header compatibility
and dependency checks are NOT evidence of successful native runtime loading. */
#define UI_MAP_INVENTORY_LIMIT 4096
#define UI_MAP_INVENTORY_PAGE_SIZE 9
enum
{
    _ui_map_inventory_campaign = 0,
    _ui_map_inventory_multiplayer = 1,
    _ui_map_inventory_ui = 2,
    _ui_map_inventory_resource = 3,
    _ui_map_inventory_unknown = 4,
    _ui_map_inventory_family_unknown = 3,
    _ui_map_inventory_requirements_unchecked = 0,
    _ui_map_inventory_requirements_present = 1,
    _ui_map_inventory_requirements_failed = 2
};
struct ui_map_inventory_entry
{
    char map_name[64];       /* exact family-qualified engine name */
    char basename[64];       /* filename without .map or family suffix */
    char path[512];          /* actual enumerated file, including shadowed copies */
    char requirements[192];  /* header/location/dependency detail */
    unsigned long generation;
    unsigned int version;
    unsigned int file_size;
    unsigned int header_crc;
    short family;           /* halo_map_families, or family_unknown */
    short map_type;         /* enum above; filters accept NONE for All */
    short requirement_status;
    boolean valid_header;
    boolean routable;       /* this file is the engine resolver's exact copy */
};
long ui_map_inventory_count(void);
long ui_map_inventory_overflow(void); /* omitted files; never silently truncated */
long ui_map_inventory_filtered_count(void);
long ui_map_inventory_page(void);     /* zero based */
long ui_map_inventory_page_count(void); /* at least one, including empty view */
void ui_map_inventory_set_query(char const *query);
char const *ui_map_inventory_query(void);
void ui_map_inventory_filter_set(short family, short map_type);
short ui_map_inventory_family_filter(void);
short ui_map_inventory_type_filter(void);
boolean ui_map_inventory_page_move(int direction);
boolean ui_map_inventory_result(int page_row, struct ui_map_inventory_entry *entry);
char const *ui_map_inventory_status(void);
/* Snapshot-safe selection: NONE for stale/changed/non-MP/non-routable entries. */
long ui_map_inventory_resolve(struct ui_map_inventory_entry const *snapshot);
/* Snapshot-safe lazy dependency check; FALSE means stale/changed snapshot.
On TRUE inspect requirement_status/details; never enables campaign launch. */
boolean ui_map_inventory_check(struct ui_map_inventory_entry const *snapshot,
    struct ui_map_inventory_entry *details);

#endif

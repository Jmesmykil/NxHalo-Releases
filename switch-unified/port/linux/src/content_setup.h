#ifndef CONTENT_SETUP_H
#define CONTENT_SETUP_H
#include <stddef.h>
void content_setup_open_folder(void);
void content_setup_import_local(void);
void content_setup_download_clipboard(void);
int content_setup_download_map(const char *map_name);
char const *content_setup_status(void);
void content_setup_clear_status(void);
#define CONTENT_CATALOG_PAGE_SIZE 9
void content_setup_catalog_poll(void);
void content_setup_catalog_set_query(const char *query);
const char *content_setup_catalog_query(void);
void content_setup_catalog_category_cycle(int direction);
const char *content_setup_catalog_category(void);
void content_setup_catalog_install_filter_cycle(int direction);
const char *content_setup_catalog_install_filter(void);
void content_setup_catalog_sort_cycle(void);
const char *content_setup_catalog_sort(void);
int content_setup_catalog_page_move(int direction);
int content_setup_catalog_result(int row, char *name, size_t name_size, char *type, size_t type_size);
int content_setup_catalog_download_row(int row);
int content_setup_catalog_installed(const char *name);
const char *content_setup_catalog_status(void);
#define CONTENT_TEXTURE_PACK_PAGE_SIZE 12
void content_setup_texture_pack_import(void);
void content_setup_texture_pack_set_query(const char *query);
const char *content_setup_texture_pack_query(void);
int content_setup_texture_pack_page_move(int direction);
int content_setup_texture_pack_disable_selected(void);
int content_setup_texture_pack_stage_selected_removal(void);
int content_setup_texture_pack_removal_pending(void);
void content_setup_texture_pack_cancel_removal(void);
int content_setup_texture_pack_confirm_removal(void);
void content_setup_texture_pack_refresh(void);
int content_setup_texture_pack_count(void);
int content_setup_texture_pack_name(int row, char *name, size_t size);
int content_setup_texture_pack_select_row(int row);
int content_setup_texture_pack_toggle(void);
const char *content_setup_texture_pack_status(void);
int content_setup_deck_upscaling_available(void);

#endif

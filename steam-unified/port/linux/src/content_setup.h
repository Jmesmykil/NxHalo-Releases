#ifndef CONTENT_SETUP_H
#define CONTENT_SETUP_H
void content_setup_open_folder(void);
void content_setup_import_local(void);
void content_setup_download_clipboard(void);
int content_setup_download_map(const char *map_name);
char const *content_setup_status(void);
void content_setup_clear_status(void);
#endif

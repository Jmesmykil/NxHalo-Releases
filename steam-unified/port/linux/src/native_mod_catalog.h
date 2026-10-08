/* Optional authored native image packs. These are not DLL/Lua/OpenSauce mods. */
#ifndef NATIVE_MOD_CATALOG_H
#define NATIVE_MOD_CATALOG_H
#include <stddef.h>
#define NATIVE_MOD_CATALOG_PAGE_SIZE 9
#define NATIVE_MOD_CATALOG_MAX_ENTRIES 256
#define NATIVE_MOD_CATALOG_MAX_TEXT (1024U * 1024U)
#define NATIVE_MOD_CATALOG_MAX_ARCHIVE (64U * 1024U * 1024U)
#define NATIVE_MOD_CATALOG_MAX_UNPACKED (256U * 1024U * 1024U)
#define NATIVE_MOD_CATALOG_MAX_FILES 512
struct native_mod_catalog_entry {
 char id[32], version[32], pack_name[64], title[96], author[96], license[64];
 char source[512], archive[512], sha256[65], native_format[32], map_scope[256];
 unsigned int compressed_limit, unpacked_limit, file_count;
 int installed;
};
/* Poll from the main thread; starts catalogue fetch and reaps finished jobs. */
void native_mod_catalog_poll(void);
/* Explicit retry; last validated catalogue remains available on failure. */
void native_mod_catalog_refresh(void);
void native_mod_catalog_set_query(const char *query);
const char *native_mod_catalog_query(void);
int native_mod_catalog_page_move(int direction);
int native_mod_catalog_result(int row, struct native_mod_catalog_entry *entry);
/* Captures the displayed row; asynchronous, returns 1 only when started.
   Import never selects or enables the pack. */
int native_mod_catalog_install_row(int row);
/* Revalidates every captured detail/security field against the catalogue;
   the mutable installed flag is excluded. */
int native_mod_catalog_install_entry(const struct native_mod_catalog_entry *snapshot);
int native_mod_catalog_install_running(void);
const char *native_mod_catalog_status(void);
#ifdef NATIVE_MOD_CATALOG_TEST
int native_mod_catalog_test_parse(const char *text, size_t size,
 struct native_mod_catalog_entry *entries, int *count, char *error, size_t capacity);
int native_mod_catalog_test_extract(const unsigned char *zip, size_t size,
 const struct native_mod_catalog_entry *entry, const char *private_stage,
 char *error, size_t capacity);
int native_mod_catalog_test_entry_equal(const struct native_mod_catalog_entry *a,
 const struct native_mod_catalog_entry *b);
int native_mod_catalog_test_receipt(const struct native_mod_catalog_entry *entry,
 const char *private_stage, char *error, size_t capacity);
#endif
#endif

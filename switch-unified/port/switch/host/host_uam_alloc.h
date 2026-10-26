/*
HOST_UAM_ALLOC.H

The memory UAM's shader compiler allocates, a thread's own (host_uam_alloc.c
says why and how): UAM's calls to the C library's allocation functions and
C++'s new and delete are renamed to these when it is combined into the host
(and into the deko3d probe).
*/

#ifndef HOST_UAM_ALLOC_H
#define HOST_UAM_ALLOC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *host_uam_malloc(size_t size);
void host_uam_free(void *block);
void *host_uam_calloc(size_t count, size_t size);
void *host_uam_realloc(void *block, size_t size);
char *host_uam_strdup(const char *text);
char *host_uam_strndup(const char *text, size_t most);
void *host_uam_new(size_t size);
void host_uam_delete(void *block);
void *host_uam_new_array(size_t size);
void host_uam_delete_array(void *block);

/* a thread that stops compiling: its free blocks go to the pool, for the
threads that compile after it (host_uam_alloc.c) */
void host_uam_alloc_thread_end(void);

/* how many regions were taken from newlib, the bytes given to pages, and
the bytes of free blocks in the pool */
struct host_uam_alloc_figures
{
	unsigned regions;
	unsigned long bytes;
	unsigned long pooled;
};

void host_uam_alloc_figures(struct host_uam_alloc_figures *figures);

#ifdef __cplusplus
}
#endif

#endif

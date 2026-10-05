/*
HOST.H

Internals of the Switch port's host NRO. Adapted from the Android port's
host.h — the interface is identical; only the implementations of each
module use libnx instead of bionic/ART.

See port/android/README.md for the guest/host architecture (it applies
here without change) and port/switch/README.md for Switch specifics.
*/

#ifndef __HALO_SWITCH_HOST_H
#define __HALO_SWITCH_HOST_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

extern FILE *s_logFile;

#include "halo_switch_abi.h"

/* Community core requires 32 MiB for its image, including BSS. */
#define HOST_GUEST_IMAGE_LIMIT 0x8A000000ULL
#define HOST_GUEST_POOL_BASE HOST_GUEST_IMAGE_LIMIT
#define HOST_GUEST_POOL_SIZE (0x90000000ULL - HOST_GUEST_POOL_BASE)

/* ---------- logging (nxlink stdio) */

void host_logf(int priority, const char *format, ...) __attribute__((format(printf, 2, 3)));
void host_logf_buffered(int priority, const char *format, ...) __attribute__((format(printf, 2, 3)));
void host_log_flush(void);
uint64_t host_log_dropped_batches(void);
void host_log_write_bytes(int fd, const char *bytes, size_t length);
#define HOST_LOG_INFO 4
#define HOST_LOG_WARN 5
#define HOST_LOG_ERROR 6

void host_exit(int code) __attribute__((noreturn));
int host_errno(void);
void host_fatal(const char *format, ...) __attribute__((format(printf, 1, 2), noreturn));

/* Exact writable-file routing; returns either path or a static host path. */
const char *host_guest_file_path(const char *path);

/* ---------- guest memory (host_memory.c)

All guest pointers are below 4 GiB. CodeMemory provides fixed writable
data ranges and a separate writable alias for the executable image. */

int host_memory_initialize(uint32_t image_base, uint32_t image_size, uint32_t code_size);
int host_native_memory_initialize(uint32_t image_base, uint32_t image_size, uint32_t code_size, uint32_t xbox_base);
void *host_native_image_pointer(uint32_t address, size_t size);
void host_native_image_flush(void);
void *host_low_backing_allocate(size_t size);
void host_low_backing_free(void *address, size_t size);
void *host_low_map(size_t size, int protection);
void host_low_unmap(void *address, size_t size);
int host_low_owns(uintptr_t address, size_t size);

/* the guest's mmap/munmap/mprotect (via syscall dispatch) */
long host_guest_mmap(uint64_t address, uint64_t size, int protection,
                     int flags, int fd, int64_t offset);
long host_guest_munmap(uint64_t address, uint64_t size);
long host_guest_mprotect(uint64_t address, uint64_t size, int protection);

/* write tracking (GPU texture cache coherency) */
void host_install_fault_handler(void);
void host_memory_watch_initialize(void);
void host_memory_watch_protect(uint32_t address, uint32_t size);
uint32_t host_memory_watch_serial(void);
uint32_t host_memory_watch_generation(uint32_t address, uint32_t size);
void host_memory_watch_prepare_write(uint32_t address, uint32_t size);
void host_memory_watch_forget(uint32_t address, uint32_t size);
void host_memory_watch_log_stats(uint32_t frame, uint64_t elapsed_ns);
void host_memory_crc_benchmark(void);
void host_gl_log_stats(uint32_t frame);
void host_phase_log_stats(uint32_t frame);
void host_gl_log_capabilities(void);

/* ---------- the guest image (host_loader.c) */

struct host_guest_image
{
	const struct halo_guest_header *header;
	uint32_t base, end;
};

extern struct host_guest_image host_image;

int host_load_image(const void *elf, size_t size);

/* ---------- entering guest code (host_thread.c) */

uint32_t host_call_guest(uint32_t function, uint32_t a, uint32_t b,
                         uint32_t c, uint32_t d);
int host_native_thread_create(void *(*function)(void *), void *argument,
                              size_t stack_size);
void host_run_guest_main(uint32_t boot) __attribute__((noreturn));

uint32_t host_get_tp(void);
void host_set_tp(uint32_t thread);
int host_thread_create(uint32_t guest_thread, uint32_t stack_size);

/* ---------- debugging (host_debug.c) */

void host_debug_thread_started(void);
void host_debug_thread_exited(void);
void host_debug_start_sampler(const char *setting);

/* Number of guest map-alias IPC requests staged in compatible host memory. */
uint64_t host_ipc_staged_requests(void);

/* ---------- import table */

void *host_resolve_import(const char *name);

/* ---------- SDL / GL (host_sdl.c, host_gl.c) */

void *host_gl_resolve(const char *name);

#endif

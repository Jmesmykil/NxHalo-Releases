/*
HOST_POSIX.C — Nintendo Switch

Implements the posix_* functions required by the guest runtime (posix.h),
which the guest imports via hostposix_<name>. Provides file system
operations via newlib/libnx, hardware entropy via randomGet, and safe
socket helpers in host_net.c.
*/

#include "host.h"
#include "host_file_io.h"

#include <switch.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>

#include "posix.h"
#include "host_phase_profile.h"
#include "host_stall_profile.h"
#include "host_checkpoint_profile.h"
#include "host_frame_bridge.h"
static struct host_frame_bridge frame_bridge;
uint32_t host_frame_bridge_next(void) { return host_frame_bridge_take(&frame_bridge); }
void host_frame_bridge_report(uint32_t frame)
{
    host_logf_buffered(HOST_LOG_INFO,"frame_bridge frame=%u received=%llu rejected=%llu overwritten=%llu unmapped=%llu",
        frame,(unsigned long long)frame_bridge.received,(unsigned long long)frame_bridge.rejected,
        (unsigned long long)frame_bridge.overwritten,(unsigned long long)frame_bridge.unmapped);
    host_log_flush();
}

/* ---------- logging ---------- */

/* HC1 can arrive on checkpoint workers; only the rendering thread formats the
 * async batch. Hold this independent mutex for bounded copies/counters only. */
static struct host_checkpoint_state checkpoint_state;
static Mutex checkpoint_lock;
static struct host_stall_state stall_state;
static struct host_phase_state phase_state;
static struct host_phase_state extra_phase_state;

void host_log(int priority, const char *text)
{
    /* Consume malformed reserved packets too: never fall through to ordinary
     * synchronous error logging with this deliberately large priority. */
    if (priority == HOST_PHASE_PRIORITY) {
        if (text && !strncmp(text,"HF1",3)) host_frame_bridge_receive(&frame_bridge,text);
        else if (text && !strncmp(text,"HC1",3)) {
            mutexLock(&checkpoint_lock);
            host_checkpoint_receive(&checkpoint_state,text);
            mutexUnlock(&checkpoint_lock);
        }
        else if (text && !strncmp(text, "HP3", 3))
            host_stall_receive(&stall_state,text);
        else if (text && !strncmp(text, "HP2", 3))
            host_extra_phase_receive(&extra_phase_state, text);
        else
            host_phase_receive(&phase_state, text);
        return;
    }
    host_logf(priority, "%s", text);
}

void host_phase_log_stats(uint32_t frame)
{
    uint32_t guest_frame = 0;
    const char *packet = host_phase_drain(&phase_state, frame, &guest_frame);
    if (packet)
        host_logf_buffered(HOST_LOG_INFO, "phase frame=%u guest=%u %s", frame, guest_frame, packet);
    if (phase_state.rejected || phase_state.mismatched || phase_state.overwritten)
        host_logf_buffered(HOST_LOG_INFO, "phase_rx frame=%u rejected=%llu differing_boundaries=%llu overwritten=%llu",
            frame, (unsigned long long)phase_state.rejected,
            (unsigned long long)phase_state.mismatched, (unsigned long long)phase_state.overwritten);
}

/* Drained in its own async batch after the original report flush. */
void host_extra_phase_log_stats(uint32_t frame)
{
    uint32_t guest_frame = 0;
    const char *packet = host_phase_drain(&extra_phase_state, frame, &guest_frame);
    if (packet)
        host_logf_buffered(HOST_LOG_INFO, "phase_extra frame=%u guest=%u %s", frame, guest_frame, packet);
    if (extra_phase_state.rejected || extra_phase_state.mismatched || extra_phase_state.overwritten)
        host_logf_buffered(HOST_LOG_INFO, "phase_extra_rx frame=%u rejected=%llu differing_boundaries=%llu overwritten=%llu",
            frame, (unsigned long long)extra_phase_state.rejected,
            (unsigned long long)extra_phase_state.mismatched, (unsigned long long)extra_phase_state.overwritten);
}

/* Three guest pre-swap intervals drain independently of HP1/HP2 batches. */
void host_stall_log_stats(uint32_t frame)
{
    unsigned count=host_stall_take(&stall_state,frame);
    for(unsigned i=0;i<count;i++)host_logf_buffered(HOST_LOG_INFO,"stall frame=%u %s",frame,stall_state.packet[i]);
    if(stall_state.rejected || stall_state.mismatched)host_logf_buffered(HOST_LOG_INFO,
        "stall_rx frame=%u rejected=%llu mismatched=%llu",frame,
        (unsigned long long)stall_state.rejected,(unsigned long long)stall_state.mismatched);
    /* Shares the already isolated HP3 async batch: eight bounded HC1 rows plus
     * counters fit alongside three HP3 rows in 4096 bytes. No disk logging
     * while receiving a checkpoint or holding the receiver mutex. */
    struct host_checkpoint_batch checkpoints;
    mutexLock(&checkpoint_lock);
    host_checkpoint_take(&checkpoint_state,&checkpoints);
    mutexUnlock(&checkpoint_lock);
    for(unsigned i=0;i<checkpoints.count;i++)
        host_logf_buffered(HOST_LOG_INFO,"checkpoint hostframe=%u %s",frame,checkpoints.packet[i]);
    if(checkpoints.accepted||checkpoints.dropped||checkpoints.rejected)
        host_logf_buffered(HOST_LOG_INFO,"checkpoint_rx hostframe=%u accepted=%llu drained=%llu dropped=%llu rejected=%llu",frame,
            (unsigned long long)checkpoints.accepted,(unsigned long long)checkpoints.drained,
            (unsigned long long)checkpoints.dropped,(unsigned long long)checkpoints.rejected);
}

/* ---------- writable application files ---------- */

const char *host_guest_file_path(const char *path)
{
    const char *name;
    /* Only these application files move out of the bundled data root.
     * Maps/assets and the existing save-container paths retain their paths. */
    if (!path || strncmp(path, "romfs:", 6))
        return path;
    name = path + 6;
    if (*name == '/') name++;
    if (!strcmp(name, "config.toml")) return "sdmc:/switch/halo/config.toml";
    if (!strcmp(name, "debug.txt")) return "sdmc:/switch/halo/debug.txt";
    if (!strcmp(name, "gamestate.txt")) return "sdmc:/switch/halo/gamestate.txt";
    return path;
}

/* ---------- file helpers ---------- */

static void split64(unsigned long long value, posix_ulong *low, posix_ulong *high)
{
	*low = (posix_ulong)(value & 0xffffffffULL);
	*high = (posix_ulong)(value >> 32);
}

static void fill_information(const struct stat *st, struct posix_file_information *information)
{
	memset(information, 0, sizeof(*information));
	if (S_ISDIR(st->st_mode))
		information->flags |= _posix_file_is_directory;
	if (!(st->st_mode & S_IWUSR))
		information->flags |= _posix_file_is_read_only;
	split64((unsigned long long)st->st_size, &information->size_low, &information->size_high);
	information->modification_seconds = (posix_ulong)st->st_mtim.tv_sec;
	information->modification_nanoseconds = (posix_ulong)st->st_mtim.tv_nsec;
	information->access_seconds = (posix_ulong)st->st_atim.tv_sec;
	information->access_nanoseconds = (posix_ulong)st->st_atim.tv_nsec;
	information->creation_seconds = (posix_ulong)st->st_ctim.tv_sec;
	information->creation_nanoseconds = (posix_ulong)st->st_ctim.tv_nsec;
}

int posix_stat(const char *path, struct posix_file_information *information)
{
	struct stat st;
	if (stat(host_guest_file_path(path), &st) != 0)
		return -1;
	fill_information(&st, information);
	return 0;
}

int posix_fstat(int descriptor, struct posix_file_information *information)
{
	struct stat st;
	if (host_file_fstat(descriptor, &st) != 0)
		return -1;
	fill_information(&st, information);
	return 0;
}

int posix_set_file_times(const char *path,
	posix_ulong access_seconds, posix_ulong access_nanoseconds,
	posix_ulong modification_seconds, posix_ulong modification_nanoseconds)
{
	(void)path;
	(void)access_seconds;
	(void)access_nanoseconds;
	(void)modification_seconds;
	(void)modification_nanoseconds;
	return 0;
}

int posix_seek(int descriptor, posix_long offset_low, posix_long offset_high, int whence,
	posix_ulong *position_low, posix_ulong *position_high)
{
	off_t offset = (off_t)(((unsigned long long)(posix_ulong)offset_high << 32) | (posix_ulong)offset_low);
	off_t result = host_file_seek(descriptor, offset, whence);
	if (result == (off_t)-1)
		return -1;
	split64((unsigned long long)result, position_low, position_high);
	return 0;
}

int posix_truncate(int descriptor, posix_ulong size_low, posix_ulong size_high)
{
	return host_file_truncate(descriptor, (off_t)(((unsigned long long)size_high << 32) | size_low));
}

int posix_disk_space(const char *path,
	posix_ulong *free_low, posix_ulong *free_high,
	posix_ulong *total_low, posix_ulong *total_high)
{
	struct statvfs st;
	if (statvfs(path, &st) != 0)
	{
		/* Fallback: report 4 GB free, 32 GB total */
		split64(4ULL * 1024 * 1024 * 1024, free_low, free_high);
		split64(32ULL * 1024 * 1024 * 1024, total_low, total_high);
		return 0;
	}
	split64((unsigned long long)st.f_bavail * st.f_frsize, free_low, free_high);
	split64((unsigned long long)st.f_blocks * st.f_frsize, total_low, total_high);
	return 0;
}

int posix_set_read_only(const char *path, int read_only)
{
	(void)path;
	(void)read_only;
	return 0;
}

int posix_make_directory(const char *path)
{
	return mkdir(path, 0777);
}

/* Directory enumeration with small 32-bit handles */
#define DIRECTORY_HANDLE_COUNT 64

static DIR *directory_handles[DIRECTORY_HANDLE_COUNT];
static Mutex directory_handle_lock;
static int directory_handle_lock_init = 0;

static void *directory_handle_new(DIR *directory)
{
	unsigned long index;
	if (!directory)
		return NULL;
	if (!directory_handle_lock_init)
	{
		mutexInit(&directory_handle_lock);
		directory_handle_lock_init = 1;
	}
	mutexLock(&directory_handle_lock);
	for (index = 0; index < DIRECTORY_HANDLE_COUNT; index++)
	{
		if (!directory_handles[index])
		{
			directory_handles[index] = directory;
			mutexUnlock(&directory_handle_lock);
			return (void *)(index + 1);
		}
	}
	mutexUnlock(&directory_handle_lock);
	closedir(directory);
	return NULL;
}

static DIR *directory_from_handle(void *handle, int release)
{
	unsigned long index = (unsigned long)handle - 1;
	DIR *directory = NULL;
	if (index >= DIRECTORY_HANDLE_COUNT)
		return NULL;
	if (!directory_handle_lock_init)
	{
		mutexInit(&directory_handle_lock);
		directory_handle_lock_init = 1;
	}
	mutexLock(&directory_handle_lock);
	directory = directory_handles[index];
	if (release)
		directory_handles[index] = NULL;
	mutexUnlock(&directory_handle_lock);
	return directory;
}

void *posix_directory_open(const char *path)
{
	return directory_handle_new(opendir(path));
}

int posix_directory_next(void *directory, char *name, posix_ulong name_size)
{
	DIR *stream = directory_from_handle(directory, 0);
	struct dirent *entry;
	if (!stream)
		return 0;
	while ((entry = readdir(stream)) != NULL)
	{
		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
			continue;
		if (strlen(entry->d_name) + 1 > name_size)
			continue;
		strcpy(name, entry->d_name);
		return 1;
	}
	return 0;
}

void posix_directory_close(void *directory)
{
	DIR *stream = directory_from_handle(directory, 1);
	if (stream)
		closedir(stream);
}

int posix_find_entry_case_insensitive(const char *directory, const char *name,
	char *result, posix_ulong result_size)
{
	DIR *handle = opendir(*directory ? directory : ".");
	struct dirent *entry;
	int found = 0;
	if (!handle)
		return 0;
	while ((entry = readdir(handle)) != NULL)
	{
		if (!strcasecmp(entry->d_name, name) && strlen(entry->d_name) + 1 <= result_size)
		{
			strcpy(result, entry->d_name);
			found = 1;
			break;
		}
	}
	closedir(handle);
	return found;
}

/* ---------- sockets & crypto ---------- */

void posix_random_bytes(void *buffer, posix_ulong size)
{
	randomGet(buffer, size);
}

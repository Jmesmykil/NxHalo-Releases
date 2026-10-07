/*
HOST_SYSCALL.C — Nintendo Switch

System call translation for the guest. The guest's musl libc makes Linux
system calls through stubs that call host_syscall(). On Android these went
to real Linux system calls. On the Switch, we map them to libnx/newlib
equivalents.

The guest only uses a subset of system calls:
- File I/O: open, read, write, close, lseek, stat, mkdir, unlink, rename
- Memory: mmap, munmap, mprotect (→ host_memory.c)
- Time: clock_gettime, nanosleep, gettimeofday
- Threading: futex (→ libnx Mutex/CondVar), clone (→ host_thread.c)
- Process: exit, getpid, gettid
- Other: getrandom, uname, sched_yield
*/

#include "host.h"
#include "host_sysinfo_guest.h"

#include <switch.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include "host_file_io.h"

/* Authoritative fixed-width kernel stat ABI read by the ILP32 guest.
 * Native libc reserves the header's padding name as an attribute macro. */
#pragma push_macro("__unused")
#undef __unused
#include "../../android/guest/libc/arch/arm64_32/kstat.h"
#pragma pop_macro("__unused")

/* Guest pointer helper: zero-extends a 32-bit guest address */
#define GUEST(type, address) ((type)(uintptr_t)(uint32_t)(address))

/* Linux syscall numbers (AArch64) that the guest uses */
#define __NR_read          63
#define __NR_write         64
#define __NR_readv         65
#define __NR_writev        66
#define __NR_openat        56
#define __NR_close         57
#define __NR_lseek         62
#define __NR_pread64       67
#define __NR_pwrite64      68
#define __NR_fstat         80
#define __NR_newfstatat    79
#define __NR_exit          93
#define __NR_exit_group    94
#define __NR_clock_gettime 113
#define __NR_nanosleep     101
#define __NR_gettimeofday  169
#define __NR_getpid        172
#define __NR_gettid        178
#define __NR_sysinfo       179
#define __NR_sched_yield   124
#define __NR_getrandom     278
#define __NR_mmap          222
#define __NR_munmap        215
#define __NR_mprotect      226
#define __NR_madvise       233
#define __NR_brk           214
#define __NR_mremap        216
#define __NR_set_tid_address 96
#define __NR_rt_sigaction  134
#define __NR_rt_sigprocmask 135
#define __NR_sigaltstack   132
#define __NR_mkdirat       34
#define __NR_unlinkat      35
#define __NR_renameat      38
#define __NR_renameat2     276
#define __NR_fcntl         25
#define __NR_ftruncate     46
#define __NR_fsync         82
#define __NR_fdatasync     83
#define __NR_getcwd        17
#define __NR_chdir         49
#define __NR_dup           23
#define __NR_pipe2         59
#define __NR_getuid        174
#define __NR_geteuid       175
#define __NR_getgid        176
#define __NR_getegid       177
#define __NR_umask         166
#define __NR_uname         160
#define __NR_fchmod        52
#define __NR_fchmodat      53
#define __NR_futex         98
#define __NR_clone         220
#define __NR_ioctl         29
#define __NR_faccessat     48
#define __NR_readlinkat    78
#define __NR_getdents64    61
#define __NR_flock         32
#define __NR_prlimit64     261
#define __NR_getrlimit     163
#define __NR_kill          129
#define __NR_tkill         130
#define __NR_tgkill        131
#define __NR_membarrier    283
#define __NR_getppid       173
#define __NR_clock_getres  114
#define __NR_clock_nanosleep 115
#define __NR_ppoll         73
#define __NR_utimensat     88

/* Standard output uses the host's shared output-lifetime lock. */
static void log_bytes(int fd, const char *bytes, size_t size)
{
    host_log_write_bytes(fd, bytes, size);
}

static int translate_open_flags(int linux_flags)
{
	int newlib_flags = 0;
	int acc = linux_flags & 3;
	if (acc == 1) newlib_flags |= O_WRONLY;
	else if (acc == 2) newlib_flags |= O_RDWR;
	else newlib_flags |= O_RDONLY;

	if (linux_flags & 0x0040) newlib_flags |= O_CREAT;
	if (linux_flags & 0x0080) newlib_flags |= O_EXCL;
	if (linux_flags & 0x0200) newlib_flags |= O_TRUNC;
	if (linux_flags & 0x0400) newlib_flags |= O_APPEND;

	return newlib_flags;
}

/* ---------- guest application file opens ---------- */

static int host_open_guest_file(const char *path, int linux_flags, mode_t mode)
{
    const char *routed = host_guest_file_path(path);
    int flags = translate_open_flags(linux_flags);
    int fd = host_file_open(routed, flags, mode);
    int error_number = errno;
    /* A packaged default may be read on first launch. Only ENOENT and a
     * pure read permit fallback; writes always use the writable SD path. */
    if (fd < 0 && error_number == ENOENT && routed != path &&
        !(flags & O_CREAT) && (flags & O_ACCMODE) == O_RDONLY)
    {
        fd = host_file_open(path, flags, mode);
        error_number = errno;
    }
    if (fd < 0)
    {
        host_logf(HOST_LOG_WARN, "openat('%s', linux=0x%x -> newlib=0x%x) failed: errno=%d",
            routed, linux_flags, flags, error_number);
        /* Log I/O can alter errno. Preserve the actual guest file error. */
        errno = error_number;
    }
    return fd;
}

/* ---------- guest file metadata ---------- */

_Static_assert(sizeof(struct kstat) == 128, "AArch64 kernel stat size");
_Static_assert(offsetof(struct kstat, st_mode) == 16, "AArch64 kernel stat mode");
_Static_assert(offsetof(struct kstat, st_size) == 48, "AArch64 kernel stat size field");

static void host_store_guest_stat(void *target, const struct stat *value)
{
    struct kstat guest = {
        .st_dev = value->st_dev,
        .st_ino = value->st_ino,
        .st_mode = value->st_mode,
        .st_nlink = value->st_nlink,
        .st_uid = value->st_uid,
        .st_gid = value->st_gid,
        .st_rdev = value->st_rdev,
        .st_size = value->st_size,
        .st_blksize = value->st_blksize,
        .st_blocks = value->st_blocks,
        .st_atime_sec = value->st_atim.tv_sec,
        .st_atime_nsec = value->st_atim.tv_nsec,
        .st_mtime_sec = value->st_mtim.tv_sec,
        .st_mtime_nsec = value->st_mtim.tv_nsec,
        .st_ctime_sec = value->st_ctim.tv_sec,
        .st_ctime_nsec = value->st_ctim.tv_nsec,
    };
    /* The newlib layout differs from the kernel layout in sizes and offsets.
     * Copy a constructed guest object, including zeroed padding, exactly. */
    memcpy(target, &guest, sizeof(guest));
}

/* ---------- process memory diagnostics ---------- */

static long long host_sysinfo(uint64_t address)
{
    struct host_guest_sysinfo guest;
    uint64_t total = 0, used = 0;
    const size_t size = sizeof(guest);
    if (!host_sysinfo_address_valid(address) || !host_low_owns((uintptr_t)address, size))
        return -EFAULT;
    /* Ownership alone also includes the RX image. Validate every intersected
     * Horizon region before writing, including a destination spanning regions. */
    uint64_t cursor = address, end = address + size;
    while (cursor < end) {
        MemoryInfo memory;
        u32 page;
        if (R_FAILED(svcQueryMemory(&memory, &page, cursor)) ||
            !(memory.perm & Perm_W) || memory.addr > cursor ||
            cursor - memory.addr >= memory.size)
            return -EFAULT;
        uint64_t remaining = memory.size - (cursor - memory.addr);
        cursor += remaining < end - cursor ? remaining : end - cursor;
    }
    int total_valid = R_SUCCEEDED(svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0));
    int used_valid = R_SUCCEEDED(svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0));
    host_sysinfo_fill(&guest, armGetSystemTick() / UINT64_C(19200000), total, used,
        total_valid, used_valid);
    memcpy((void *)(uintptr_t)address, &guest, size);
    return 0;
}

/* ---------- clocks ---------- */

static clockid_t translate_clockid(int linux_clk)
{
	switch (linux_clk)
	{
	case 0: return CLOCK_REALTIME;  /* Linux CLOCK_REALTIME (0) -> newlib CLOCK_REALTIME (1) */
	case 1: return CLOCK_MONOTONIC; /* Linux CLOCK_MONOTONIC (1) -> newlib CLOCK_MONOTONIC (4) */
	default: return CLOCK_MONOTONIC;
	}
}

long long host_syscall(long long number, long long a, long long b,
                       long long c, long long d, long long e, long long f)
{

	switch (number)
	{
	/* --- File I/O: mapped to newlib/fsdev --- */
	case __NR_read:
	{
		ssize_t ret = host_file_read((int)a, GUEST(void *, b), (size_t)(uint32_t)c);
		return ret < 0 ? -errno : ret;
	}
	case __NR_write:
	{
		if (a == 1 || a == 2)
		{
			log_bytes((int)a, GUEST(const char *, b),
				(size_t)(uint32_t)c);
			return (uint32_t)c;
		}
		ssize_t ret = host_file_write((int)a, GUEST(const void *, b),
			(size_t)(uint32_t)c);
		return ret < 0 ? -errno : ret;
	}
	case __NR_readv:
	{
		/* struct iovec in ILP32 AArch64: uint32_t iov_base, uint32_t iov_len */
		const uint32_t *iov = GUEST(const uint32_t *, b);
		int vcnt = (int)c;
		ssize_t total_read = 0;
        host_file_io_lock();
		int vi;
		for (vi = 0; vi < vcnt; vi++)
		{
			char *base = GUEST(char *, iov[vi * 2]);
			size_t len = (size_t)iov[vi * 2 + 1];
			if (len == 0) continue;
			ssize_t ret = read((int)a, base, len);
			if (ret < 0) { long long result = total_read > 0 ? total_read : -errno; host_file_io_unlock(); return result; }
			total_read += ret;
			if ((size_t)ret < len) break;
		}
		host_file_io_unlock();
        return total_read;
	}
	case __NR_writev:
	{
		/* struct iovec in ILP32 AArch64: uint32_t iov_base, uint32_t iov_len */
		const uint32_t *iov = GUEST(const uint32_t *, b);
		int vcnt = (int)c;
		ssize_t total_written = 0;
        int file_operation = a != 1 && a != 2;
        if (file_operation) host_file_io_lock();
		int vi;
		for (vi = 0; vi < vcnt; vi++)
		{
			const char *base = GUEST(const char *, iov[vi * 2]);
			size_t len = (size_t)iov[vi * 2 + 1];
			if (len == 0) continue;
			if (a == 1 || a == 2)
			{
				log_bytes((int)a, base, len);
				total_written += len;
			}
			else
			{
				ssize_t ret = write((int)a, base, len);
				if (ret < 0) { long long result = total_written > 0 ? total_written : -errno; host_file_io_unlock(); return result; }
				total_written += ret;
				if ((size_t)ret < len) break;
			}
		}
		if (file_operation) host_file_io_unlock();
        return total_written;
	}

	case __NR_openat:
	{
		const char *path = GUEST(const char *, b);
		int fd = host_open_guest_file(path, (int)c, (mode_t)d);
		return fd < 0 ? -errno : fd;
	}
	case __NR_close:
	{
		int ret = host_file_close((int)a);
		return ret < 0 ? -errno : ret;
	}
	case __NR_lseek:
	{
		off_t ret = host_file_seek((int)a, (off_t)b, (int)c);
		return ret == (off_t)-1 ? -errno : ret;
	}
	case __NR_pread64:
	{
		ssize_t ret = host_file_pread((int)a, GUEST(void *, b), (size_t)(uint32_t)c, (off_t)d);
		return ret < 0 ? -errno : ret;
	}
	case __NR_pwrite64:
	{
		ssize_t ret = host_file_pwrite((int)a, GUEST(const void *, b), (size_t)(uint32_t)c, (off_t)d);
		return ret < 0 ? -errno : ret;
	}
	case __NR_mkdirat:
	{
		const char *path = GUEST(const char *, b);
		int ret = mkdir(path, (mode_t)c);
		return ret < 0 ? -errno : ret;
	}
	case __NR_unlinkat:
	{
		const char *path = GUEST(const char *, b);
		int ret = unlink(host_guest_file_path(path));
		return ret < 0 ? -errno : ret;
	}
	case __NR_renameat:
	case __NR_renameat2:
	{
		int ret = rename(host_guest_file_path(GUEST(const char *, b)),
			host_guest_file_path(GUEST(const char *, d)));
		return ret < 0 ? -errno : ret;
	}
	case __NR_ftruncate:
	{
		int ret = host_file_truncate((int)a, (off_t)b);
		return ret < 0 ? -errno : ret;
	}
	case __NR_fsync:
	case __NR_fdatasync:
	{
		int ret = host_file_sync((int)a);
		return ret < 0 ? -errno : ret;
	}
	case __NR_getcwd:
	{
		char *result = getcwd(GUEST(char *, a), (size_t)(uint32_t)b);
		return result ? 0 : -errno;
	}
	case __NR_chdir:
	{
		int ret = chdir(GUEST(const char *, a));
		return ret < 0 ? -errno : ret;
	}
	case __NR_dup:
	{
		int ret = host_file_dup((int)a);
		return ret < 0 ? -errno : ret;
	}
	case __NR_fcntl:
	{
		int ret = host_file_fcntl((int)a, (int)b, (int)c);
		return ret < 0 ? -errno : ret;
	}
	case __NR_fchmod:
	case __NR_fchmodat:
		return 0; /* no permissions on Switch FS */
	case __NR_faccessat:
	{
		int ret = access(host_guest_file_path(GUEST(const char *, b)), (int)c);
		return ret < 0 ? -errno : ret;
	}
	case __NR_flock:
		return 0; /* no file locking on Switch */
	case __NR_umask:
		return 0;

	/* --- fstat/newfstatat: fixed-width guest kernel ABI --- */
	case __NR_fstat:
	{
		struct stat st;
		int result = host_file_fstat((int)a, &st);
		if (result == 0 && b)
			host_store_guest_stat(GUEST(void *, b), &st);
		return result < 0 ? -errno : result;
	}
	case __NR_newfstatat:
	{
		struct stat st;
		int result = stat(host_guest_file_path(GUEST(const char *, b)), &st);
		if (result == 0 && c)
			host_store_guest_stat(GUEST(void *, c), &st);
		return result < 0 ? -errno : result;
	}

	/* --- Memory (→ host_memory.c) --- */
	case __NR_mmap:
		return host_guest_mmap((uint64_t)a, (uint64_t)b, (int)c,
			(int)d, (int)e, f);
	case __NR_munmap:
		return host_guest_munmap((uint64_t)a, (uint64_t)b);
	case __NR_mprotect:
		return host_guest_mprotect((uint64_t)a, (uint64_t)b, (int)c);
	case __NR_madvise:
		return 0; /* advisory only */
	case __NR_brk:
	case __NR_mremap:
		return -ENOMEM; /* musl falls back to mmap */

	/* --- Time --- */
	case __NR_clock_gettime:
	{
		struct timespec ts;
		clock_gettime(translate_clockid((int)a), &ts);
		if (b)
		{
			/* Guest timespec is {int32_t sec, int32_t nsec} */
			int32_t *guest_ts = GUEST(int32_t *, b);
			guest_ts[0] = (int32_t)ts.tv_sec;
			guest_ts[1] = (int32_t)ts.tv_nsec;
		}
		return 0;
	}
	case __NR_clock_getres:
	{
		if (b)
		{
			int32_t *guest_ts = GUEST(int32_t *, b);
			guest_ts[0] = 0;
			guest_ts[1] = 1000; /* 1 microsecond */
		}
		return 0;
	}
	case __NR_nanosleep:
	{
		if (a)
		{
			const int32_t *guest_ts = GUEST(const int32_t *, a);
			s64 sleep_ns = (s64)guest_ts[0] * 1000000000LL + (s64)guest_ts[1];
			if (sleep_ns > 0)
				svcSleepThread(sleep_ns);
			else
				svcSleepThread(0);
		}
		return 0;
	}
	case __NR_clock_nanosleep:
	{
		if (c)
		{
			const int32_t *guest_ts = GUEST(const int32_t *, c);
			int flags = (int)b;
			s64 sleep_ns = (s64)guest_ts[0] * 1000000000LL + (s64)guest_ts[1];
			if (flags & 1) /* TIMER_ABSTIME */
			{
				struct timespec now;
				clock_gettime(translate_clockid((int)a), &now);
				s64 now_ns = (s64)now.tv_sec * 1000000000LL + (s64)now.tv_nsec;
				sleep_ns -= now_ns;
			}
			if (sleep_ns > 0)
				svcSleepThread(sleep_ns);
			else
				svcSleepThread(0);
		}
		return 0;
	}
	case __NR_gettimeofday:
	{
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		if (a)
		{
			int32_t *guest_tv = GUEST(int32_t *, a);
			guest_tv[0] = (int32_t)ts.tv_sec;
			guest_tv[1] = (int32_t)(ts.tv_nsec / 1000);
		}
		return 0;
	}

	/* --- Futex (used by musl's mutexes and condvars) --- */
	case __NR_futex:
	{
		/* Minimal futex emulation: WAIT and WAKE only.
		   The guest's musl uses futexes for its pthread
		   implementation. On Switch, we spin-wait or yield. */
		int op = (int)b & 0x7f;
		volatile uint32_t *addr = GUEST(volatile uint32_t *, a);

		if (op == 0 || op == 9) /* FUTEX_WAIT or FUTEX_WAIT_BITSET */
		{
			uint32_t val = (uint32_t)c;
			int retries = 10000;

			while (*addr == val && retries-- > 0)
				svcSleepThread(100000); /* 100 µs */
			return *addr == val ? -ETIMEDOUT : 0;
		}
		else if (op == 1 || op == 10) /* FUTEX_WAKE or FUTEX_WAKE_BITSET */
		{
			/* Wake is a no-op in our spin-wait model;
			   the waiting thread will see the change. */
			return (uint32_t)c; /* "woke N waiters" */
		}
		else if (op == 3 || op == 4) /* FUTEX_REQUEUE or FUTEX_CMP_REQUEUE */
		{
			return (uint32_t)c;
		}
		host_logf(HOST_LOG_WARN, "unhandled futex op %d (b=0x%llx)", op, b);
		return -ENOSYS;
	}

	/* --- Threading --- */
	case __NR_clone:
	case __NR_set_tid_address:
		return 1; /* fake TID */
	case __NR_sched_yield:
		svcSleepThread(0);
		return 0;

	/* --- Process info --- */
	case __NR_sysinfo:
		return host_sysinfo((uint64_t)a);
	case __NR_exit:
	case __NR_exit_group:
		host_exit((int)a);
	case __NR_getpid:
	case __NR_getppid:
	case __NR_gettid:
		return 1;
	case __NR_getuid:
	case __NR_geteuid:
	case __NR_getgid:
	case __NR_getegid:
		return 0;

	/* --- Signals (host owns these) --- */
	case __NR_rt_sigaction:
	case __NR_rt_sigprocmask:
	case __NR_sigaltstack:
	case __NR_kill:
	case __NR_tkill:
	case __NR_tgkill:
		return 0;

	/* --- Random --- */
	case __NR_getrandom:
	{
		/* Use Switch's hardware RNG */
		randomGet(GUEST(void *, a), (size_t)(uint32_t)b);
		return (uint32_t)b;
	}

	/* --- Misc --- */
	case __NR_uname:
	{
		/* Fake a Linux uname for musl */
		struct { char fields[6][65]; } *buf = GUEST(void *, a);
		if (buf)
		{
			memset(buf, 0, sizeof(*buf));
			strcpy(buf->fields[0], "Linux");
			strcpy(buf->fields[1], "switch");
			strcpy(buf->fields[2], "5.10.0");
			strcpy(buf->fields[3], "Halo Switch Port");
			strcpy(buf->fields[4], "aarch64");
		}
		return 0;
	}
	case __NR_prlimit64:
	case __NR_getrlimit:
		return -ENOSYS;
	case __NR_ioctl:
		return -ENOTTY;
	case __NR_membarrier:
		__sync_synchronize();
		return 0;

	default:
		host_logf(HOST_LOG_WARN,
			"guest system call %lld is not supported", number);
		return -ENOSYS;
	}
}

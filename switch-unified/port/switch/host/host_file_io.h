#ifndef HOST_FILE_IO_H
#define HOST_FILE_IO_H
#include <sys/types.h>
#include <sys/stat.h>
#include <stddef.h>
#include "host_io_profile.h"
/* Current thread only: first snapshot enables and returns unavailable. */
int host_file_io_snapshot(struct host_io_profile *);
/* All threads, cumulative; snapshot holds the existing descriptor gate. */
enum host_io_operation { HOST_IO_OPEN, HOST_IO_CLOSE, HOST_IO_DUP, HOST_IO_FCNTL,
    HOST_IO_FSTAT, HOST_IO_TRUNCATE, HOST_IO_SYNC, HOST_IO_SEEK,
    HOST_IO_READ, HOST_IO_WRITE, HOST_IO_PREAD, HOST_IO_PWRITE };
#define HOST_IO_FAILURE_SLOTS 16
struct host_io_failure { unsigned operation; int error; uint64_t count; };
struct host_io_all {
    struct host_io_profile timing;
    uint64_t failures, read_bytes, write_bytes, failure_overflow;
    struct host_io_failure failure[HOST_IO_FAILURE_SLOTS];
};
void host_file_io_all_snapshot(struct host_io_all *);
/* One process-wide guest descriptor gate also covers dup aliases. No logging
 * or guest callbacks under this lock. unlock preserves the operation errno. */
void host_file_io_lock(void);
void host_file_io_unlock(void);
int host_file_open(const char *, int, mode_t);
int host_file_close(int);
int host_file_dup(int);
int host_file_fcntl(int, int, int);
int host_file_fstat(int, struct stat *);
int host_file_truncate(int, off_t);
int host_file_sync(int);
off_t host_file_seek(int, off_t, int);
ssize_t host_file_read(int, void *, size_t);
ssize_t host_file_write(int, const void *, size_t);
ssize_t host_file_pread(int, void *, size_t, off_t);
ssize_t host_file_pwrite(int, const void *, size_t, off_t);
#endif

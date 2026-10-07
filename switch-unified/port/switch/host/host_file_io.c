/* Guest descriptor operations: make seek-based positioned I/O atomic against
 * ordinary offset changes and close/dup. Independently opened files are also
 * serialized; this deliberately favors correctness over parallel I/O. */
#include "host_file_io.h"
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#ifdef __SWITCH__
#include <switch.h>
#endif
#ifndef HOST_FILE_IO_CLOCK
#ifdef __SWITCH__
#define HOST_FILE_IO_CLOCK() armGetSystemTick()
#else
#include <time.h>
static uint64_t host_file_io_clock(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return (uint64_t)t.tv_sec*1000000000+(uint64_t)t.tv_nsec; }
#define HOST_FILE_IO_CLOCK() host_file_io_clock()
#endif
#endif
static pthread_mutex_t file_io_gate = PTHREAD_MUTEX_INITIALIZER;
static _Thread_local unsigned io_enabled;
static _Thread_local uint64_t io_enter, io_acquired;
static _Thread_local struct host_io_profile io_completed;
static struct host_io_all io_all;
/* Called only under the descriptor gate. Preserve exact errno pairs without
 * logging paths, allocating, or concealing exhaustion of the bounded table. */
static void record_failure(unsigned operation)
{
    int error=errno;
    io_all.failures++;
    for(unsigned i=0;i<HOST_IO_FAILURE_SLOTS;++i) {
        struct host_io_failure *f=&io_all.failure[i];
        if(!f->count || (f->operation==operation && f->error==error)) {
            f->operation=operation;f->error=error;f->count++;return;
        }
    }
    io_all.failure_overflow++;
}
void host_file_io_all_snapshot(struct host_io_all *out)
{
    int saved = errno;
    pthread_mutex_lock(&file_io_gate);
    *out = io_all;
    pthread_mutex_unlock(&file_io_gate);
    errno = saved;
}
int host_file_io_snapshot(struct host_io_profile *out)
{
    int available=io_enabled;
    *out=io_completed;
    memset(&io_completed,0,sizeof(io_completed));
    io_enabled=1;
    return available;
}
void host_file_io_lock(void) {
    int saved=errno;
    io_enter=HOST_FILE_IO_CLOCK();
    pthread_mutex_lock(&file_io_gate);
    io_acquired=HOST_FILE_IO_CLOCK();
    errno=saved;
}
void host_file_io_unlock(void) {
    int saved = errno;
    uint64_t finished=HOST_FILE_IO_CLOCK();
    host_io_record(&io_all.timing,io_acquired-io_enter,finished-io_acquired);
    pthread_mutex_unlock(&file_io_gate);
    /* TLS update after release: no profiler lock, I/O, allocation or callback.
     * Time includes completed backend/restore work, excludes unlock/bookkeeping. */
    if(io_enabled)host_io_record(&io_completed,io_acquired-io_enter,finished-io_acquired);
    errno = saved;
}
#define FILE_CALL(type, name, args, call, op) \
    type name args { host_file_io_lock(); type result = (call); \
        if(result < 0) record_failure(op); \
        host_file_io_unlock(); return result; }
FILE_CALL(int, host_file_open, (const char *p, int f, mode_t m), open(p,f,m), HOST_IO_OPEN)
FILE_CALL(int, host_file_close, (int fd), close(fd), HOST_IO_CLOSE)
FILE_CALL(int, host_file_dup, (int fd), dup(fd), HOST_IO_DUP)
FILE_CALL(int, host_file_fcntl, (int fd,int cmd,int arg), fcntl(fd,cmd,arg), HOST_IO_FCNTL)
FILE_CALL(int, host_file_fstat, (int fd,struct stat *s), fstat(fd,s), HOST_IO_FSTAT)
FILE_CALL(int, host_file_truncate, (int fd,off_t n), ftruncate(fd,n), HOST_IO_TRUNCATE)
FILE_CALL(int, host_file_sync, (int fd), fsync(fd), HOST_IO_SYNC)
FILE_CALL(off_t, host_file_seek, (int fd,off_t n,int w), lseek(fd,n,w), HOST_IO_SEEK)
#undef FILE_CALL
ssize_t host_file_read(int fd, void *b, size_t n)
{
    host_file_io_lock(); ssize_t result = read(fd,b,n);
    if(result < 0) record_failure(HOST_IO_READ); else io_all.read_bytes += (uint64_t)result;
    host_file_io_unlock(); return result;
}
ssize_t host_file_write(int fd, const void *b, size_t n)
{
    host_file_io_lock(); ssize_t result = write(fd,b,n);
    if(result < 0) record_failure(HOST_IO_WRITE); else io_all.write_bytes += (uint64_t)result;
    host_file_io_unlock(); return result;
}
static ssize_t positioned(int fd, void *buffer, size_t count, off_t offset, int writing)
{
    ssize_t result = -1;
    int operation_error;
    off_t original;
    host_file_io_lock();
    if (offset < 0) { errno = EINVAL; goto finish; }
    original = lseek(fd, 0, SEEK_CUR);
    if (original == (off_t)-1) goto finish;
    if (lseek(fd, offset, SEEK_SET) == (off_t)-1) goto finish;
    result = writing ? write(fd, buffer, count) : read(fd, buffer, count);
    if(result >= 0) {
        if(writing) io_all.write_bytes += (uint64_t)result;
        else io_all.read_bytes += (uint64_t)result;
    }
    operation_error = errno;
    /* Preserve the original read/write error. After successful I/O, report a
     * restore failure instead of silently claiming the position was retained.
     * A write may already have transferred bytes when restoration fails. */
    if (lseek(fd, original, SEEK_SET) == (off_t)-1) {
        if (result >= 0) result = -1;
        else errno = operation_error;
    } else if (result < 0) errno = operation_error;
finish:
    if(result < 0) record_failure(writing ? HOST_IO_PWRITE : HOST_IO_PREAD);
    host_file_io_unlock();
    return result;
}
ssize_t host_file_pread(int fd, void *b, size_t n, off_t offset)
{ return positioned(fd,b,n,offset,0); }
ssize_t host_file_pwrite(int fd, const void *b, size_t n, off_t offset)
{ return positioned(fd,(void *)b,n,offset,1); }

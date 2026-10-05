#include "../host/host_file_io.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
static int fd;
static void *worker(void *unused)
{
    (void)unused; char b[16];
    for (int i=0;i<1000;++i) assert(host_file_pread(fd,b,sizeof b,0)==sizeof b);
    return NULL;
}
int main(void)
{
    char path[]="/tmp/nxhalo-io-test-XXXXXX", bytes[16]={0};
    fd=mkstemp(path); assert(fd>=0); unlink(path);
    assert(host_file_write(fd,bytes,sizeof bytes)==sizeof bytes);
    pthread_t threads[4];
    for(int i=0;i<4;++i) assert(!pthread_create(&threads[i],NULL,worker,NULL));
    for(int i=0;i<4;++i) assert(!pthread_join(threads[i],NULL));
    errno=0; assert(host_file_read(-1,bytes,1)==-1 && errno==EBADF);
    assert(host_file_pread(fd,bytes,1,-1)==-1 && errno==EINVAL);
    struct host_io_all all; host_file_io_all_snapshot(&all);
    assert(errno==EINVAL && all.timing.calls==4003 && all.failures==2);
    assert(all.read_bytes==64000 && all.write_bytes==16);
    assert(all.failure[0].operation==HOST_IO_READ && all.failure[0].error==EBADF && all.failure[0].count==1);
    assert(all.failure[1].operation==HOST_IO_PREAD && all.failure[1].error==EINVAL && all.failure[1].count==1);
    assert(!all.failure_overflow);
    assert(!all.timing.saturated); assert(!host_file_close(fd));
    puts("all-thread IO: 4000 worker reads, bytes, injected failures and errno passed");
}

#ifndef HALO_DATAGRAM_BATCH_H
#define HALO_DATAGRAM_BATCH_H
/* Receive batching adapted from halo-ce-universal build-114 host_net.c.
 * Retain complete UDP payloads: later reads may use a different buffer size.
 * At most four queues (2 MiB payload total); other sockets use native reads.
 * All native receives and closes on tracked sockets share this lock, avoiding
 * descriptor reuse races. No protocol, send scheduling or thread changes. */
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#define HALO_RX_BATCH 8
#define HALO_RX_SLOTS 4
#define HALO_RX_PAYLOAD 65536
struct halo_rx_queue {
    int next, count, enabled;
    struct { unsigned size; struct sockaddr_storage address; socklen_t address_size;
        unsigned char data[HALO_RX_PAYLOAD]; } packet[HALO_RX_BATCH];
};
static pthread_mutex_t halo_rx_lock = PTHREAD_MUTEX_INITIALIZER;
static struct { int fd; struct halo_rx_queue *queue; } halo_rx_slots[HALO_RX_SLOTS];
/* Caller holds lock. A non-NULL queue marks an assigned slot (fd 0 is valid). */
static struct halo_rx_queue *halo_rx_find(int fd) {
    for (int i=0;i<HALO_RX_SLOTS;i++)
        if (halo_rx_slots[i].queue && halo_rx_slots[i].fd==fd) return halo_rx_slots[i].queue;
    return NULL;
}
static void halo_rx_enable(int fd) {
    if (halo_rx_find(fd)) { halo_rx_find(fd)->enabled=1; return; }
    for (int i=0;i<HALO_RX_SLOTS;i++) if (!halo_rx_slots[i].queue) {
        halo_rx_slots[i].queue=calloc(1,sizeof(struct halo_rx_queue));
        halo_rx_slots[i].fd=fd;
        if (halo_rx_slots[i].queue) halo_rx_slots[i].queue->enabled=1;
        return;
    }
}
static void halo_rx_forget(int fd) {
    for (int i=0;i<HALO_RX_SLOTS;i++) if (halo_rx_slots[i].queue && halo_rx_slots[i].fd==fd) {
        free(halo_rx_slots[i].queue); halo_rx_slots[i].queue=NULL;
    }
}
static int halo_rx_held(int fd, int *bytes) {
    struct halo_rx_queue *q=halo_rx_find(fd);
    if (!q || q->next==q->count) return 0;
    if (bytes) *bytes=(int)q->packet[q->next].size;
    return 1;
}
/* Drop our lock for native blocking/untracked reads; no queue pointer survives. */
static ssize_t halo_rx_native(int fd,void *buffer,size_t length,int flags,struct sockaddr *address,socklen_t *address_length) {
    pthread_mutex_unlock(&halo_rx_lock);
    ssize_t result=recvfrom(fd,buffer,length,flags,address,address_length);
    int saved_errno=errno;
    pthread_mutex_lock(&halo_rx_lock);
    errno=saved_errno;
    return result;
}
/* Invoked with lock held, only on registered nonblocking UDP sockets.
 * Queued payloads remain readable after switching back to blocking mode. */
static ssize_t halo_rx_receive(int fd,void *buffer,size_t length,int flags,
    struct sockaddr *address,socklen_t *address_length) {
    struct halo_rx_queue *q=halo_rx_find(fd);
    int supported=!(flags & ~(MSG_PEEK|MSG_DONTWAIT|MSG_WAITALL));
    if (!q) return halo_rx_native(fd,buffer,length,flags,address,address_length);
    if (!supported) {
        /* Unsupported UDP flags never bypass earlier buffered datagrams. */
        if (q->next!=q->count) { errno=EOPNOTSUPP; return -1; }
        return halo_rx_native(fd,buffer,length,flags,address,address_length);
    }
    if (q->next==q->count) {
        if (!q->enabled) return halo_rx_native(fd,buffer,length,flags,address,address_length);
        /* libnx allocates a service serialization buffer even on EAGAIN. */
        struct pollfd ready={.fd=fd,.events=POLLIN};
        int readiness=poll(&ready,1,0);
        if (readiness<0) return -1;
        if (!readiness) { errno=EAGAIN; return -1; }
        if (ready.revents&POLLNVAL) { errno=EBADF; return -1; }
        struct mmsghdr messages[HALO_RX_BATCH]={0};
        struct iovec vectors[HALO_RX_BATCH];
        for (int i=0;i<HALO_RX_BATCH;i++) {
            vectors[i].iov_base=q->packet[i].data;
            vectors[i].iov_len=HALO_RX_PAYLOAD;
            messages[i].msg_hdr.msg_iov=&vectors[i];
            messages[i].msg_hdr.msg_iovlen=1;
            messages[i].msg_hdr.msg_name=&q->packet[i].address;
            messages[i].msg_hdr.msg_namelen=sizeof(q->packet[i].address);
        }
        int n=recvmmsg(fd,messages,HALO_RX_BATCH,MSG_DONTWAIT,NULL);
        if (n<0 && (errno==ENOSYS || errno==EOPNOTSUPP)) {
            q->enabled=0;
            return halo_rx_native(fd,buffer,length,flags,address,address_length);
        }
        if (n<=0) { if (!n) errno=EAGAIN; return -1; }
        q->next=0; q->count=n;
        for (int i=0;i<n;i++) {
            q->packet[i].size=messages[i].msg_len;
            q->packet[i].address_size=messages[i].msg_hdr.msg_namelen;
        }
    }
    unsigned i=(unsigned)q->next;
    size_t copied=q->packet[i].size<length?q->packet[i].size:length;
    if (copied) memcpy(buffer,q->packet[i].data,copied);
    if (address && address_length) {
        socklen_t n=*address_length<q->packet[i].address_size?*address_length:q->packet[i].address_size;
        if (n) memcpy(address,&q->packet[i].address,n);
        *address_length=q->packet[i].address_size;
    }
    if (!(flags&MSG_PEEK)) q->next++;
    return (ssize_t)copied;
}
#endif

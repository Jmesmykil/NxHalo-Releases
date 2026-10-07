/* Switch socket transport, adapted from our existing posix_net.c.
 * Guest Winsock addresses have a uint16 family; libnx BSD uses length/family
 * bytes. Keep conversion at this boundary; do not mutate guest addresses. */
#include <switch.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdatomic.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include "posix.h"
#include "host.h"
#include "host_socket_poll.h"
#include "host_connected_sendto.h"
#include "host_lan_discovery.h"

/* select consumes native SO_ERROR; retain it until the guest reads it. */
static _Atomic int pending_socket_errors[FD_SETSIZE];
static Mutex net_profile_lock;
static struct net_profile_counter { uint64_t calls,bytes,ticks,max_ticks,would_block,failed,partial; } net_profile[4];
static void net_profile_record(unsigned op,uint64_t begin,int result,int requested)
{
    int saved=errno;
    uint64_t elapsed=armGetSystemTick()-begin;
    mutexLock(&net_profile_lock);
    struct net_profile_counter *p=&net_profile[op];
    p->calls++; p->ticks+=elapsed;
    if(elapsed>p->max_ticks)p->max_ticks=elapsed;
    if(result<0) { if(saved==EAGAIN || saved==EWOULDBLOCK)p->would_block++; else p->failed++; }
    else { p->bytes+=(uint64_t)result; if(op<2 && result<requested)p->partial++; }
    mutexUnlock(&net_profile_lock);
    errno=saved;
}
void host_network_profile_report(uint32_t frame)
{
    struct net_profile_counter snapshot[4];
    const char *names[]={"send","sendto","recv","recvfrom"};
    mutexLock(&net_profile_lock); memcpy(snapshot,net_profile,sizeof(snapshot)); mutexUnlock(&net_profile_lock);
    for(unsigned i=0;i<4;++i) {
        struct net_profile_counter *p=&snapshot[i];
        host_logf_buffered(HOST_LOG_INFO,
            "network_io frame=%u op=%s cumulative=1 calls=%llu bytes=%llu elapsed_us=%llu max_us=%llu would_block=%llu failures=%llu partial_send=%llu rtt=unmeasured loss=unmeasured",
            frame,names[i],(unsigned long long)p->calls,(unsigned long long)p->bytes,
            (unsigned long long)(armTicksToNs(p->ticks)/1000),(unsigned long long)(armTicksToNs(p->max_ticks)/1000),
            (unsigned long long)p->would_block,(unsigned long long)p->failed,(unsigned long long)p->partial);
    }
    host_log_flush();
}

static int guest_address(const void *raw, socklen_t length, struct sockaddr_in *out)
{
    uint16_t family;
    if (!raw || length < 16) { errno = EINVAL; return -1; }
    memcpy(&family, raw, 2);
    if (family != 2) { errno = EAFNOSUPPORT; return -1; }
    memset(out, 0, sizeof(*out));
    out->sin_len = sizeof(*out); out->sin_family = AF_INET;
    memcpy(&out->sin_port, (const char *)raw + 2, 2);
    memcpy(&out->sin_addr, (const char *)raw + 4, 4);
    return 0;
}
static void returned_address(void *raw, socklen_t *length, const struct sockaddr_in *in)
{
    unsigned char bytes[16] = {0}; uint16_t family = 2;
    memcpy(bytes, &family, 2); memcpy(bytes + 2, &in->sin_port, 2);
    memcpy(bytes + 4, &in->sin_addr, 4);
    if (raw && length) memcpy(raw, bytes, *length < 16 ? *length : 16);
    if (length) *length = 16;
}
static int guest_bind(int fd, const void *raw, socklen_t length)
{
    struct sockaddr_in a; if (guest_address(raw,length,&a)<0) return -1;
    return bind(fd,(const struct sockaddr *)&a,sizeof(a));
}
static int guest_connect(int fd, const void *raw, socklen_t length)
{
    struct sockaddr_in a; if (guest_address(raw,length,&a)<0) return -1;
    return connect(fd,(const struct sockaddr *)&a,sizeof(a));
}
static ssize_t guest_sendto(int fd,const void *buffer,size_t length,int flags,const void *raw,socklen_t size)
{
    struct sockaddr_in a;
    if (!raw) return sendto(fd,buffer,length,flags,NULL,0);
    if (guest_address(raw,size,&a)<0) return -1;
    ssize_t result = host_connected_sendto(fd,buffer,length,flags,&a);
    int original_error = errno;
    /* Only the game's 16-byte LAN search to the server port. Gameplay,
     * replies, custom unicast targets and Internet traffic are untouched. */
    if (length == 16 && a.sin_addr.s_addr == INADDR_BROADCAST &&
        ntohs(a.sin_port) == 5150) {
        u32 local, mask, gateway, dns1, dns2;
        Result rc = nifmGetCurrentIpConfigInfo(&local, &mask, &gateway, &dns1, &dns2);
        if (R_SUCCEEDED(rc) && mask) {
            static unsigned cursor, reports;
            struct sockaddr_in target = a;
            target.sin_addr.s_addr = local | ~mask;
            ssize_t sent = sendto(fd, buffer, length, flags,
                (const struct sockaddr *)&target, sizeof(target));
            if (sent >= 0) result = sent;
            uint32_t targets[32];
            int count = host_lan_targets(ntohl(local), ntohl(mask), &cursor, targets, 32);
            unsigned successful = 0;
            for (int i = 0; i < count; i++) {
                target.sin_addr.s_addr = htonl(targets[i]);
                sent = sendto(fd, buffer, length, flags,
                    (const struct sockaddr *)&target, sizeof(target));
                if (sent >= 0) { result = sent; successful++; }
            }
            if (reports++ < 8)
                host_logf(HOST_LOG_INFO, "LAN23 discovery ip=0x%x mask=0x%x local_targets=%d sent=%u",
                    ntohl(local), ntohl(mask), count, successful);
        } else {
            static unsigned reports;
            if (reports++ < 4) host_logf(HOST_LOG_WARN, "LAN23 IP configuration unavailable result=0x%x", rc);
        }
    }
    errno = result < 0 ? original_error : 0;
    return result;
}
static int guest_accept(int fd,void *raw,socklen_t *length)
{
    struct sockaddr_in a; socklen_t size=sizeof(a);
    int result=accept(fd,raw?(struct sockaddr *)&a:NULL,raw?&size:NULL);
    if (result>=0 && raw) returned_address(raw,length,&a);
    return result;
}
static ssize_t guest_recvfrom(int fd,void *buffer,size_t length,int flags,void *raw,socklen_t *size)
{
    struct sockaddr_in a; socklen_t count=sizeof(a);
    ssize_t result=recvfrom(fd,buffer,length,flags,raw?(struct sockaddr *)&a:NULL,raw?&count:NULL);
    if (result>=0 && raw) returned_address(raw,size,&a);
    return result;
}
static int guest_name(int fd,void *raw,socklen_t *length,int peer)
{
    struct sockaddr_in a; socklen_t size=sizeof(a);
    int result=peer?getpeername(fd,(struct sockaddr *)&a,&size):getsockname(fd,(struct sockaddr *)&a,&size);
    if (result>=0) returned_address(raw,length,&a);
    return result;
}
#define bind guest_bind
#define connect guest_connect
#define sendto guest_sendto
#define accept guest_accept
#define recvfrom guest_recvfrom
#define getsockname(fd,a,n) guest_name(fd,a,n,0)
#define getpeername(fd,a,n) guest_name(fd,a,n,1)
/* Winsock error codes (winsockx.h) */
#define WSAEINTR 10004
#define WSAEBADF 10009
#define WSAEACCES 10013
#define WSAEFAULT 10014
#define WSAEINVAL 10022
#define WSAEMFILE 10024
#define WSAEWOULDBLOCK 10035
#define WSAEINPROGRESS 10036
#define WSAEALREADY 10037
#define WSAENOTSOCK 10038
#define WSAEDESTADDRREQ 10039
#define WSAEMSGSIZE 10040
#define WSAEPROTOTYPE 10041
#define WSAENOPROTOOPT 10042
#define WSAEPROTONOSUPPORT 10043
#define WSAEOPNOTSUPP 10045
#define WSAEAFNOSUPPORT 10047
#define WSAEADDRINUSE 10048
#define WSAEADDRNOTAVAIL 10049
#define WSAENETDOWN 10050
#define WSAENETUNREACH 10051
#define WSAECONNABORTED 10053
#define WSAECONNRESET 10054
#define WSAENOBUFS 10055
#define WSAEISCONN 10056
#define WSAENOTCONN 10057
#define WSAETIMEDOUT 10060
#define WSAECONNREFUSED 10061
#define WSAEHOSTUNREACH 10065

/* Winsock SOL_SOCKET option values (winsockx.h) */
#define WINSOCK_SOL_SOCKET 0xffff
#define WINSOCK_SO_REUSEADDR 0x0004
#define WINSOCK_SO_KEEPALIVE 0x0008
#define WINSOCK_SO_BROADCAST 0x0020
#define WINSOCK_SO_LINGER 0x0080
#define WINSOCK_SO_SNDBUF 0x1001
#define WINSOCK_SO_RCVBUF 0x1002
#define WINSOCK_SO_ERROR 0x1007
#define WINSOCK_SO_TYPE 0x1008

static __thread int last_error;

static int fail(void)
{
	switch (errno)
	{
	case EINTR: last_error = WSAEINTR; break;
	case EBADF: last_error = WSAEBADF; break;
	case EACCES: case EPERM: last_error = WSAEACCES; break;
	case EFAULT: last_error = WSAEFAULT; break;
	case EMFILE: case ENFILE: last_error = WSAEMFILE; break;
	case EAGAIN: last_error = WSAEWOULDBLOCK; break;
	case EINPROGRESS: last_error = WSAEINPROGRESS; break;
	case EALREADY: last_error = WSAEALREADY; break;
	case ENOTSOCK: last_error = WSAENOTSOCK; break;
	case EDESTADDRREQ: last_error = WSAEDESTADDRREQ; break;
	case EMSGSIZE: last_error = WSAEMSGSIZE; break;
	case EPROTOTYPE: last_error = WSAEPROTOTYPE; break;
	case ENOPROTOOPT: last_error = WSAENOPROTOOPT; break;
	case EPROTONOSUPPORT: last_error = WSAEPROTONOSUPPORT; break;
	case EOPNOTSUPP: last_error = WSAEOPNOTSUPP; break;
	case EAFNOSUPPORT: last_error = WSAEAFNOSUPPORT; break;
	case EADDRINUSE: last_error = WSAEADDRINUSE; break;
	case EADDRNOTAVAIL: last_error = WSAEADDRNOTAVAIL; break;
	case ENETDOWN: last_error = WSAENETDOWN; break;
	case ENETUNREACH: last_error = WSAENETUNREACH; break;
	case ECONNABORTED: last_error = WSAECONNABORTED; break;
	case ECONNRESET: last_error = WSAECONNRESET; break;
	case ENOBUFS: case ENOMEM: last_error = WSAENOBUFS; break;
	case EISCONN: last_error = WSAEISCONN; break;
	case ENOTCONN: last_error = WSAENOTCONN; break;
	case ETIMEDOUT: last_error = WSAETIMEDOUT; break;
	case ECONNREFUSED: last_error = WSAECONNREFUSED; break;
	case EHOSTUNREACH: last_error = WSAEHOSTUNREACH; break;
	default: last_error = WSAEINVAL; break;
	}
	return -1;
}

static int invalid_argument(int fault)
{
    last_error = fault ? WSAEFAULT : WSAEINVAL;
    return -1;
}

static int succeed(int result)
{
	if (result < 0)
		return fail();
	last_error = 0;
	return result;
}

/* Bounded low-frequency transport diagnostics; retain the original errno. */
static unsigned net_diagnostics;
static int net_result(const char *operation, int fd, int result, int arg0, int arg1)
{
    int saved_errno = errno;
    Result bsd_result = socketGetLastResult();
    int mapped = succeed(result);
    if ((result < 0 || !strcmp(operation,"socket") || !strcmp(operation,"close") || !strcmp(operation,"bind") || !strcmp(operation,"listen") || !strcmp(operation,"setsockopt") || !strcmp(operation,"nonblocking")) &&
        __atomic_fetch_add(&net_diagnostics,1,__ATOMIC_RELAXED) < 128)
        host_logf(result < 0 ? HOST_LOG_WARN : HOST_LOG_INFO,
            "net op=%s fd=%d result=%d errno=%d wsa=%d bsd=0x%x arg0=%d arg1=%d",
            operation,fd,result,result < 0 ? saved_errno : 0,last_error,bsd_result,arg0,arg1);
    errno = saved_errno;
    return mapped;
}

/* LAN22: bound datagram diagnostics, including addresses before guest conversion.
 * Preserve errno and the Winsock mapping across logging. */
static int lan22_datagram_result(const char *op, int fd, int result, int flags,
    const void *address, int address_length)
{
    static unsigned successes[2], failures[2], would_block[2];
    unsigned direction = !strcmp(op, "recvfrom");
    int saved_errno = errno;
    int mapped = succeed(result);
    Result bsd_result = socketGetLastResult();
    unsigned sample;
    if (result < 0 && (saved_errno == EAGAIN || saved_errno == EWOULDBLOCK)) {
        sample = __atomic_fetch_add(&would_block[direction], 1, __ATOMIC_RELAXED) + 1;
        if (sample != 1 && sample % 10000 != 0) { errno = saved_errno; return mapped; }
    } else {
        sample = __atomic_fetch_add(result < 0 ? &failures[direction] : &successes[direction], 1, __ATOMIC_RELAXED);
        if (sample >= 16) { errno = saved_errno; return mapped; }
    }
    const unsigned char *a = address;
    unsigned port = a && address_length >= 16 ? ((unsigned)a[2] << 8) | a[3] : 0;
    host_logf(result < 0 ? HOST_LOG_WARN : HOST_LOG_INFO,
        "LAN22 op=%s fd=%d result=%d errno=%d wsa=%d bsd=0x%x flags=0x%x peer=%u.%u.%u.%u:%u again=%u",
        op, fd, result, result < 0 ? saved_errno : 0, last_error, bsd_result, flags,
        a && address_length >= 16 ? a[4] : 0, a && address_length >= 16 ? a[5] : 0,
        a && address_length >= 16 ? a[6] : 0, a && address_length >= 16 ? a[7] : 0,
        port, __atomic_load_n(&would_block[direction], __ATOMIC_RELAXED));
    errno = saved_errno;
    return mapped;
}

int posix_socket_last_error(void)
{
	return last_error;
}

int posix_socket(int family, int type, int protocol)
{
	int fd = socket(family,type,protocol);
    if (fd >= 0 && fd < FD_SETSIZE) atomic_store(&pending_socket_errors[fd], 0);
    return net_result("socket", -1, fd,type,protocol);
}

int posix_socket_close(int socket)
{
	if (socket >= 0 && socket < FD_SETSIZE) atomic_store(&pending_socket_errors[socket], 0);
    return net_result("close", socket, close(socket),0,0);
}

int posix_socket_bind(int socket, const void *address, int address_length)
{
    if (!address) return invalid_argument(1);
    if (address_length < 16) return invalid_argument(0);
	return net_result("bind",socket,bind(socket,address,(socklen_t)address_length),address_length,0);
}

int posix_socket_connect(int socket, const void *address, int address_length)
{
    if (!address) return invalid_argument(1);
    if (address_length < 16) return invalid_argument(0);
	/* A non-blocking connect that is under way is EINPROGRESS here but
	WSAEWOULDBLOCK in Winsock, which is what the game waits on before it
	selects for the socket becoming writeable (connect_endpoint,
	transport_endpoint_winsock.c); as WSAEINPROGRESS it gave up at once,
	and every system link join failed, a split screen game's join of its
	own host included. */
	int result = connect(socket, address, (socklen_t)address_length);

	if (result < 0 && errno == EINPROGRESS)
	{
		last_error = WSAEWOULDBLOCK;
		return -1;
	}
	return net_result("connect",socket,result,address_length,0);
}

int posix_socket_listen(int socket, int backlog)
{
	return net_result("listen",socket,listen(socket,backlog),backlog,0);
}

int posix_socket_accept(int socket, void *address, int *address_length)
{
    if (address && !address_length) return invalid_argument(1);
    if (address_length && *address_length < 0) return invalid_argument(0);
	socklen_t length = address_length ? (socklen_t)*address_length : 0;
	int result = accept(socket, address, address_length ? &length : NULL);

	if (address_length)
		*address_length = (int)length;
	return succeed(result);
}

int posix_socket_send(int socket, const void *buffer, int length, int flags)
{
    if (length < 0) return invalid_argument(0);
    if (length && !buffer) return invalid_argument(1);
    uint64_t begin=armGetSystemTick();
    int result = (int)send(socket, buffer, (size_t)length, flags | MSG_NOSIGNAL);
    net_profile_record(0,begin,result,length);
    int saved = errno;
    static unsigned failures, udp_samples;
    int type = 0; socklen_t type_size = sizeof(type);
    /* Candidate-only bounded diagnostics; no payload contents are logged. */
    if ((result < 0 && __atomic_fetch_add(&failures, 1, __ATOMIC_RELAXED) < 16) ||
        (__atomic_load_n(&udp_samples, __ATOMIC_RELAXED) < 16 &&
         getsockopt(socket, SOL_SOCKET, SO_TYPE, &type, &type_size) == 0 && type == SOCK_DGRAM &&
         __atomic_fetch_add(&udp_samples, 1, __ATOMIC_RELAXED) < 16))
        host_logf(result < 0 ? HOST_LOG_WARN : HOST_LOG_INFO,
            "community24 send fd=%d type=%d length=%d result=%d errno=%d flags=%x",
            socket, type, length, result, result < 0 ? saved : 0, flags);
    errno = saved;
    return succeed(result);
}

int posix_socket_sendto(int socket, const void *buffer, int length, int flags,
	const void *address, int address_length)
{
    if (length < 0) return invalid_argument(0);
    if (length && !buffer) return invalid_argument(1);
    if (address && address_length < 16) return invalid_argument(0);
    uint64_t begin=armGetSystemTick();
    int result = (int)sendto(socket, buffer, (size_t)length, flags | MSG_NOSIGNAL,
        address, (socklen_t)address_length);
    net_profile_record(1,begin,result,length);
    return lan22_datagram_result("sendto", socket, result, flags | MSG_NOSIGNAL,
        address, address_length);
}

int posix_socket_recv(int socket, void *buffer, int length, int flags)
{
    if (length < 0) return invalid_argument(0);
    if (length && !buffer) return invalid_argument(1);
    uint64_t begin=armGetSystemTick();
    int result=(int)recv(socket, buffer, (size_t)length, flags);
    net_profile_record(2,begin,result,length);
	return succeed(result);
}

int posix_socket_recvfrom(int socket, void *buffer, int length, int flags,
	void *address, int *address_length)
{
    if (length < 0) return invalid_argument(0);
    if (length && !buffer) return invalid_argument(1);
    if (address && !address_length) return invalid_argument(1);
    if (address && *address_length < 0) return invalid_argument(0);
	socklen_t socket_length = address_length ? (socklen_t)*address_length : 0;
    uint64_t begin=armGetSystemTick();
	int result = (int)recvfrom(socket, buffer, (size_t)length, flags, address,
		address_length ? &socket_length : NULL);
    net_profile_record(3,begin,result,length);

	if (address_length)
		*address_length = (int)socket_length;
	return lan22_datagram_result("recvfrom", socket, result, flags,
        result >= 0 ? address : NULL, result >= 0 && address_length ? *address_length : 0);
}

int posix_socket_shutdown(int socket, int how)
{
	return succeed(shutdown(socket, how));
}

int posix_socket_set_nonblocking(int socket, int nonblocking)
{
	int flags = fcntl(socket, F_GETFL);

	if (flags < 0)
		return fail();
	flags = nonblocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
	return net_result("nonblocking", socket, fcntl(socket, F_SETFL, flags), flags, nonblocking);
}

int posix_socket_bytes_available(int socket, posix_ulong *count)
{
    if (!count) return invalid_argument(1);
	int available = 0;
	int result = ioctl(socket, FIONREAD, &available);

	if (result >= 0)
		*count = (posix_ulong)available;
	return succeed(result);
}

static int translate_option(int level, int name, int *host_level, int *host_name)
{
	if (level != WINSOCK_SOL_SOCKET)
	{
		/* IPPROTO_IP / IPPROTO_TCP option numbers are shared */
		*host_level = level;
		*host_name = name;
		return 0;
	}
	*host_level = SOL_SOCKET;
	switch (name)
	{
	case WINSOCK_SO_REUSEADDR: *host_name = SO_REUSEADDR; return 0;
	case WINSOCK_SO_KEEPALIVE: *host_name = SO_KEEPALIVE; return 0;
	case WINSOCK_SO_BROADCAST: *host_name = SO_BROADCAST; return 0;
	case WINSOCK_SO_LINGER: *host_name = SO_LINGER; return 0;
	case WINSOCK_SO_SNDBUF: *host_name = SO_SNDBUF; return 0;
	case WINSOCK_SO_RCVBUF: *host_name = SO_RCVBUF; return 0;
	case WINSOCK_SO_ERROR: *host_name = SO_ERROR; return 0;
	case WINSOCK_SO_TYPE: *host_name = SO_TYPE; return 0;
	default: return -1;
	}
}

int posix_socket_setsockopt(int socket, int level, int name, const void *value, int length)
{
    if (!value) return invalid_argument(1);
    if (length < 0) return invalid_argument(0);
    if (level == WINSOCK_SOL_SOCKET && name == WINSOCK_SO_LINGER) {
        uint16_t words[2]; struct linger native;
        if (length < (int)sizeof(words)) return invalid_argument(1);
        memcpy(words, value, sizeof(words));
        native.l_onoff = words[0]; native.l_linger = words[1];
        return succeed(setsockopt(socket, SOL_SOCKET, SO_LINGER, &native, sizeof(native)));
    }
	int host_level, host_name;

	if (translate_option(level, name, &host_level, &host_name) != 0)
	{
		/* Xbox-only options such as SO_ENCRYPT have nothing to do here */
		last_error = 0;
		return 0;
	}
	return net_result("setsockopt",socket,setsockopt(socket,host_level,host_name,value,(socklen_t)length),name,length);
}

int posix_socket_getsockopt(int socket, int level, int name, void *value, int *length)
{
    if (!value || !length) return invalid_argument(1);
    if (*length < 0) return invalid_argument(0);
    if (level == WINSOCK_SOL_SOCKET && name == WINSOCK_SO_LINGER) {
        struct linger native; socklen_t size = sizeof(native); uint16_t words[2]; int result;
        if (*length < (int)sizeof(words)) return invalid_argument(1);
        result = getsockopt(socket, SOL_SOCKET, SO_LINGER, &native, &size);
        if (result < 0) return fail();
        if (native.l_onoff < 0 || native.l_onoff > 65535 || native.l_linger < 0 || native.l_linger > 65535) return invalid_argument(0);
        words[0] = (uint16_t)native.l_onoff; words[1] = (uint16_t)native.l_linger;
        memcpy(value, words, sizeof(words)); *length = sizeof(words);
        return succeed(0);
    }
	int host_level, host_name;
	socklen_t socket_length = (socklen_t)*length;
	int result;

	if (translate_option(level, name, &host_level, &host_name) != 0)
	{
		last_error = WSAENOPROTOOPT;
		return -1;
	}
    if (host_level == SOL_SOCKET && host_name == SO_ERROR && socket >= 0 && socket < FD_SETSIZE) {
        int pending;
        if (*length < (int)sizeof(int)) return invalid_argument(0);
        pending = atomic_exchange(&pending_socket_errors[socket], 0);
        if (pending) { memcpy(value, &pending, sizeof(pending)); *length = sizeof(pending); return succeed(0); }
    }
	result = getsockopt(socket, host_level, host_name, value, &socket_length);
	*length = (int)socket_length;
	return succeed(result);
}

int posix_socket_getsockname(int socket, void *address, int *address_length)
{
    if (!address || !address_length) return invalid_argument(1);
    if (address_length && *address_length < 0) return invalid_argument(0);
	socklen_t length = (socklen_t)*address_length;
	int result = getsockname(socket, address, &length);

	*address_length = (int)length;
	return succeed(result);
}

int posix_socket_getpeername(int socket, void *address, int *address_length)
{
    if (!address || !address_length) return invalid_argument(1);
    if (address_length && *address_length < 0) return invalid_argument(0);
	socklen_t length = (socklen_t)*address_length;
	int result = getpeername(socket, address, &length);

	*address_length = (int)length;
	return succeed(result);
}

static int fill_set(fd_set *set, const int *descriptors, int count, int maximum)
{
	int index;

	FD_ZERO(set);
	for (index = 0; index < count; index++)
	{
		if (descriptors[index] >= 0 && descriptors[index] < FD_SETSIZE)
		{
			FD_SET(descriptors[index], set);
			if (descriptors[index] > maximum)
				maximum = descriptors[index];
		}
	}
	return maximum;
}

static void keep_ready(fd_set *set, int *descriptors, int *count)
{
	int index, kept = 0;

	for (index = 0; index < *count; index++)
	{
		if (descriptors[index] >= 0 && descriptors[index] < FD_SETSIZE && FD_ISSET(descriptors[index], set))
			descriptors[kept++] = descriptors[index];
	}
	*count = kept;
}

int posix_socket_select(int *read, int *read_count, int *write, int *write_count,
	int *error, int *error_count, posix_long timeout_seconds, posix_long timeout_microseconds, int infinite)
{
	fd_set read_set, write_set, error_set, requested_errors;
	struct timeval timeout;
	int maximum = -1;
	int result;

	maximum = fill_set(&read_set, read, read ? *read_count : 0, maximum);
	maximum = fill_set(&write_set, write, write ? *write_count : 0, maximum);
	maximum = fill_set(&error_set, error, error ? *error_count : 0, maximum);
    requested_errors = error_set;
	timeout.tv_sec = timeout_seconds;
	timeout.tv_usec = timeout_microseconds;
	result = halo_socket_poll_select(maximum + 1, read ? &read_set : NULL, write ? &write_set : NULL,
		error ? &error_set : NULL, infinite ? NULL : &timeout);
    {
        static unsigned diagnostic_calls;
        if (diagnostic_calls++ < 24)
            host_logf(HOST_LOG_INFO, "NET22 readiness max=%d read=%d fd=%d write=%d fd=%d result=%d errno=%d",
                maximum, read ? *read_count : -1, read && *read_count ? read[0] : -1,
                write ? *write_count : -1, write && *write_count ? write[0] : -1, result, errno);
    }
	if (result < 0)
		return fail();
	if (write)
	{
		/* Winsock reports a socket writeable once its connect has succeeded;
		one whose connect failed is not (it is in the error set), where
		POSIX reports it writeable with the failure in SO_ERROR. The game
		takes writeable as connected (connect_endpoint). */
		int index;

		for (index = 0; index < *write_count; index++)
		{
			int descriptor = write[index];
			int pending = 0;
			socklen_t length = sizeof(pending);

            if (descriptor < 0 || descriptor >= FD_SETSIZE || !FD_ISSET(descriptor, &write_set)) continue;
            pending = atomic_load(&pending_socket_errors[descriptor]);
            if (!pending && getsockopt(descriptor, SOL_SOCKET, SO_ERROR, &pending, &length) < 0) continue;
            if (pending) {
                atomic_store(&pending_socket_errors[descriptor], pending);
                FD_CLR(descriptor, &write_set);
                if (error && FD_ISSET(descriptor, &requested_errors)) FD_SET(descriptor, &error_set);
                errno = pending; fail();
            }
		}
	}
	if (read)
		keep_ready(&read_set, read, read_count);
	if (write)
		keep_ready(&write_set, write, write_count);
	if (error)
		keep_ready(&error_set, error, error_count);
    result = (read ? *read_count : 0) + (write ? *write_count : 0) + (error ? *error_count : 0);
	/* like Winsock, a select with nothing ready leaves the last error as it
	was: after a connect under way, still WSAEWOULDBLOCK, which the game
	reads as not connected yet */
	if (result > 0)
		last_error = 0;
	return result;
}


posix_ulong posix_local_ipv4_address(void)
{
    u32 address=0;
    if (R_FAILED(nifmInitialize(NifmServiceType_User))) return 0;
    Result rc=nifmGetCurrentIpAddress(&address);
    nifmExit();
    return R_SUCCEEDED(rc)?address:0;
}

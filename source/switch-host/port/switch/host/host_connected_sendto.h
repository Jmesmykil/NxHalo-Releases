#ifndef HOST_CONNECTED_SENDTO_H
#define HOST_CONNECTED_SENDTO_H
/* BSD rejects an explicit destination on a connected UDP socket with
 * EISCONN. Only retry as send() when the destination is its connected peer. */
static ssize_t host_connected_sendto(int fd, const void *buffer, size_t length,
    int flags, const struct sockaddr_in *destination)
{
    ssize_t result = sendto(fd, buffer, length, flags,
        (const struct sockaddr *)destination, sizeof(*destination));
    if (result < 0 && errno == EISCONN) {
        const int original_error = errno;
        struct sockaddr_in peer;
        socklen_t size = sizeof(peer), type_size = sizeof(int);
        int type = 0;
        if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &type_size) == 0 &&
            type == SOCK_DGRAM &&
            getpeername(fd, (struct sockaddr *)&peer, &size) == 0 &&
            size >= sizeof(peer) && peer.sin_family == AF_INET &&
            peer.sin_port == destination->sin_port &&
            peer.sin_addr.s_addr == destination->sin_addr.s_addr)
            return send(fd, buffer, length, flags);
        errno = original_error;
    }
    return result;
}
#endif

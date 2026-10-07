#ifndef HALO_SOCKET_POLL_H
#define HALO_SOCKET_POLL_H
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <stdint.h>
#include <sys/select.h>

/* Keep Winsock readiness semantics without requiring Horizon BSD Select.
 * Several emulators implement Poll but return a stubbed zero from Select. */
static int halo_socket_poll_select(int maximum, fd_set *read_set,
    fd_set *write_set, fd_set *error_set, const struct timeval *timeout)
{
    struct pollfd descriptors[FD_SETSIZE];
    fd_set requested_read, requested_write, requested_error;
    int count = 0, milliseconds = -1, ready = 0, index;
    if (maximum < 0 || maximum > FD_SETSIZE) { errno = EINVAL; return -1; }
    FD_ZERO(&requested_read); FD_ZERO(&requested_write); FD_ZERO(&requested_error);
    if (read_set) requested_read = *read_set;
    if (write_set) requested_write = *write_set;
    if (error_set) requested_error = *error_set;
    if (timeout) {
        uint64_t value;
        if (timeout->tv_sec < 0 || timeout->tv_usec < 0 || timeout->tv_usec >= 1000000) {
            errno = EINVAL; return -1;
        }
        if ((uint64_t)timeout->tv_sec > (uint64_t)INT_MAX / 1000) milliseconds = INT_MAX;
        else {
            value = (uint64_t)timeout->tv_sec * 1000 +
                ((uint64_t)timeout->tv_usec + 999) / 1000;
            milliseconds = value > INT_MAX ? INT_MAX : (int)value;
        }
    }
    for (index = 0; index < maximum; ++index) {
        short events = 0;
        if (FD_ISSET(index, &requested_read)) events |= POLLIN;
        if (FD_ISSET(index, &requested_write)) events |= POLLOUT;
        if (FD_ISSET(index, &requested_error)) events |= POLLPRI;
        if (events) {
            descriptors[count].fd = index;
            descriptors[count].events = events;
            descriptors[count].revents = 0;
            ++count;
        }
    }
    if (poll(descriptors, (nfds_t)count, milliseconds) < 0) return -1;
    for (index = 0; index < count; ++index) {
        if (descriptors[index].revents & POLLNVAL) { errno = EBADF; return -1; }
    }
    if (read_set) FD_ZERO(read_set);
    if (write_set) FD_ZERO(write_set);
    if (error_set) FD_ZERO(error_set);
    for (index = 0; index < count; ++index) {
        int descriptor = descriptors[index].fd;
        short events = descriptors[index].revents;
        if (read_set && FD_ISSET(descriptor, &requested_read) &&
            (events & (POLLIN | POLLHUP | POLLERR))) { FD_SET(descriptor, read_set); ++ready; }
        if (write_set && FD_ISSET(descriptor, &requested_write) &&
            (events & (POLLOUT | POLLHUP | POLLERR))) { FD_SET(descriptor, write_set); ++ready; }
        if (error_set && FD_ISSET(descriptor, &requested_error) &&
            (events & POLLPRI)) { FD_SET(descriptor, error_set); ++ready; }
    }
    return ready;
}
#endif

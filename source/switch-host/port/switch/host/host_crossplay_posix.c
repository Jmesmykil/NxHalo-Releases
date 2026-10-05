/* Switch adapters for protocol10's expanded portable host boundary. */
#include "posix.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

int posix_socket_set_nodelay(int fd) {
    int enabled = 1;
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled));
}
posix_ulong posix_resolve_ipv4(const char *host) {
    struct addrinfo hints, *results = NULL;
    posix_ulong address = 0;
    if (!host || !*host) return 0;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(host, NULL, &hints, &results) != 0) return 0;
    if (results && results->ai_addr && results->ai_addr->sa_family == AF_INET)
        address = ((struct sockaddr_in *)results->ai_addr)->sin_addr.s_addr;
    if (results) freeaddrinfo(results);
    return address;
}
int posix_descriptor_is_stream(int fd) {
    struct stat st;
    return fstat(fd, &st) == 0 && (S_ISFIFO(st.st_mode) || S_ISSOCK(st.st_mode));
}
int posix_descriptors_same_file(int a, int b) {
    struct stat x, y;
    return fstat(a, &x) == 0 && fstat(b, &y) == 0 && x.st_dev == y.st_dev && x.st_ino == y.st_ino;
}
/* Mobile-style platform contract: there is no desktop command line, URL
 * registration, or Discord IPC. UPnP lives in host_upnp.c. */
int posix_command_line_argument(int index, char *buffer, posix_ulong size) {
    (void)index; if (buffer && size) buffer[0] = 0; return 0;
}
posix_ulong posix_process_id(void) { return 1; }
int posix_register_url_scheme(const char *scheme, const char *description) {
    (void)scheme; (void)description; return 0;
}
int posix_discord_connect(void) { return -1; }
int posix_discord_write(int fd, const void *buffer, int size) {
    (void)fd; (void)buffer; (void)size; errno = ENOSYS; return -1;
}
int posix_discord_read(int fd, void *buffer, int size) {
    (void)fd; (void)buffer; (void)size; errno = ENOSYS; return -1;
}
void posix_discord_close(int fd) { (void)fd; }

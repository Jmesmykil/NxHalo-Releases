/* Switch HTTPS service. Callbacks never cross the ILP32/LP64 boundary. */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "../../../linux/src/update.h"

extern int host_download(unsigned int packet_address);
struct download_packet {
    uint32_t version, url, url_size, path, path_size, error, error_size, reserved;
    uint64_t maximum_bytes, received, total;
};
_Static_assert(sizeof(struct download_packet) == 56, "download ABI size");
_Static_assert(offsetof(struct download_packet, maximum_bytes) == 32, "download ABI offset");

int update_download_limited(const char *url, const char *path, unsigned long long maximum_bytes,
    update_progress_proc progress, void *context, char *error, int error_size)
{
    size_t u = url ? strnlen(url, 2048) : 0;
    size_t p = path ? strnlen(path, 1024) : 0;
    if (!u || u == 2048 || !p || p == 1024 || !maximum_bytes || error_size < 0 || error_size > 4096) {
        if (error && error_size > 0) snprintf(error, (size_t)error_size, "invalid download arguments");
        return 0;
    }
    struct download_packet packet = {
        .version = 1, .url = (uint32_t)(uintptr_t)url, .url_size = (uint32_t)u + 1,
        .path = (uint32_t)(uintptr_t)path, .path_size = (uint32_t)p + 1,
        .error = (uint32_t)(uintptr_t)error, .error_size = error ? (uint32_t)error_size : 0,
        .maximum_bytes = maximum_bytes
    };
    if (progress) progress(context, 0, 0);
    int result = host_download((uint32_t)(uintptr_t)&packet);
    if (progress) progress(context, packet.received, packet.total);
    return result;
}

int update_download(const char *url, const char *path, update_progress_proc progress,
    void *context, char *error, int error_size)
{
    return update_download_limited(url, path, 512ull * 1024 * 1024, progress, context, error, error_size);
}

void update_delete_file(const char *path)
{
    if (path && unlink(path) != 0) rmdir(path);
}

int update_fetch_text_limited(const char *url,const char *path,unsigned long long maximum_bytes,
    update_progress_proc progress,void *context,char *error,int error_size)
{
    if(!maximum_bytes||maximum_bytes>8ull*1024*1024){
        if(error&&error_size>0)snprintf(error,(size_t)error_size,"Catalogue text exceeds 8 MiB limit");
        return 0;
    }
    return update_download_limited(url,path,maximum_bytes,progress,context,error,error_size);
}

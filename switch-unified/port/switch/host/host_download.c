/* Bounded verified HTTPS downloads; no guest callback enters the host ABI. */
#include "host.h"
#include <curl/curl.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

struct download_packet {
    uint32_t version, url, url_size, path, path_size, error, error_size, reserved;
    uint64_t maximum_bytes, received, total;
};
_Static_assert(sizeof(struct download_packet) == 56, "download ABI size");
_Static_assert(offsetof(struct download_packet, maximum_bytes) == 32, "download ABI offset");
#define DOWNLOAD_CAP (512ull * 1024 * 1024)
static pthread_once_t download_once = PTHREAD_ONCE_INIT;
static CURLcode download_ready;
static void download_init(void) { download_ready = curl_global_init(CURL_GLOBAL_DEFAULT); }

struct download_body { FILE *file; uint64_t received, maximum; int limit, io; };
static size_t download_write(char *data, size_t size, size_t count, void *context)
{
    struct download_body *body = context;
    if (size && count > SIZE_MAX / size) { body->limit = 1; return 0; }
    size_t bytes = size * count;
    if ((uint64_t)bytes > body->maximum - body->received) { body->limit = 1; return 0; }
    size_t written = fwrite(data, 1, bytes, body->file);
    body->received += written;
    if (written != bytes) body->io = 1;
    return written;
}

static int download_string(uint32_t address, uint32_t size, char *out, size_t capacity)
{
    if (!address || size < 2 || size > capacity || !host_low_owns(address, size)) return 0;
    memcpy(out, (const void *)(uintptr_t)address, size);
    return out[size - 1] == 0 && !memchr(out, 0, size - 1);
}

/* Kept separate for native fault tests; destination stays intact on every failure. */
static int download_file(const char *url, const char *path, uint64_t maximum,
    uint64_t *received, uint64_t *total, char *error, size_t error_size)
{
    CURL *curl = NULL;
    CURLcode result = CURLE_FAILED_INIT;
    long status = 0;
    curl_off_t length = -1;
    char temporary[1100] = "", detail[CURL_ERROR_SIZE] = "";
    struct download_body body = { .maximum = maximum < DOWNLOAD_CAP ? maximum : DOWNLOAD_CAP };
    int ok = 0, fd = -1, created = 0;
    *received = *total = 0;
    error[0] = 0;
    if (!body.maximum || strncasecmp(url, "https://", 8) != 0) {
        snprintf(error, error_size, "download requires HTTPS and a nonzero size limit"); return 0;
    }
    pthread_once(&download_once, download_init);
    if (download_ready != CURLE_OK || !(curl = curl_easy_init())) {
        snprintf(error, error_size, "could not initialize HTTPS"); goto done;
    }
    if (snprintf(temporary, sizeof(temporary), "%s.nxdownload-XXXXXX", path) >= (int)sizeof(temporary)) {
        snprintf(error, error_size, "download path too long"); goto done;
    }
    fd = mkstemp(temporary); /* exclusive creation, same filesystem as final file */
    if (fd < 0) { snprintf(error, error_size, "cannot create download file: %s", strerror(errno)); goto done; }
    created = 1;
    body.file = fdopen(fd, "wb");
    if (!body.file) { close(fd); snprintf(error, error_size, "cannot open download stream"); goto done; }
#define OPT(option, value) do { result = curl_easy_setopt(curl, option, value); if (result != CURLE_OK) goto curl_error; } while (0)
    OPT(CURLOPT_URL, url);
#if LIBCURL_VERSION_NUM >= 0x075500
    OPT(CURLOPT_PROTOCOLS_STR, "https");
    OPT(CURLOPT_REDIR_PROTOCOLS_STR, "https");
#else
    OPT(CURLOPT_PROTOCOLS, (long)CURLPROTO_HTTPS);
    OPT(CURLOPT_REDIR_PROTOCOLS, (long)CURLPROTO_HTTPS);
#endif
    OPT(CURLOPT_FOLLOWLOCATION, 1L);
    OPT(CURLOPT_MAXREDIRS, 8L);
    OPT(CURLOPT_SSL_VERIFYPEER, 1L);
    OPT(CURLOPT_SSL_VERIFYHOST, 2L);
#ifdef __SWITCH__
    OPT(CURLOPT_CAINFO, "romfs:/cacert.pem");
#endif
    OPT(CURLOPT_CONNECTTIMEOUT, 20L);
    OPT(CURLOPT_TIMEOUT, 1800L);
    OPT(CURLOPT_LOW_SPEED_LIMIT, 1024L);
    OPT(CURLOPT_LOW_SPEED_TIME, 30L);
    OPT(CURLOPT_NOSIGNAL, 1L);
    OPT(CURLOPT_FAILONERROR, 1L);
    OPT(CURLOPT_MAXFILESIZE_LARGE, (curl_off_t)body.maximum);
    OPT(CURLOPT_ERRORBUFFER, detail);
    OPT(CURLOPT_USERAGENT, "NxHalo-Unified35/1");
    OPT(CURLOPT_WRITEFUNCTION, download_write);
    OPT(CURLOPT_WRITEDATA, &body);
    result = curl_easy_perform(curl);
    if (result != CURLE_OK) goto curl_error;
    result = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    if (result != CURLE_OK) goto curl_error;
    if (status != 200) { snprintf(error, error_size, "download server returned HTTP %ld", status); goto done; }
    if (curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &length) == CURLE_OK && length >= 0)
        *total = (uint64_t)length;
    if (fflush(body.file) != 0 || fsync(fileno(body.file)) != 0) {
        snprintf(error, error_size, "cannot flush download: %s", strerror(errno)); goto done;
    }
    if (fclose(body.file) != 0) { body.file = NULL; snprintf(error, error_size, "cannot close download"); goto done; }
    body.file = NULL;
    if (rename(temporary, path) != 0) {
        snprintf(error, error_size, "cannot install download: %s", strerror(errno)); goto done;
    }
    created = 0;
    ok = 1;
    goto done;
curl_error:
    snprintf(error, error_size, "%s", body.limit ? "download exceeds size limit" :
        body.io ? "download storage write failed" : detail[0] ? detail : curl_easy_strerror(result));
done:
    *received = body.received;
    if (body.file) fclose(body.file);
    if (created) unlink(temporary); /* only this call's exclusive temporary */
    if (curl) curl_easy_cleanup(curl);
    return ok;
#undef OPT
}

int host_download(uint32_t address)
{
    struct download_packet packet;
    char url[2048], path[1024], mapped[1024], error[512];
    if (!address || !host_low_owns(address, sizeof(packet))) return 0;
    memcpy(&packet, (const void *)(uintptr_t)address, sizeof(packet));
    if (packet.version != 1 || packet.reserved || packet.error_size > 4096 ||
        (packet.error_size && (!packet.error || !host_low_owns(packet.error, packet.error_size)))) return 0;
    int ok = 0;
    packet.received = packet.total = 0;
    if (!download_string(packet.url, packet.url_size, url, sizeof(url)) ||
        !download_string(packet.path, packet.path_size, path, sizeof(path))) {
        snprintf(error, sizeof(error), "invalid download string");
    } else {
        const char *destination = host_guest_file_path(path);
        if (!destination || strnlen(destination, sizeof(mapped)) >= sizeof(mapped)) {
            snprintf(error, sizeof(error), "invalid mapped download path");
        } else {
            strcpy(mapped, destination);
            ok = download_file(url, mapped, packet.maximum_bytes, &packet.received, &packet.total, error, sizeof(error));
        }
    }
    host_memory_watch_prepare_write(address, sizeof(packet));
    memcpy((void *)(uintptr_t)address, &packet, sizeof(packet));
    if (packet.error_size) {
        host_memory_watch_prepare_write(packet.error, packet.error_size);
        snprintf((char *)(uintptr_t)packet.error, packet.error_size, "%s", error);
    }
    return ok;
}

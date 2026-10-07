/* Community24: bounded browser HTTP through libcurl's verified libnx TLS. */
#include "host.h"
#include <switch.h>
#include <curl/curl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define SWITCH_CA_BUNDLE "romfs:/cacert.pem"
#define BROWSER_URL_MAX 2048
#define BROWSER_PATH_MAX 512
#define BROWSER_REQUEST_BODY_MAX 16384
#define BROWSER_RESPONSE_MAX (1024 * 1024)

static pthread_once_t curl_once = PTHREAD_ONCE_INIT;
static CURLcode curl_ready;
static pthread_mutex_t key_lock = PTHREAD_MUTEX_INITIALIZER;
static void browser_curl_init(void) { curl_ready = curl_global_init(CURL_GLOBAL_DEFAULT); }
struct response_buffer { char *data; size_t capacity, used; int overflow; };
struct download_output { FILE *file; unsigned long long maximum, received; int overflow; };
static size_t browser_download_write(char *bytes, size_t size, size_t count, void *opaque)
{
    struct download_output *out = opaque;
    if (size && count > SIZE_MAX / size) return 0;
    size_t total = size * count;
    if (out->received > out->maximum || (unsigned long long)total > out->maximum - out->received) { out->overflow = 1; return 0; }
    size_t written = fwrite(bytes, 1, total, out->file);
    out->received += written;
    return written;
}

int host_browser_download(const char *url, const char *path, unsigned long long maximum_bytes,
                          char *error, int error_size)
{
    CURL *curl = NULL;
    FILE *file = NULL;
    char detail[CURL_ERROR_SIZE] = {0};
    long status = 0;
    CURLcode result = CURLE_FAILED_INIT;
    struct download_output output = {0};
    int ok = 0;
    const char *component;
    if (error && error_size > 0) error[0] = 0;
    if (!url || strnlen(url, BROWSER_URL_MAX + 1) > BROWSER_URL_MAX || strncmp(url, "https://", 8) ||
        !path || strnlen(path, BROWSER_PATH_MAX + 1) > BROWSER_PATH_MAX ||
        strncmp(path, "sdmc:/switch/halo/", 18) || maximum_bytes == 0 || maximum_bytes > 512ULL * 1024 * 1024) {
        if (error && error_size > 0) snprintf(error, (size_t)error_size, "Only bounded HTTPS downloads into the Halo SD data folder are allowed");
        return 0;
    }
    for (component = path + 18; *component;) {
        const char *end = strchr(component, '/');
        size_t length = end ? (size_t)(end - component) : strlen(component);
        if (!length || (length == 1 && component[0] == '.') ||
            (length == 2 && component[0] == '.' && component[1] == '.') ||
            memchr(component, '\\', length) || memchr(component, ':', length)) {
            if (error && error_size > 0) snprintf(error, (size_t)error_size, "Unsafe download path");
            return 0;
        }
        if (!end) break;
        component = end + 1;
    }
    file = fopen(path, "wb");
    if (!file) { if (error && error_size > 0) snprintf(error, (size_t)error_size, "Could not create download file"); return 0; }
    output.file = file;
    output.maximum = maximum_bytes;
    pthread_once(&curl_once, browser_curl_init);
    if (curl_ready == CURLE_OK) curl = curl_easy_init();
    if (!curl) goto done;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 8000L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 180000L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, detail);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "NxHalo-Switch/1");
    result = curl_easy_setopt(curl, CURLOPT_CAINFO, SWITCH_CA_BUNDLE);
    if (result != CURLE_OK) {
        snprintf(detail, sizeof(detail), "Could not configure bundled CA roots: %s", curl_easy_strerror(result));
        goto done;
    }
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, browser_download_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &output);
    curl_easy_setopt(curl, CURLOPT_MAXFILESIZE_LARGE, (curl_off_t)maximum_bytes);
    result = curl_easy_perform(curl);
    if (result == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    ok = result == CURLE_OK && status >= 200 && status < 300 && !output.overflow && fflush(file) == 0;
done:
    if (curl) curl_easy_cleanup(curl);
    if (fclose(file) != 0) ok = 0;
    if (!ok) {
        unlink(path);
        if (error && error_size > 0) snprintf(error, (size_t)error_size, "%s", output.overflow ? "Download exceeded its configured byte limit" : detail[0] ? detail : "HTTPS download failed");
    }
    return ok;
}

static size_t browser_write(char *bytes, size_t size, size_t count, void *opaque)
{
    struct response_buffer *out = opaque;
    if (size && count > SIZE_MAX / size) return 0;
    size_t total = size * count;
    size_t room = out->capacity - 1 - out->used;
    if (total > room) { out->overflow = 1; return 0; }
    memcpy(out->data + out->used, bytes, total);
    out->used += total;
    out->data[out->used] = 0;
    return total;
}
int posix_browser_request(const char *url, const char *body, const char *content_type,
                         char *response, int response_size, char *error, int error_size)
{
    CURL *curl = NULL;
    struct curl_slist *headers = NULL;
    char detail[CURL_ERROR_SIZE] = {0};
    long status = 0;
    CURLcode result = CURLE_FAILED_INIT;
    struct response_buffer output = {0};
    if (error && error_size > 0) error[0] = 0;
    if (!response || response_size < 1 || response_size > BROWSER_RESPONSE_MAX || !url ||
        strnlen(url, BROWSER_URL_MAX + 1) > BROWSER_URL_MAX ||
        (strncmp(url, "https://", 8) && strncmp(url, "http://", 7))) {
        if (error && error_size > 0) snprintf(error, (size_t)error_size, "HTTP request has an invalid URL or response capacity");
        return 0;
    }
    if (body && strnlen(body, BROWSER_REQUEST_BODY_MAX + 1) > BROWSER_REQUEST_BODY_MAX) {
        if (error && error_size > 0) snprintf(error, (size_t)error_size, "HTTPS request body exceeds its configured limit");
        return 0;
    }
    if (content_type && strnlen(content_type, 129) > 128) {
        if (error && error_size > 0) snprintf(error, (size_t)error_size, "HTTPS content type is too long");
        return 0;
    }
    response[0] = 0;
    output.data = response;
    output.capacity = (size_t)response_size;
    pthread_once(&curl_once, browser_curl_init);
    if (curl_ready != CURLE_OK || !(curl = curl_easy_init())) goto done;
    /* curl_easy_perform is synchronous: output and every guest-owned string
     * below remain valid until curl and its callbacks are finished. */
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 4000L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 10000L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, detail);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "NxHalo-Community24/1");
    result = curl_easy_setopt(curl, CURLOPT_CAINFO, SWITCH_CA_BUNDLE);
    if (result != CURLE_OK) {
        snprintf(detail, sizeof(detail), "Could not configure bundled CA roots: %s", curl_easy_strerror(result));
        goto done;
    }
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, browser_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &output);
    if (body) {
        char header[256];
        snprintf(header, sizeof(header), "Content-Type: %s", content_type ? content_type : "application/x-www-form-urlencoded");
        headers = curl_slist_append(NULL, header);
        if (!headers) goto done;
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(body));
    }
    result = curl_easy_perform(curl);
    if (result == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
done:
    if (result != CURLE_OK && error && error_size > 0)
        snprintf(error, (size_t)error_size, "%s", output.overflow ? "HTTPS response exceeded its configured capacity" : detail[0] ? detail : curl_easy_strerror(result));
    curl_slist_free_all(headers);
    if (curl) curl_easy_cleanup(curl);
    return result == CURLE_OK ? (int)status : 0;
}
static int browser_write_key(const char *path, const unsigned char *key, int size, int replace)
{
    char temporary[1100];
    if (!path || !key || size <= 0 || size > 4096 || strlen(path) > sizeof(temporary) - 8) return 0;
    snprintf(temporary, sizeof(temporary), "%s.new", path);
    int fd = open(temporary, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) return 0;
    int ok = write(fd, key, (size_t)size) == size && fsync(fd) == 0;
    if (close(fd) != 0) ok = 0;
    struct stat existing;
    if (!replace && stat(path, &existing) == 0) ok = 0;
    if (ok) ok = rename(temporary, path) == 0;
    if (!ok) unlink(temporary);
    return ok;
}
int posix_browser_private_key(const char *path, unsigned char *key, int size)
{
    if (!path || !key || size <= 0 || size > 4096) return 0;
    pthread_mutex_lock(&key_lock);
    int ok = 0, fd = open(path, O_RDONLY);
    if (fd >= 0) {
        struct stat status;
        ok = fstat(fd, &status) == 0 && S_ISREG(status.st_mode) && status.st_size == size && read(fd, key, (size_t)size) == size;
        close(fd);
    } else if (errno == ENOENT) {
        randomGet(key, (size_t)size);
        ok = browser_write_key(path, key, size, 0);
    }
    pthread_mutex_unlock(&key_lock);
    return ok;
}
int posix_browser_replace_key(const char *path, const unsigned char *key, int size)
{
    pthread_mutex_lock(&key_lock);
    int ok = browser_write_key(path, key, size, 1);
    pthread_mutex_unlock(&key_lock);
    return ok;
}

int posix_user_secret(unsigned char *secret, int size) {
    return posix_browser_private_key("sdmc:/switch/halo/community24-native.key", secret, size);
}
void posix_describe_address(void *address, char *buffer, uint32_t size) {
    if (buffer && size) snprintf(buffer, size, "guest address 0x%lx", (unsigned long)(uintptr_t)address);
}

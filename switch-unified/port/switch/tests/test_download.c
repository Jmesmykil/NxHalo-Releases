#include <assert.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../host/host_download.c"

int host_low_owns(uintptr_t address, size_t size) { (void)address; (void)size; return 0; }
void host_memory_watch_prepare_write(uint32_t a, uint32_t b) { (void)a; (void)b; }
const char *host_guest_file_path(const char *path) { return path; }

static void assert_contents(const char *path, const char *expected)
{
    FILE *file = fopen(path, "rb"); assert(file);
    char buffer[64] = ""; size_t n = fread(buffer, 1, sizeof(buffer), file);
    assert(n == strlen(expected) && !memcmp(buffer, expected, n)); fclose(file);
}
int main(int argc, char **argv)
{
    char directory[] = "/Volumes/Media/nxhalo-takeover-20260929/download-test-XXXXXX";
    assert(mkdtemp(directory));
    char path[1024], partial[1024], error[512]; uint64_t received, total;
    snprintf(path, sizeof(path), "%s/map.zip", directory);
    snprintf(partial, sizeof(partial), "%s/map.zip.part", directory);
    FILE *f = fopen(path, "wb"); assert(f); assert(fwrite("original", 1, 8, f) == 8); fclose(f);
    f = fopen(partial, "wb"); assert(f); assert(fwrite("partial", 1, 7, f) == 7); fclose(f);
    assert(!download_file("http://127.0.0.1/", path, 8, &received, &total, error, sizeof(error)));
    assert(strstr(error, "HTTPS")); assert_contents(path, "original");
    assert(!download_file("https://127.0.0.1:1/", path, 8, &received, &total, error, sizeof(error)));
    assert(error[0]); assert_contents(path, "original"); assert_contents(partial, "partial");
    DIR *dir = opendir(directory); assert(dir); struct dirent *entry;
    while ((entry = readdir(dir))) assert(!strstr(entry->d_name, ".nxdownload-"));
    closedir(dir);
    f = tmpfile(); assert(f);
    struct download_body body = {.file = f, .maximum = 4};
    assert(download_write("1234", 1, 4, &body) == 4 && body.received == 4);
    assert(download_write("5", 1, 1, &body) == 0 && body.limit && body.received == 4);
    body.limit = 0;
    assert(download_write("x", SIZE_MAX, 2, &body) == 0 && body.limit); fclose(f);
    assert(!host_download(0)); assert(!host_download(1));
    if (argc > 1) {
        /* A real HTTPS body over the cap must never replace the destination. */
        assert(!download_file(argv[1], path, 1, &received, &total, error, sizeof(error)));
        assert_contents(path, "original"); assert_contents(partial, "partial");
        assert(download_file(argv[1], path, 1024 * 1024, &received, &total, error, sizeof(error)));
        assert(received > 0); assert_contents(partial, "partial");
        printf("verified HTTPS download: %llu bytes\n", (unsigned long long)received);
    }
    if (argc > 2) {
        assert(!download_file(argv[2], path, 1024 * 1024, &received, &total, error, sizeof(error)));
        assert(strstr(error, "certificate") || strstr(error, "SSL"));
        assert_contents(partial, "partial");
        puts("untrusted TLS certificate rejected");
    }
    unlink(path); unlink(partial); rmdir(directory);
    puts("download bounds, overflow, failure preservation and cleanup passed");
    return 0;
}

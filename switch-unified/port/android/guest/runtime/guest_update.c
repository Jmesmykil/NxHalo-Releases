/* Switch guest HTTPS updates and community downloads are served by the
 * Switch host's verified libcurl bridge. No host pointer or file handle is
 * ever represented as a guest pointer. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "guest_host.h"
#include "../../../linux/src/update.h"

int update_download_limited(const char *url, const char *path, unsigned long long maximum_bytes,
	update_progress_proc progress, void *context, char *error, int error_size)
{
	(void)progress;
	(void)context;
	if (!url || strncmp(url, "https://", 8) || !path || maximum_bytes == 0) {
		if (error && error_size > 0) snprintf(error, (size_t)error_size, "HTTPS URL, output path, and byte limit are required");
		return 0;
	}
	return host_browser_download(url, path, maximum_bytes, error, error_size);
}

int update_download(const char *url, const char *path, update_progress_proc progress,
	void *context, char *error, int error_size)
{
	return update_download_limited(url, path, 512ULL * 1024 * 1024, progress, context, error, error_size);
}

int update_executable_path(char *path, int size)
{
	char root[512];
	int count;
	if (!path || size < 2) return 0;
	host_android_path(0, root, sizeof(root));
	root[sizeof(root) - 1] = 0;
	count = snprintf(path, (size_t)size, "%s/halo.nro", root);
	return count > 0 && count < size;
}

void update_delete_file(const char *path)
{
	if (path) (void)unlink(path);
}

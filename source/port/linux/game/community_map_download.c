/* The Online Games browser's opt-in Custom Edition map download. */
#ifdef HALO_GAME_BROWSER
#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "SDL3/SDL.h"
#include "zlib.h"
#include "../src/update.h"
#include "community_map_download.h"

#include <stdio.h>
#include <string.h>

enum
{
	MAP_DOWNLOAD_MAX_ARCHIVE = 512 * 1024 * 1024,
	MAP_DOWNLOAD_MAX_MAP = 1024 * 1024 * 1024
};

struct map_download_job
{
	SDL_Mutex *lock;
	SDL_Thread *thread;
	int state;
	char name[64];
	char target[512];
	char archive[512];
	char message[160];
};

static struct map_download_job map_download;

static unsigned long map_zip_word(unsigned char const *p)
{
	return (unsigned long)p[0] | (unsigned long)p[1] << 8;
}

static unsigned long map_zip_long(unsigned char const *p)
{
	return map_zip_word(p) | map_zip_word(p + 2) << 16;
}

/* Return the basename while rejecting absolute paths and parent traversal. */
static char const *map_zip_basename(char *name, unsigned long size, int *unsafe)
{
	char *base = name;
	unsigned long index;
	*unsafe = !size || name[0] == '/' || name[0] == '\\' ||
		(size >= 2 && ((name[0] >= 'A' && name[0] <= 'Z') || (name[0] >= 'a' && name[0] <= 'z')) && name[1] == ':');
	for (index = 0; index < size; index++)
	{
		if (!name[index]) { *unsafe = 1; return NULL; }
		if (name[index] == '\\') name[index] = '/';
		if (name[index] == '/')
		{
			if (index >= 2 && name[index - 1] == '.' && name[index - 2] == '.' &&
				(index == 2 || name[index - 3] == '/'))
				*unsafe = 1;
			base = name + index + 1;
		}
	}
	if (size >= 2 && name[size - 1] == '.' && name[size - 2] == '.' &&
		(size == 2 || name[size - 3] == '/'))
		*unsafe = 1;
	return base;
}

static void map_download_progress(void *context, unsigned long long received, unsigned long long total)
{
	struct map_download_job *job = context;
	SDL_LockMutex(job->lock);
	if (total)
		snprintf(job->message, sizeof(job->message), "Downloading map: %llu%%", received * 100 / total);
	else
		snprintf(job->message, sizeof(job->message), "Downloading map: %llu MB", received / (1024 * 1024));
	SDL_UnlockMutex(job->lock);
}

/* Extract only the exact requested map. Other archive entries are never written. */
static int map_zip_extract(FILE *zip, char const *expected, char const *target)
{
	unsigned char tail[65558], central[46], local[30], input[16384], output[16384];
	long end, tail_size, directory, end_record;
	unsigned long entries, index, packed, unpacked, crc, offset, written = 0, selected_flags = 0;
	unsigned long checksum = crc32(0L, Z_NULL, 0);
	unsigned long directory_size;
	int found = 0, method = -1, success = 0;
	HANDLE file = INVALID_HANDLE_VALUE;
	char partial[560];

	if (fseek(zip, 0, SEEK_END) || (end = ftell(zip)) < 22)
		return 0;
	tail_size = end < (long)sizeof(tail) ? end : (long)sizeof(tail);
	if (fseek(zip, end - tail_size, SEEK_SET) || fread(tail, 1, (size_t)tail_size, zip) != (size_t)tail_size)
		return 0;
	for (index = (unsigned long)(tail_size - 22 + 1); index-- > 0;)
	{
		if (map_zip_long(tail + index) == 0x06054b50 && index + 22 <= (unsigned long)tail_size &&
			index + 22 + map_zip_word(tail + index + 20) == (unsigned long)tail_size)
		{
			found = 1;
			break;
		}
	}
	if (!found)
		return 0;
	if (map_zip_word(tail + index + 4) || map_zip_word(tail + index + 6) ||
		map_zip_word(tail + index + 8) != map_zip_word(tail + index + 10))
		return 0;
	entries = map_zip_word(tail + index + 10);
	directory_size = map_zip_long(tail + index + 12);
	directory = (long)map_zip_long(tail + index + 16);
	end_record = end - tail_size + (long)index;
	if (directory < 0 || directory > end_record || directory_size > (unsigned long)(end_record - directory) ||
		fseek(zip, directory, SEEK_SET))
		return 0;
	found = 0;
	for (index = 0; index < entries; index++)
	{
		char name[256];
		unsigned long name_size, extra_size, comment_size, flags, compressed_size, uncompressed_size;
		char const *basename;
		int unsafe_path;
		if (fread(central, 1, sizeof(central), zip) != sizeof(central) || map_zip_long(central) != 0x02014b50)
			return 0;
		name_size = map_zip_word(central + 28);
		extra_size = map_zip_word(central + 30);
		comment_size = map_zip_word(central + 32);
		flags = map_zip_word(central + 8);
		compressed_size = map_zip_long(central + 20);
		uncompressed_size = map_zip_long(central + 24);
		if (!name_size || name_size >= sizeof(name) || fread(name, 1, name_size, zip) != name_size)
			return 0;
		name[name_size] = 0;
		basename = map_zip_basename(name, name_size, &unsafe_path);
		if (basename && !SDL_strcasecmp(basename, expected))
		{
			unsigned char extra[4096];
			unsigned long cursor = 0;
			if (unsafe_path || found || (flags & 1) || compressed_size == 0xffffffffUL ||
				uncompressed_size == 0xffffffffUL || map_zip_long(central + 42) == 0xffffffffUL ||
				extra_size > sizeof(extra) || fread(extra, 1, extra_size, zip) != extra_size)
				return 0;
			while (cursor + 4 <= extra_size)
			{
				unsigned long id = map_zip_word(extra + cursor);
				unsigned long length = map_zip_word(extra + cursor + 2);
				if (cursor + 4 + length > extra_size) return 0;
				if (id == 0x0001) return 0;
				cursor += 4 + length;
			}
			if (cursor != extra_size) return 0;
			method = (int)map_zip_word(central + 10);
			packed = compressed_size;
			unpacked = uncompressed_size;
			crc = map_zip_long(central + 16);
			offset = map_zip_long(central + 42);
			selected_flags = flags;
			found = 1;
		}
		else if (fseek(zip, (long)(extra_size + comment_size), SEEK_CUR))
			return 0;
		if (found && basename && !SDL_strcasecmp(basename, expected) &&
			fseek(zip, (long)comment_size, SEEK_CUR))
			return 0;
	}
	if (!found || (method != 0 && method != 8) || !unpacked || unpacked > MAP_DOWNLOAD_MAX_MAP ||
		(method == 0 && packed != unpacked) ||
		packed > MAP_DOWNLOAD_MAX_ARCHIVE || fseek(zip, (long)offset, SEEK_SET) ||
		fread(local, 1, sizeof(local), zip) != sizeof(local) || map_zip_long(local) != 0x04034b50 ||
		map_zip_word(local + 6) != selected_flags || map_zip_word(local + 8) != (unsigned long)method)
		return 0;
	{
		unsigned long local_name_size = map_zip_word(local + 26);
		unsigned long local_extra_size = map_zip_word(local + 28);
		unsigned char local_extra[4096];
		unsigned long cursor = 0;
		if (!local_name_size || local_extra_size > sizeof(local_extra) ||
			fseek(zip, (long)local_name_size, SEEK_CUR) ||
			fread(local_extra, 1, local_extra_size, zip) != local_extra_size)
			return 0;
		while (cursor + 4 <= local_extra_size)
		{
			unsigned long id = map_zip_word(local_extra + cursor);
			unsigned long length = map_zip_word(local_extra + cursor + 2);
			if (cursor + 4 + length > local_extra_size || id == 0x0001) return 0;
			cursor += 4 + length;
		}
		if (cursor != local_extra_size) return 0;
	}
	snprintf(partial, sizeof(partial), "%s.partial", target);
	file = CreateFileA(partial, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return 0;
	{
		z_stream stream;
		unsigned long remaining = packed;
		int ended = method == 0;
		memset(&stream, 0, sizeof(stream));
		if (method == 8 && inflateInit2(&stream, -MAX_WBITS) != Z_OK)
			goto done;
		while (remaining)
		{
			size_t count = remaining < sizeof(input) ? remaining : sizeof(input);
			DWORD amount;
			if (fread(input, 1, count, zip) != count) break;
			remaining -= (unsigned long)count;
			if (method == 0)
			{
				if (count > unpacked - written || !WriteFile(file, input, (DWORD)count, &amount, NULL) || amount != count)
					break;
				checksum = crc32(checksum, input, (uInt)count);
				written += (unsigned long)count;
			}
			else
			{
				int result;
				stream.next_in = input;
				stream.avail_in = (uInt)count;
				do
				{
					size_t produced;
					stream.next_out = output;
					stream.avail_out = sizeof(output);
					result = inflate(&stream, Z_NO_FLUSH);
					if (result != Z_OK && result != Z_STREAM_END) goto inflate_done;
					produced = sizeof(output) - stream.avail_out;
					if (produced > unpacked - written ||
						!WriteFile(file, output, (DWORD)produced, &amount, NULL) || amount != produced) goto inflate_done;
					checksum = crc32(checksum, output, (uInt)produced);
					written += (unsigned long)produced;
					if (result == Z_STREAM_END) ended = 1;
				} while (stream.avail_out == 0 && !ended);
				if (ended) break;
				continue;
inflate_done:
				inflateEnd(&stream);
				goto done;
			}
		}
		if (written == unpacked && checksum == crc &&
			(method != 8 || (ended && remaining == 0 && stream.avail_in == 0)))
			success = 1;
		if (method == 8) inflateEnd(&stream);
	}
done:
	CloseHandle(file);
	if (success)
	{
		if (MoveFileA(partial, target)) return 1;
	}
	DeleteFileA(partial);
	return 0;
}

static int SDLCALL map_download_worker(void *context)
{
	struct map_download_job *job = context;
	char url[256], error[256] = "Map download failed.";
	char *directory, expected[80], *slash;
	FILE *zip;
	int index, result = 0;
	for (index = 0; job->name[index]; index++)
		if (!((job->name[index] >= 'a' && job->name[index] <= 'z') ||
			(job->name[index] >= 'A' && job->name[index] <= 'Z') ||
			(job->name[index] >= '0' && job->name[index] <= '9') ||
			job->name[index] == '_' || job->name[index] == '-' || job->name[index] == '.'))
			goto failed;
	if (!job->name[0]) goto failed;
	directory = SDL_GetPrefPath("NxHalo", "map-download");
	if (!directory) goto failed;
	snprintf(job->archive, sizeof(job->archive), "%s%s.zip", directory, job->name);
	SDL_free(directory);
	snprintf(url, sizeof(url), "https://maps.halonet.net/halonet/locator.php?map=%s&type=ce&format=zip", job->name);
	if (!update_download(url, job->archive, MAP_DOWNLOAD_MAX_ARCHIVE, map_download_progress, job, error, sizeof(error)))
		goto failed;
	zip = fopen(job->archive, "rb");
	if (!zip) goto failed;
	snprintf(expected, sizeof(expected), "%s.map", job->name);
	/* The game path is trusted and constructed by the caller; create only its CE directory. */
	snprintf(url, sizeof(url), "%s", job->target);
	slash = strrchr(url, '\\');
	if (slash) { *slash = 0; CreateDirectoryA(url, NULL); }
	result = map_zip_extract(zip, expected, job->target);
	fclose(zip);
	update_delete_file(job->archive);
	if (!result) { snprintf(error, sizeof(error), "Map archive was invalid or failed its size/CRC check."); goto failed; }
	SDL_LockMutex(job->lock);
	job->state = COMMUNITY_MAP_DOWNLOAD_READY;
	snprintf(job->message, sizeof(job->message), "Map downloaded and verified.");
	SDL_UnlockMutex(job->lock);
	return 0;
failed:
	update_delete_file(job->archive);
	SDL_LockMutex(job->lock);
	job->state = COMMUNITY_MAP_DOWNLOAD_FAILED;
	snprintf(job->message, sizeof(job->message), "%s", error);
	SDL_UnlockMutex(job->lock);
	return 0;
}

int community_map_download_start(char const *name, char const *target)
{
	if (!map_download.lock) map_download.lock = SDL_CreateMutex();
	if (!map_download.lock) return 0;
	SDL_LockMutex(map_download.lock);
	if (map_download.state == COMMUNITY_MAP_DOWNLOAD_RUNNING)
	{
		SDL_UnlockMutex(map_download.lock);
		return 0;
	}
	SDL_UnlockMutex(map_download.lock);
	if (map_download.thread) { SDL_WaitThread(map_download.thread, NULL); map_download.thread = NULL; }
	snprintf(map_download.name, sizeof(map_download.name), "%s", name);
	snprintf(map_download.target, sizeof(map_download.target), "%s", target);
	SDL_LockMutex(map_download.lock);
	map_download.state = COMMUNITY_MAP_DOWNLOAD_RUNNING;
	snprintf(map_download.message, sizeof(map_download.message), "Starting map download...");
	SDL_UnlockMutex(map_download.lock);
	map_download.thread = SDL_CreateThread(map_download_worker, "community map download", &map_download);
	if (!map_download.thread)
	{
		SDL_LockMutex(map_download.lock);
		map_download.state = COMMUNITY_MAP_DOWNLOAD_FAILED;
		snprintf(map_download.message, sizeof(map_download.message), "Could not start map download.");
		SDL_UnlockMutex(map_download.lock);
		return 0;
	}
	return 1;
}

int community_map_download_status(char *message, int size)
{
	int state;
	if (!map_download.lock) return COMMUNITY_MAP_DOWNLOAD_IDLE;
	SDL_LockMutex(map_download.lock);
	state = map_download.state;
	snprintf(message, (size_t)size, "%s", map_download.message);
	SDL_UnlockMutex(map_download.lock);
	return state;
}

void community_map_download_clear(void)
{
	if (map_download.state == COMMUNITY_MAP_DOWNLOAD_RUNNING) return;
	if (map_download.thread) { SDL_WaitThread(map_download.thread, NULL); map_download.thread = NULL; }
	map_download.state = COMMUNITY_MAP_DOWNLOAD_IDLE;
}
#endif

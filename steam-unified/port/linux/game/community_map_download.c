/* The Online Games browser's opt-in Custom Edition map download. */
#ifdef HALO_GAME_BROWSER
#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "SDL3/SDL.h"
#include "zlib.h"
#include "../src/update.h"
#include "community_map_download.h"

#include <errno.h>
#include <stdio.h>
/* SDL file dialogs and pref paths are native paths, never Xbox d:\ paths.
 * The game stdio shim otherwise translates these a second time. */
#ifdef fopen
#undef fopen
#endif
#include <string.h>

enum
{
	MAP_DOWNLOAD_MAX_ARCHIVE = 512 * 1024 * 1024,
	MAP_DOWNLOAD_MAX_MAP = 0x11600000,
	MAP_RESOURCE_HEADER_SIZE = 16,
	MAP_RESOURCE_RECORD_SIZE = 12
};

struct map_download_job
{
	SDL_Mutex *lock;
	SDL_Thread *thread;
	int state;
	int kind;
	char name[64];
	char source[1024];
	char target_directory[512];
	char target[512];
	char archive[512];
	char message[160];
};

static struct map_download_job map_download;
static SDL_SpinLock map_initialization_lock;

enum
{
	_map_job_download,
	_map_job_local_map,
	_map_job_local_resource
};

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

/* Check the cache header used by the loader. The file may be CE (609) or
Halo PC retail (7); engine extension maps and DLL payloads are not accepted. */
static int map_cache_header_valid(FILE *file, unsigned long long physical_size)
{
	unsigned char header[0x800];
	unsigned long declared_size;
	unsigned long version;
	if (physical_size < sizeof(header) || physical_size > MAP_DOWNLOAD_MAX_MAP ||
		fseek(file, 0, SEEK_SET) || fread(header, 1, sizeof(header), file) != sizeof(header))
		return 0;
	if (memcmp(header, "daeh", 4) || memcmp(header + sizeof(header) - 4, "toof", 4))
		return 0;
	version = map_zip_long(header + 4);
	declared_size = map_zip_long(header + 8);
	return (version == 609 || version == 7) && declared_size >= sizeof(header) &&
		declared_size == physical_size && declared_size <= MAP_DOWNLOAD_MAX_MAP;
}

/* Resource maps are not cache files. Their four-word header identifies one
of bitmaps (1), sounds (2), or loc (3), and bounds its paths and table. */
static int map_resource_header_valid(FILE *file, unsigned long long physical_size, unsigned long expected_type)
{
	unsigned char header[MAP_RESOURCE_HEADER_SIZE];
	unsigned long paths_offset, resources_offset, count;
	unsigned long long table_size;
	if (physical_size < sizeof(header) || physical_size > 0x7fffffffULL ||
		fseek(file, 0, SEEK_SET) || fread(header, 1, sizeof(header), file) != sizeof(header))
		return 0;
	if (map_zip_long(header) != expected_type)
		return 0;
	paths_offset = map_zip_long(header + 4);
	resources_offset = map_zip_long(header + 8);
	count = map_zip_long(header + 12);
	table_size = (unsigned long long)count * MAP_RESOURCE_RECORD_SIZE;
	return paths_offset >= MAP_RESOURCE_HEADER_SIZE && paths_offset <= resources_offset &&
		resources_offset <= physical_size && table_size <= physical_size - resources_offset;
}

static int map_file_size(FILE *file, unsigned long long *size)
{
	long end;
	if (fseek(file, 0, SEEK_END) || (end = ftell(file)) < 0)
		return 0;
	*size = (unsigned long long)end;
	return 1;
}

/* Host path basename, with map names restricted to a single safe component. */
static char const *map_local_basename(char const *path)
{
	char const *base = path;
	char const *cursor;
	for (cursor = path; *cursor; cursor++)
		if (*cursor == '/' || *cursor == '\\')
			base = cursor + 1;
	return base;
}

static int map_safe_component(char const *name, char const *extension)
{
	size_t length = strlen(name), extension_length = strlen(extension), index;
	if (length <= extension_length || SDL_strcasecmp(name + length - extension_length, extension))
		return 0;
	for (index = 0; index < length - extension_length; index++)
		if (!((name[index] >= 'a' && name[index] <= 'z') ||
			(name[index] >= 'A' && name[index] <= 'Z') ||
			(name[index] >= '0' && name[index] <= '9') ||
			name[index] == '_' || name[index] == '-'))
			return 0;
	return 1;
}

static int map_file_copy_to_target(char const *source, char const *target, int resource_type, char *error, int error_size)
{
	char partial[600] = "";
	FILE *input = NULL;
	HANDLE output = INVALID_HANDLE_VALUE, existing;
	unsigned char buffer[65536], header[0x800];
	unsigned long long size, copied = 0;
	int valid = 0;
	DWORD written;
	existing = CreateFileA(target, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (existing != INVALID_HANDLE_VALUE)
	{
		CloseHandle(existing);
		snprintf(error, (size_t)error_size, "%s already exists; existing content was preserved.", map_local_basename(target));
		return 0;
	}
	input = fopen(source, "rb");
	if (!input || !map_file_size(input, &size))
	{
		snprintf(error, (size_t)error_size, "Could not read the selected file.");
		goto done;
	}
	if (resource_type)
		valid = map_resource_header_valid(input, size, (unsigned long)resource_type);
	else
		valid = map_cache_header_valid(input, size);
	if (!valid)
	{
		snprintf(error, (size_t)error_size, resource_type ?
			"Resource map header or bounds are invalid." :
			"Map must have a valid version 609 or 7 cache header.");
		goto done;
	}
	if (fseek(input, 0, SEEK_SET))
		goto done;
	snprintf(partial, sizeof(partial), "%s.partial", target);
	output = CreateFileA(partial, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
	if (output == INVALID_HANDLE_VALUE)
	{
		snprintf(error, (size_t)error_size, "A partial target already exists; it was preserved.");
		goto done;
	}
	while (copied < size)
	{
		size_t wanted = size - copied < sizeof(buffer) ? (size_t)(size - copied) : sizeof(buffer);
		size_t got = fread(buffer, 1, wanted, input);
		if (!got || !WriteFile(output, buffer, (DWORD)got, &written, NULL) || written != got)
			goto done;
		copied += got;
	}
	if (SetFilePointer(output, 0, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER ||
		!ReadFile(output, header, resource_type ? MAP_RESOURCE_HEADER_SIZE : sizeof(header), &written, NULL) ||
		written != (resource_type ? MAP_RESOURCE_HEADER_SIZE : sizeof(header)))
		goto done;
	/* Validate the copied bytes again before the atomic no-replace move. */
	if (resource_type)
		valid = map_zip_long(header) == (unsigned long)resource_type &&
			map_zip_long(header + 4) >= MAP_RESOURCE_HEADER_SIZE &&
			map_zip_long(header + 4) <= map_zip_long(header + 8) &&
			map_zip_long(header + 8) <= size &&
			(unsigned long long)map_zip_long(header + 12) * MAP_RESOURCE_RECORD_SIZE <=
				size - map_zip_long(header + 8);
	else
		valid = !memcmp(header, "daeh", 4) &&
			(map_zip_long(header + 4) == 609 || map_zip_long(header + 4) == 7) &&
			map_zip_long(header + 8) == size && size <= MAP_DOWNLOAD_MAX_MAP;
	if (!CloseHandle(output))
	{
		output = INVALID_HANDLE_VALUE;
		snprintf(error, (size_t)error_size, "Could not close the partial file.");
		goto done;
	}
	output = INVALID_HANDLE_VALUE;
	if (!valid)
	{
		snprintf(error, (size_t)error_size, "The copied file did not pass validation.");
		goto done;
	}
	output = INVALID_HANDLE_VALUE;
	if (!MoveFileA(partial, target))
	{
		snprintf(error, (size_t)error_size, "Could not install the file; any existing target was preserved.");
		goto done;
	}
	valid = 1;
done:
	if (output != INVALID_HANDLE_VALUE)
		CloseHandle(output);
	if (input)
		fclose(input);
	if (!valid && partial[0])
		DeleteFileA(partial);
	return valid;
}

/* Find one safe cache map in a user ZIP. Archives with multiple maps require
a deliberate per-file choice and are therefore not imported automatically. */
static int map_zip_single_map(FILE *zip, char *expected, size_t expected_size)
{
	unsigned char tail[65558], central[46];
	long end, tail_size;
	unsigned long index, entries, directory, directory_size;
	int found = 0, maps = 0;
	if (fseek(zip, 0, SEEK_END) || (end = ftell(zip)) < 22) return 0;
	tail_size = end < (long)sizeof(tail) ? end : (long)sizeof(tail);
	if (fseek(zip, end - tail_size, SEEK_SET) || fread(tail, 1, (size_t)tail_size, zip) != (size_t)tail_size) return 0;
	for (index = (unsigned long)(tail_size - 22 + 1); index-- > 0;)
		if (map_zip_long(tail + index) == 0x06054b50 && index + 22 <= (unsigned long)tail_size &&
			index + 22 + map_zip_word(tail + index + 20) == (unsigned long)tail_size) { found = 1; break; }
	if (!found || map_zip_word(tail + index + 4) || map_zip_word(tail + index + 6) ||
		map_zip_word(tail + index + 8) != map_zip_word(tail + index + 10)) return 0;
	entries = map_zip_word(tail + index + 10);
	directory_size = map_zip_long(tail + index + 12);
	directory = map_zip_long(tail + index + 16);
	if (directory > (unsigned long)(end - tail_size + (long)index) ||
		directory_size > (unsigned long)(end - tail_size + (long)index) - directory ||
		fseek(zip, (long)directory, SEEK_SET)) return 0;
	for (index = 0; index < entries; index++)
	{
		char name[256];
		unsigned long name_size, extra_size, comment_size, flags;
		char const *base;
		int unsafe_path;
		if (fread(central, 1, sizeof(central), zip) != sizeof(central) ||
			map_zip_long(central) != 0x02014b50) return 0;
		name_size=map_zip_word(central+28); extra_size=map_zip_word(central+30);
		comment_size=map_zip_word(central+32); flags=map_zip_word(central+8);
		if (!name_size || name_size >= sizeof(name) || fread(name,1,name_size,zip)!=name_size) return 0;
		name[name_size]=0;
		base=map_zip_basename(name,name_size,&unsafe_path);
		if (base && !unsafe_path && !(flags & 1) && name[name_size-1]!='/' &&
			strlen(base)>4 && !SDL_strcasecmp(base+strlen(base)-4,".map") &&
			SDL_strcasecmp(base,"bitmaps.map") && SDL_strcasecmp(base,"sounds.map") &&
			SDL_strcasecmp(base,"loc.map"))
		{
			if (++maps != 1 || strlen(base) + 1 > expected_size) return 0;
			snprintf(expected, expected_size, "%s", base);
		}
		if (fseek(zip,(long)(extra_size+comment_size),SEEK_CUR)) return 0;
	}
	return maps == 1;
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
	{
		HANDLE existing = CreateFileA(target, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (existing != INVALID_HANDLE_VALUE)
		{
			CloseHandle(existing);
			return 0;
		}
	}
	file = CreateFileA(partial, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
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
	if (success)
	{
		unsigned long long file_size = GetFileSize(file, NULL);
		if (file_size == INVALID_FILE_SIZE)
			success = 0;
		else
		{
			unsigned char cache_header[0x800];
			DWORD read = 0;
			if (SetFilePointer(file, 0, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER ||
				!ReadFile(file, cache_header, sizeof(cache_header), &read, NULL) || read != sizeof(cache_header) ||
				memcmp(cache_header, "daeh", 4) || memcmp(cache_header + 0x7fc, "toof", 4) ||
				(map_zip_long(cache_header + 4) != 609 && map_zip_long(cache_header + 4) != 7) ||
				map_zip_long(cache_header + 8) < sizeof(cache_header) ||
				map_zip_long(cache_header + 8) != file_size || file_size > MAP_DOWNLOAD_MAX_MAP)
				success = 0;
		}
	}
	CloseHandle(file);
	if (success)
	{
		if (MoveFileA(partial, target)) return 1;
	}
	DeleteFileA(partial);
	return 0;
}

static int map_target_exists(char const *target)
{
	HANDLE file = CreateFileA(target, GENERIC_READ, 0, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return 0;
	CloseHandle(file);
	return 1;
}

static void map_target_path(char const *directory, char const *name, char *path, size_t size)
{
	size_t length = strlen(directory);
	char separator = length && (directory[length - 1] == '/' || directory[length - 1] == '\\') ? 0 : '/';
	snprintf(path, size, "%s%c%s", directory, separator ? separator : directory[length - 1], name);
}

static int map_local_install(struct map_download_job *job, char *error, int error_size)
{
	char const *base = map_local_basename(job->source);
	char target[512], expected[80];
	size_t length = strlen(base);
	int result;
	FILE *zip;
	if (job->kind == _map_job_local_resource)
	{
		unsigned long type;
		if (!SDL_strcasecmp(base, "bitmaps.map")) type = 1;
		else if (!SDL_strcasecmp(base, "sounds.map")) type = 2;
		else if (!SDL_strcasecmp(base, "loc.map")) type = 3;
		else
		{
			snprintf(error, (size_t)error_size, "Select bitmaps.map, sounds.map, or loc.map to import a resource map.");
			return 0;
		}
		map_target_path(job->target_directory, type==1 ? "bitmaps.map" : type==2 ? "sounds.map" : "loc.map", target, sizeof(target));
		if (map_target_exists(target))
		{
			snprintf(error, (size_t)error_size, "%s already exists; existing content was preserved.", base);
			return 0;
		}
		result = map_file_copy_to_target(job->source, target, (int)type, error, error_size);
		if (result) snprintf(error, (size_t)error_size, "Imported %s.", base);
		return result;
	}
	if (length > 4 && !SDL_strcasecmp(base + length - 4, ".zip"))
	{
		zip = fopen(job->source, "rb");
		if (!zip || !map_zip_single_map(zip, expected, sizeof(expected)))
		{
			if (zip) fclose(zip);
			snprintf(error, (size_t)error_size, "ZIP must contain exactly one safe .map file; resource maps are imported separately.");
			return 0;
		}
		map_target_path(job->target_directory, expected, target, sizeof(target));
		if (map_target_exists(target))
		{
			fclose(zip);
			snprintf(error, (size_t)error_size, "%s already exists; existing content was preserved.", expected);
			return 0;
		}
		result = map_zip_extract(zip, expected, target);
		fclose(zip);
		if (!result)
		{
			snprintf(error, (size_t)error_size, "Map ZIP failed its bounds, header, size, or CRC checks.");
			return 0;
		}
		snprintf(error, (size_t)error_size, "Installed %s from ZIP.", expected);
		return 1;
	}
	if (!map_safe_component(base, ".map"))
	{
		snprintf(error, (size_t)error_size, "Choose a .map file or a ZIP containing one map. .yelo and DLL files are unsupported.");
		return 0;
	}
	map_target_path(job->target_directory, base, target, sizeof(target));
	result = map_file_copy_to_target(job->source, target, 0, error, error_size);
	if (result) snprintf(error, (size_t)error_size, "Installed %s.", base);
	return result;
}

static void map_download_log_failure(char const *name, char const *error)
{
	fprintf(stderr, "halo-linux: CE map download %s failed: %s\n",
		name && name[0] ? name : "(unknown)", error && error[0] ? error : "unspecified failure");
}

static int SDLCALL map_download_worker(void *context)
{
	struct map_download_job *job = context;
	char url[256], error[256] = "";
	char *directory, expected[80], *slash;
	FILE *zip;
	int index, result = 0;
	if (job->kind != _map_job_download)
	{
		char message[160] = "Local content import failed.";
		if (!CreateDirectoryA(job->target_directory, NULL))
		{
			char probe[560];
			HANDLE existing;
			snprintf(probe, sizeof(probe), "%s/probe.tmp", job->target_directory);
			existing = CreateFileA(probe, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
			if (existing != INVALID_HANDLE_VALUE)
			{ CloseHandle(existing); DeleteFileA(probe); }
		}
		result = map_local_install(job, message, sizeof(message));
		if (!result)
			map_download_log_failure(job->source, message);
		SDL_LockMutex(job->lock);
		job->state = result ? COMMUNITY_MAP_DOWNLOAD_READY : COMMUNITY_MAP_DOWNLOAD_FAILED;
		snprintf(job->message, sizeof(job->message), "%s", message);
		SDL_UnlockMutex(job->lock);
		return 0;
	}
	for (index = 0; job->name[index]; index++)
		if (!((job->name[index] >= 'a' && job->name[index] <= 'z') ||
			(job->name[index] >= 'A' && job->name[index] <= 'Z') ||
			(job->name[index] >= '0' && job->name[index] <= '9') ||
			job->name[index] == '_' || job->name[index] == '-' || job->name[index] == '.'))
		{
			snprintf(error, sizeof(error), "Map name contains unsupported characters.");
			goto failed;
		}
	if (!job->name[0])
	{
		snprintf(error, sizeof(error), "Map name is empty.");
		goto failed;
	}
	if (map_target_exists(job->target))
	{
		snprintf(error, sizeof(error), "The target map already exists; existing content was preserved.");
		goto failed;
	}
	directory = SDL_GetPrefPath("NxHalo", "map-download");
	if (!directory)
	{
		snprintf(error, sizeof(error), "Could not open the map-download cache directory: %s", SDL_GetError());
		goto failed;
	}
	snprintf(job->archive, sizeof(job->archive), "%s%s.zip", directory, job->name);
	SDL_free(directory);
	snprintf(url, sizeof(url), "https://maps.halonet.net/halonet/locator.php?map=%s&type=ce&format=zip", job->name);
	if (!update_download_limited(url, job->archive, MAP_DOWNLOAD_MAX_ARCHIVE, map_download_progress, job, error, sizeof(error)))
		goto failed;
	zip = fopen(job->archive, "rb");
	if (!zip)
	{
		snprintf(error, sizeof(error), "Downloaded archive could not be reopened: %s", strerror(errno));
		goto failed;
	}
	{
		unsigned char signature[4];
		if (fread(signature, 1, sizeof(signature), zip) != sizeof(signature) ||
			signature[0] != 'P' || signature[1] != 'K' || signature[2] != 3 || signature[3] != 4)
		{
			fclose(zip);
			snprintf(error, sizeof(error), "HaloNet returned a non-ZIP response; check the endpoint response and map name.");
			update_delete_file(job->archive);
			goto failed;
		}
		rewind(zip);
	}
	snprintf(expected, sizeof(expected), "%s.map", job->name);
	/* The game path is trusted and constructed by the caller; create only its CE directory. */
	snprintf(url, sizeof(url), "%s", job->target);
	slash = strrchr(url, '\\');
	if (slash)
	{
		DWORD attributes;
		*slash = 0;
		if (!CreateDirectoryA(url, NULL))
		{
			attributes = GetFileAttributesA(url);
			if (attributes == (DWORD)-1 || !(attributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				fclose(zip);
				snprintf(error, sizeof(error), "Could not create the CE map folder; no map was installed.");
				goto failed;
			}
		}
	}
	result = map_zip_extract(zip, expected, job->target);
	fclose(zip);
	update_delete_file(job->archive);
	if (!result)
	{
		snprintf(error, sizeof(error),
			"Map ZIP did not contain a safe %s with a valid cache header, size, and CRC; no target was replaced.",
			expected);
		goto failed;
	}
	SDL_LockMutex(job->lock);
	job->state = COMMUNITY_MAP_DOWNLOAD_READY;
	snprintf(job->message, sizeof(job->message), "Map downloaded and verified.");
	SDL_UnlockMutex(job->lock);
	return 0;
failed:
	update_delete_file(job->archive);
	map_download_log_failure(job->name, error);
	SDL_LockMutex(job->lock);
	job->state = COMMUNITY_MAP_DOWNLOAD_FAILED;
	snprintf(job->message, sizeof(job->message), "Map %s failed: %.120s",
		job->name[0] ? job->name : "(unknown)", error[0] ? error : "unspecified failure");
	SDL_UnlockMutex(job->lock);
	return 0;
}

static int map_download_begin(int kind, char const *name, char const *source, char const *target,
	char const *target_directory)
{
	SDL_Thread *previous;
	SDL_LockSpinlock(&map_initialization_lock);
	if (!map_download.lock) map_download.lock = SDL_CreateMutex();
	SDL_UnlockSpinlock(&map_initialization_lock);
	if (!map_download.lock) return 0;
	SDL_LockMutex(map_download.lock);
	if (map_download.state == COMMUNITY_MAP_DOWNLOAD_RUNNING)
	{
		SDL_UnlockMutex(map_download.lock);
		return 0;
	}
	map_download.state = COMMUNITY_MAP_DOWNLOAD_RUNNING;
	previous=map_download.thread; map_download.thread=NULL;
	SDL_UnlockMutex(map_download.lock);
	if (previous) SDL_WaitThread(previous, NULL);
	map_download.kind = kind;
	snprintf(map_download.name, sizeof(map_download.name), "%s", name ? name : "");
	snprintf(map_download.source, sizeof(map_download.source), "%s", source ? source : "");
	snprintf(map_download.target, sizeof(map_download.target), "%s", target ? target : "");
	snprintf(map_download.target_directory, sizeof(map_download.target_directory), "%s",
		target_directory ? target_directory : "");
	map_download.archive[0] = 0;
	SDL_LockMutex(map_download.lock);
	map_download.state = COMMUNITY_MAP_DOWNLOAD_RUNNING;
	snprintf(map_download.message, sizeof(map_download.message), "%s",
		kind == _map_job_download ? "Starting map download..." : "Checking selected content...");
	SDL_UnlockMutex(map_download.lock);
	map_download.thread = SDL_CreateThread(map_download_worker, "community map setup", &map_download);
	if (!map_download.thread)
	{
		SDL_LockMutex(map_download.lock);
		map_download.state = COMMUNITY_MAP_DOWNLOAD_FAILED;
		snprintf(map_download.message, sizeof(map_download.message), "Could not start map setup worker.");
		SDL_UnlockMutex(map_download.lock);
		return 0;
	}
	return 1;
}

int community_map_download_start(char const *name, char const *target)
{
	if (!name || !target) return 0;
	return map_download_begin(_map_job_download, name, NULL, target, NULL);
}

int community_map_download_install_local(char const *source, char const *target_directory)
{
	if (!source || !target_directory) return 0;
	return map_download_begin(_map_job_local_map, NULL, source, NULL, target_directory);
}

int community_map_download_import_resource(char const *source, char const *target_directory)
{
	if (!source || !target_directory) return 0;
	return map_download_begin(_map_job_local_resource, NULL, source, NULL, target_directory);
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
	if (!map_download.lock) return;
	SDL_LockMutex(map_download.lock);
	if (map_download.state == COMMUNITY_MAP_DOWNLOAD_RUNNING) { SDL_UnlockMutex(map_download.lock); return; }
	if (map_download.thread) { SDL_WaitThread(map_download.thread, NULL); map_download.thread = NULL; }
	map_download.state = COMMUNITY_MAP_DOWNLOAD_IDLE;
	SDL_UnlockMutex(map_download.lock);
}
#endif

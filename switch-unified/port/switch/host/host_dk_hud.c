/*
HOST_DK_HUD.C

The high-res HUD's sheets, decoded by the host (dk_commands.h,
DK_COMMAND_HUD_PREPARE). Made in the guest, at the first frame to draw it, a
2048 by 2048 sheet held that frame 135 to 180 ms: its PNG inflated, its red
and blue swapped, eleven levels averaged down, and the levels sent through
the command stream a megabyte at a time. Here a thread of its own, on a
helper core, does all of that as a map loads, each sheet into a buffer of
the host's own memory (the guest's heap has no room for a map's sheets), in
the order the guest gave; the render thread then makes the image from the
buffer in place (host_dk.c: a memory block over it, which the GPU copies the
levels from) and lets it go.

Decoded ahead, the Halo level's 71 sheets held 202 MB, 130 of them in six
2048 by 2048 sheets (three scope masks among them, drawn only when zoomed).
So a map's sheets are decoded ahead the largest first, as many as
HUD_AHEAD_BUDGET holds: the largest take longest (a 2048 by 2048 sheet 130
to 155 ms; the time goes with the texels, not the PNG's size), the rest,
smaller, are quick to decode when first drawn. A sheet the guest asks for
that is not decoded (one left out, a title's) is decoded then, before any
ahead; the original bitmap is drawn until it is ready.

The PNGs are the ones tools/hud_assets.py writes - 8-bit RGBA, not
interlaced - as port/linux/src/hud_hires.c's png_decode reads them, inflated
with the port's zlib (port/third_party/zlib, its names z_).
*/

#include "host.h"
#include "../guest/dk_commands.h"

#include <zlib.h>

#include <deko3d.h>
#include <switch.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/* (hud_hires.c's MAXIMUM_TEXTURES) */
#define HUD_SHEETS 256
/* a PNG's inflated rows and its levels, held at once: the largest shipped
is 2048 by 2048 */
#define HUD_DECODED_LIMIT (192UL << 20)
/* the most a map's sheets decoded ahead hold at once */
#define HUD_AHEAD_BUDGET (150ULL << 20)

enum
{
	_sheet_none,
	_sheet_queued,
	_sheet_decoding,
	_sheet_ready,
	_sheet_failed,
};

static struct
{
	int state;
	/* the guest's: its PNG and its tag's name */
	uint32_t png, png_size, tag;
	struct host_dk_hud_levels levels;
	/* (let go while being decoded: dropped when done) */
	int forgotten;
	/* (asked for by the guest: in the queue of those asked for too) */
	int demanded;
} sheets[HUD_SHEETS];

/* the sheets to decode: those the guest has asked for first, then those
ahead (an asset may be in both: the second time it is passed over) */
static struct
{
	long assets[HUD_SHEETS];
	long head, count;
} asked, ahead;
static int thread_started;
/* the bytes the ready sheets hold, and the most they have */
static uint64_t held_bytes, held_most;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t work = PTHREAD_COND_INITIALIZER;

static uint32_t big_endian(const unsigned char *bytes)
{
	return (uint32_t)bytes[0] << 24 | (uint32_t)bytes[1] << 16 | (uint32_t)bytes[2] << 8 | bytes[3];
}

static unsigned char paeth(unsigned char left, unsigned char up, unsigned char up_left)
{
	int estimate = (int)left + up - up_left;
	int to_left = abs(estimate - left), to_up = abs(estimate - up), to_up_left = abs(estimate - up_left);

	if (to_left <= to_up && to_left <= to_up_left)
		return left;
	return to_up <= to_up_left ? up : up_left;
}

static uint32_t level_size(uint32_t value, uint32_t level)
{
	value >>= level;
	return value ? value : 1;
}

/* the PNG's levels, blue first, the largest first, one after another in a
buffer aligned and rounded to DK_MEMBLOCK_ALIGNMENT, written back from the
CPU's cache; 0 if it is not one tools/hud_assets.py writes */
int host_dk_hud_decode(const unsigned char *data, uint32_t size, struct host_dk_hud_levels *levels)
{
	uint32_t width = size >= 33 ? big_endian(data + 16) : 0, height = size >= 33 ? big_endian(data + 20) : 0;
	uint32_t stride = width * 4, position = 8, level, row, column;
	unsigned long filtered_size = (unsigned long)height * (stride + 1), total = 0, compressed_size = 0, at;
	uLongf inflated_size = filtered_size;
	unsigned char *compressed = NULL, *filtered = NULL, *texels = NULL;
	int result;

	memset(levels, 0, sizeof(*levels));
	if (size < 33 || memcmp(data, "\x89PNG\r\n\x1a\n", 8) || memcmp(data + 12, "IHDR", 4) || !width || !height ||
		width > 8192 || height > 8192 || data[24] != 8 || data[25] != 6 || data[28] != 0)
		return 0;
	levels->width = width;
	levels->height = height;
	levels->levels = 1;
	while ((width > height ? width : height) >> levels->levels)
		levels->levels++;
	for (level = 0; level < levels->levels; level++)
		total += (unsigned long)level_size(width, level) * level_size(height, level) * 4;
	if (filtered_size + total > HUD_DECODED_LIMIT)
		return 0;
	levels->bytes = (uint32_t)((total + DK_MEMBLOCK_ALIGNMENT - 1) & ~(unsigned long)(DK_MEMBLOCK_ALIGNMENT - 1));
	compressed = malloc(size);
	filtered = compressed ? malloc(filtered_size) : NULL;
	texels = filtered ? aligned_alloc(DK_MEMBLOCK_ALIGNMENT, levels->bytes) : NULL;
	if (!texels)
		goto failed;
	while (position + 12 <= size)
	{
		uint32_t length = big_endian(data + position);

		if (length > size - position - 12)
			break;
		if (!memcmp(data + position + 4, "IDAT", 4))
		{
			memcpy(compressed + compressed_size, data + position + 8, length);
			compressed_size += length;
		}
		else if (!memcmp(data + position + 4, "IEND", 4))
			break;
		position += 12 + length;
	}
	result = uncompress(filtered, &inflated_size, compressed, compressed_size);
	if ((result != Z_OK && result != Z_BUF_ERROR) || inflated_size != filtered_size)
		goto failed;
	/* each row by its filter (the first row's above is zeroes), then its red
	and blue swapped */
	for (row = 0; row < height; row++)
	{
		const unsigned char *line = filtered + (unsigned long)row * (stride + 1) + 1;
		unsigned char *out = texels + (unsigned long)row * stride;
		const unsigned char *above = row ? out - stride : NULL;

		switch (line[-1])
		{
		case 0:
			memcpy(out, line, stride);
			break;
		case 1:
			for (column = 0; column < stride; column++)
				out[column] = (unsigned char)(line[column] + (column >= 4 ? out[column - 4] : 0));
			break;
		case 2:
			for (column = 0; column < stride; column++)
				out[column] = (unsigned char)(line[column] + (above ? above[column] : 0));
			break;
		case 3:
			for (column = 0; column < stride; column++)
				out[column] = (unsigned char)(line[column] +
					(((column >= 4 ? out[column - 4] : 0) + (above ? above[column] : 0)) >> 1));
			break;
		case 4:
			for (column = 0; column < stride; column++)
				out[column] = (unsigned char)(line[column] + paeth(column >= 4 ? out[column - 4] : 0,
					above ? above[column] : 0, above && column >= 4 ? above[column - 4] : 0));
			break;
		default:
			goto failed;
		}
	}
	free(filtered);
	free(compressed);
	filtered = compressed = NULL;
	for (at = 0; at < (unsigned long)width * height * 4; at += 4)
	{
		unsigned char red = texels[at];

		texels[at] = texels[at + 2];
		texels[at + 2] = red;
	}
	/* each next level's texel the mean of the (up to) four above it */
	for (level = 0, at = 0; level + 1 < levels->levels; level++)
	{
		uint32_t level_width = level_size(width, level), level_height = level_size(height, level);
		uint32_t next_width = level_size(width, level + 1), next_height = level_size(height, level + 1), x, y, c;
		const unsigned char *upper = texels + at;
		unsigned char *next = texels + at + (unsigned long)level_width * level_height * 4;

		for (y = 0; y < next_height; y++)
		{
			uint32_t y0 = y * 2, y1 = y * 2 + 1 < level_height ? y * 2 + 1 : y * 2;

			for (x = 0; x < next_width; x++)
			{
				uint32_t x0 = x * 2, x1 = x * 2 + 1 < level_width ? x * 2 + 1 : x * 2;

				for (c = 0; c < 4; c++)
					next[((unsigned long)y * next_width + x) * 4 + c] = (unsigned char)((
						upper[((unsigned long)y0 * level_width + x0) * 4 + c] +
						upper[((unsigned long)y0 * level_width + x1) * 4 + c] +
						upper[((unsigned long)y1 * level_width + x0) * 4 + c] +
						upper[((unsigned long)y1 * level_width + x1) * 4 + c] + 2) / 4);
			}
		}
		at += (unsigned long)level_width * level_height * 4;
	}
	/* (the GPU reads it from memory, not the cache this core wrote it into) */
	armDCacheFlush(texels, levels->bytes);
	levels->texels = texels;
	return 1;

failed:
	free(texels);
	free(filtered);
	free(compressed);
	memset(levels, 0, sizeof(*levels));
	return 0;
}

static void *sheet_thread(void *unused)
{
	(void)unused;
	host_thread_place_on_helper_core();
	for (;;)
	{
		struct host_dk_hud_levels levels;
		uint32_t png, png_size, tag;
		uint64_t start;
		long asset;
		int decoded;

		pthread_mutex_lock(&lock);
		while (!asked.count && !ahead.count)
			pthread_cond_wait(&work, &lock);
		if (asked.count)
		{
			asset = asked.assets[asked.head];
			asked.head = (asked.head + 1) % HUD_SHEETS;
			asked.count--;
		}
		else
		{
			asset = ahead.assets[ahead.head];
			ahead.head = (ahead.head + 1) % HUD_SHEETS;
			ahead.count--;
		}
		if (sheets[asset].state != _sheet_queued)
		{
			pthread_mutex_unlock(&lock);
			continue;
		}
		sheets[asset].state = _sheet_decoding;
		sheets[asset].forgotten = 0;
		sheets[asset].demanded = 0;
		png = sheets[asset].png;
		png_size = sheets[asset].png_size;
		tag = sheets[asset].tag;
		pthread_mutex_unlock(&lock);

		start = armGetSystemTick();
		decoded = host_dk_hud_decode((const unsigned char *)(uintptr_t)png, png_size, &levels);

		pthread_mutex_lock(&lock);
		if (sheets[asset].forgotten)
		{
			free(levels.texels);
			sheets[asset].state = _sheet_none;
		}
		else if (!decoded)
			sheets[asset].state = _sheet_failed;
		else
		{
			sheets[asset].levels = levels;
			sheets[asset].state = _sheet_ready;
			held_bytes += levels.bytes;
			if (held_bytes > held_most)
				held_most = held_bytes;
		}
		pthread_mutex_unlock(&lock);
		if (!decoded)
			host_logf(HOST_LOG_WARN, "deko3d: high-res hud: %s (sheet %ld) could not be decoded",
				tag ? (const char *)(uintptr_t)tag : "?", asset);
		else if (host_dk_hud_log)
			host_logf(HOST_LOG_INFO, "deko3d: high-res hud: %s, %ux%u, decoded in %.1f ms (%.1f MB; %.1f MB held)",
				tag ? (const char *)(uintptr_t)tag : "?", (unsigned)levels.width, (unsigned)levels.height,
				armTicksToNs(armGetSystemTick() - start) / 1e6, levels.bytes / 1048576.0, held_bytes / 1048576.0);
	}
	return NULL;
}

/* (under lock) */
static void sheet_queue(uint32_t asset, uint32_t png, uint32_t png_size, uint32_t tag, int demand)
{
	if (!thread_started)
	{
		pthread_t thread;

		thread_started = pthread_create(&thread, NULL, sheet_thread, NULL) == 0 ? 1 : -1;
		if (thread_started > 0)
			pthread_detach(thread);
		else
			host_logf(HOST_LOG_ERROR, "deko3d: high-res hud: no thread to decode the sheets on; drawn as they are");
	}
	if (thread_started < 0)
		return;
	/* (one queued ahead, asked for now: in the asked queue too) */
	if (demand && sheets[asset].state == _sheet_queued && !sheets[asset].demanded && asked.count < HUD_SHEETS)
	{
		sheets[asset].demanded = 1;
		asked.assets[(asked.head + asked.count++) % HUD_SHEETS] = (long)asset;
		pthread_cond_signal(&work);
		return;
	}
	if (sheets[asset].state != _sheet_none || (demand ? asked.count : ahead.count) == HUD_SHEETS)
		return;
	sheets[asset].state = _sheet_queued;
	sheets[asset].png = png;
	sheets[asset].png_size = png_size;
	sheets[asset].tag = tag;
	sheets[asset].demanded = demand;
	if (demand)
		asked.assets[(asked.head + asked.count++) % HUD_SHEETS] = (long)asset;
	else
		ahead.assets[(ahead.head + ahead.count++) % HUD_SHEETS] = (long)asset;
	pthread_cond_signal(&work);
}

/* the bytes a sheet's levels will take, from its PNG's header (0 if it
has none) */
static uint64_t sheet_bytes(uint32_t png, uint32_t png_size)
{
	const unsigned char *data = (const unsigned char *)(uintptr_t)png;
	uint32_t width, height, level = 0;
	uint64_t total = 0;

	if (png_size < 24)
		return 0;
	width = big_endian(data + 16);
	height = big_endian(data + 20);
	if (!width || !height || width > 8192 || height > 8192)
		return 0;
	do
		total += (uint64_t)level_size(width, level) * level_size(height, level) * 4;
	while ((width > height ? width : height) >> ++level);
	return (total + DK_MEMBLOCK_ALIGNMENT - 1) & ~(uint64_t)(DK_MEMBLOCK_ALIGNMENT - 1);
}

int host_dk_hud_log = 1;

void host_dk_hud_prepare(const struct dk_hud_sheet *list, uint32_t count)
{
	uint32_t order[HUD_SHEETS], index, sorted = 0, queued = 0;
	uint64_t bytes[HUD_SHEETS], planned = 0, left_out = 0;

	/* the largest first (the list is the map's, a few dozen) */
	for (index = 0; index < count && sorted < HUD_SHEETS; index++)
	{
		uint32_t at;

		if (list[index].asset >= HUD_SHEETS)
			continue;
		bytes[index] = sheet_bytes(list[index].png, list[index].png_size);
		for (at = sorted; at > 0 && bytes[order[at - 1]] < bytes[index]; at--)
			order[at] = order[at - 1];
		order[at] = index;
		sorted++;
	}
	pthread_mutex_lock(&lock);
	for (index = 0; index < sorted; index++)
	{
		const struct dk_hud_sheet *sheet = &list[order[index]];

		if (sheets[sheet->asset].state != _sheet_none)
			continue;
		if (held_bytes + planned + bytes[order[index]] > HUD_AHEAD_BUDGET)
		{
			left_out += bytes[order[index]];
			continue;
		}
		planned += bytes[order[index]];
		queued++;
		sheet_queue(sheet->asset, sheet->png, sheet->png_size, sheet->tag, 0);
	}
	pthread_mutex_unlock(&lock);
	host_logf(HOST_LOG_INFO, "deko3d: high-res hud: %u of the map's %u sheets decoded ahead, the largest (%.1f MB of "
		"%.0f MB); the other %u when first drawn (%.1f MB)", (unsigned)queued, (unsigned)sorted, planned / 1048576.0,
		HUD_AHEAD_BUDGET / 1048576.0, (unsigned)(sorted - queued), left_out / 1048576.0);
}

void host_dk_hud_forget(void)
{
	long asset;
	uint32_t ready = 0;
	uint64_t let_go = 0;

	pthread_mutex_lock(&lock);
	asked.count = ahead.count = 0;
	for (asset = 0; asset < HUD_SHEETS; asset++)
	{
		sheets[asset].demanded = 0;
		switch (sheets[asset].state)
		{
		case _sheet_queued:
			sheets[asset].state = _sheet_none;
			break;
		case _sheet_decoding:
			sheets[asset].forgotten = 1;
			break;
		case _sheet_ready:
			ready++;
			let_go += sheets[asset].levels.bytes;
			free(sheets[asset].levels.texels);
			memset(&sheets[asset].levels, 0, sizeof(sheets[asset].levels));
			sheets[asset].state = _sheet_none;
			break;
		}
	}
	held_bytes -= let_go;
	pthread_mutex_unlock(&lock);
	if (ready && host_dk_hud_log)
		host_logf(HOST_LOG_INFO, "deko3d: high-res hud: %u sheets decoded and never drawn let go (%.1f MB; %.1f MB "
			"held at most)", (unsigned)ready, let_go / 1048576.0, held_most / 1048576.0);
}

int host_dk_hud_ready(unsigned int asset, unsigned int png, unsigned int png_size, unsigned int tag)
{
	int answer;

	if (asset >= HUD_SHEETS)
		return -1;
	pthread_mutex_lock(&lock);
	if (sheets[asset].state == _sheet_none || sheets[asset].state == _sheet_queued)
		sheet_queue(asset, png, png_size, tag, 1);
	answer = sheets[asset].state == _sheet_ready ? 1 : sheets[asset].state == _sheet_failed || thread_started < 0 ?
		-1 : 0;
	pthread_mutex_unlock(&lock);
	return answer;
}

int host_dk_hud_take(uint32_t asset, struct host_dk_hud_levels *levels)
{
	int taken = 0;

	memset(levels, 0, sizeof(*levels));
	if (asset >= HUD_SHEETS)
		return 0;
	pthread_mutex_lock(&lock);
	if (sheets[asset].state == _sheet_ready)
	{
		*levels = sheets[asset].levels;
		memset(&sheets[asset].levels, 0, sizeof(sheets[asset].levels));
		/* (made: asked again only by a new image of it, which decodes it again) */
		sheets[asset].state = _sheet_none;
		held_bytes -= levels->bytes;
		taken = 1;
	}
	pthread_mutex_unlock(&lock);
	return taken;
}

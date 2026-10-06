/*
RASTERIZER_XBOX_VERTEX_SHADERS.C

symbols in this file:
00293850 8744:
	_rdata_00293850 (0000)
0030CF88 0430:
	_vertex_shader_table (0000)
*/

/* ---------- headers */

#include "cseries.h"
#include "cseries/errors.h"
#include "rasterizer_xbox_vertex_shaders.h"

/* ---------- constants */

/* ---------- macros */

#define VERTEX_SHADER_ENTRY(offset, instruction_bytes) \
	{ 0, (unsigned char const *)vertex_shader_code + (offset), 0xFFFFFFFF, (instruction_bytes) }

/* ---------- structures */

/* ---------- prototypes */

/* ---------- globals */

/* The January object owns 67 compiled Xbox vertex-shader programs in this
34,628-byte read-only payload.  These are immutable Xbox shader instruction
tokens, represented as dwords so the generated data remains inspectable and
the compiler reproduces the original little-endian bytes. */
#if defined(HALO_PUBLIC_EXTERNAL_SHADERS)
/* This buffer contains no retail instruction tokens in the distributed ELF. */
#define PUBLIC_SHADER_BYTES 34628u
static unsigned long vertex_shader_code[PUBLIC_SHADER_BYTES / 4u];
#else
static unsigned long const vertex_shader_code[] =
{
#include "rasterizer_xbox_vertex_shaders_data.inc"
};
#endif

struct vertex_shader_entry vertex_shader_table[NUMBER_OF_VERTEX_SHADERS] =
{
	VERTEX_SHADER_ENTRY(0x0000, 0x074),
	VERTEX_SHADER_ENTRY(0x0078, 0x0C4),
	VERTEX_SHADER_ENTRY(0x0140, 0x1C4),
	VERTEX_SHADER_ENTRY(0x0308, 0x164),
	VERTEX_SHADER_ENTRY(0x0470, 0x084),
	VERTEX_SHADER_ENTRY(0x04F8, 0x174),
	VERTEX_SHADER_ENTRY(0x0670, 0x094),
	VERTEX_SHADER_ENTRY(0x0708, 0x184),
	VERTEX_SHADER_ENTRY(0x0890, 0x084),
	VERTEX_SHADER_ENTRY(0x0918, 0x2D4),
	VERTEX_SHADER_ENTRY(0x0BF0, 0x424),
	VERTEX_SHADER_ENTRY(0x1018, 0x1F4),
	VERTEX_SHADER_ENTRY(0x1210, 0x1D4),
	VERTEX_SHADER_ENTRY(0x13E8, 0x154),
	VERTEX_SHADER_ENTRY(0x1540, 0x2C4),
	VERTEX_SHADER_ENTRY(0x1808, 0x364),
	VERTEX_SHADER_ENTRY(0x1B70, 0x194),
	VERTEX_SHADER_ENTRY(0x1D08, 0x4F4),
	VERTEX_SHADER_ENTRY(0x2200, 0x274),
	VERTEX_SHADER_ENTRY(0x2478, 0x294),
	VERTEX_SHADER_ENTRY(0x2710, 0x0C4),
	VERTEX_SHADER_ENTRY(0x27D8, 0x134),
	VERTEX_SHADER_ENTRY(0x2910, 0x184),
	VERTEX_SHADER_ENTRY(0x2A98, 0x154),
	VERTEX_SHADER_ENTRY(0x2BF0, 0x254),
	VERTEX_SHADER_ENTRY(0x2E48, 0x374),
	VERTEX_SHADER_ENTRY(0x31C0, 0x144),
	VERTEX_SHADER_ENTRY(0x3308, 0x184),
	VERTEX_SHADER_ENTRY(0x3490, 0x224),
	VERTEX_SHADER_ENTRY(0x36B8, 0x0B4),
	VERTEX_SHADER_ENTRY(0x3770, 0x254),
	VERTEX_SHADER_ENTRY(0x39C8, 0x564),
	VERTEX_SHADER_ENTRY(0x3F30, 0x264),
	VERTEX_SHADER_ENTRY(0x4198, 0x1E4),
	VERTEX_SHADER_ENTRY(0x4380, 0x2D4),
	VERTEX_SHADER_ENTRY(0x4658, 0x2A4),
	VERTEX_SHADER_ENTRY(0x4900, 0x3B4),
	VERTEX_SHADER_ENTRY(0x4CB8, 0x0E4),
	VERTEX_SHADER_ENTRY(0x4DA0, 0x0B4),
	VERTEX_SHADER_ENTRY(0x4E58, 0x154),
	VERTEX_SHADER_ENTRY(0x4FB0, 0x0D4),
	VERTEX_SHADER_ENTRY(0x5088, 0x154),
	VERTEX_SHADER_ENTRY(0x51E0, 0x104),
	VERTEX_SHADER_ENTRY(0x52E8, 0x1F4),
	VERTEX_SHADER_ENTRY(0x54E0, 0x164),
	VERTEX_SHADER_ENTRY(0x5648, 0x3B4),
	VERTEX_SHADER_ENTRY(0x5A00, 0x184),
	VERTEX_SHADER_ENTRY(0x5B88, 0x374),
	VERTEX_SHADER_ENTRY(0x5F00, 0x254),
	VERTEX_SHADER_ENTRY(0x6158, 0x124),
	VERTEX_SHADER_ENTRY(0x6280, 0x484),
	VERTEX_SHADER_ENTRY(0x6708, 0x124),
	VERTEX_SHADER_ENTRY(0x6830, 0x104),
	VERTEX_SHADER_ENTRY(0x6938, 0x104),
	VERTEX_SHADER_ENTRY(0x6A40, 0x234),
	VERTEX_SHADER_ENTRY(0x6C78, 0x154),
	VERTEX_SHADER_ENTRY(0x6DD0, 0x094),
	VERTEX_SHADER_ENTRY(0x6E68, 0x3F4),
	VERTEX_SHADER_ENTRY(0x7260, 0x074),
	VERTEX_SHADER_ENTRY(0x72D8, 0x134),
	VERTEX_SHADER_ENTRY(0x7410, 0x374),
	VERTEX_SHADER_ENTRY(0x7788, 0x1E4),
	VERTEX_SHADER_ENTRY(0x7970, 0x3C4),
	VERTEX_SHADER_ENTRY(0x7D38, 0x394),
	VERTEX_SHADER_ENTRY(0x80D0, 0x294),
	VERTEX_SHADER_ENTRY(0x8368, 0x1A4),
	VERTEX_SHADER_ENTRY(0x8510, 0x234),
};

/* ---------- public code */

/* ---------- private code */

#if defined(HALO_PUBLIC_EXTERNAL_SHADERS)
#include <stdio.h>
#include <string.h>

/* Load once, before any CreateVertexShader call. On failure the caller aborts
   rasterizer initialization; zeros are never submitted as shader programs. */
boolean rasterizer_vertex_shader_assets_load(void)
{
    static int loaded;
    FILE *file;
    unsigned char *bytes = (unsigned char *)vertex_shader_code;
    unsigned long crc = 0xFFFFFFFFu;
    unsigned long index;
    int bit, extra, failed;
    size_t count;
    if (loaded) return loaded > 0;
    loaded = -1;
    file = fopen("d:\\maps\\shaders.bin", "rb");
    if (!file)
    {
        error(2, "PUBLIC ASSET ERROR: maps/shaders.bin missing or unreadable. Rerun the Halo CE asset importer with your supported retail game, then restart.");
        return FALSE;
    }
    count = fread(bytes, 1, PUBLIC_SHADER_BYTES, file);
    extra = fgetc(file);
    failed = ferror(file);
    if (fclose(file) != 0) failed = 1;
    if (count == PUBLIC_SHADER_BYTES && extra == EOF && !failed)
    {
        for (index = 0; index < PUBLIC_SHADER_BYTES; ++index)
        {
            crc ^= bytes[index];
            for (bit = 0; bit < 8; ++bit)
                crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
        }
        crc ^= 0xFFFFFFFFu;
        if (crc == 0x60EC8BA1u)
        {
            loaded = 1;
            return TRUE;
        }
    }
    memset(bytes, 0, PUBLIC_SHADER_BYTES);
    error(2, "PUBLIC ASSET ERROR: maps/shaders.bin has invalid length, checksum or read status. Rerun the Halo CE asset importer with your supported retail game, then restart.");
    return FALSE;
}
#endif

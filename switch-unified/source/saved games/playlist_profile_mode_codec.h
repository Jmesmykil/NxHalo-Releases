#ifndef PLAYLIST_PROFILE_MODE_CODEC_H
#define PLAYLIST_PROFILE_MODE_CODEC_H
#ifndef PLAYLIST_PROFILE_MODE_CODEC_TEST_TYPES
#include "cseries/cseries.h"
#endif
#define PLAYLIST_PROFILE_MODE_CODEC_VERSION 1
struct playlist_profile_mode_header
{
	word magic_low;
	word magic_high;
	word version;
	word size;
	short preset;
	short matchup;
};
typedef void (*playlist_profile_mode_checksum_proc)(byte *data, word size, byte *signature);
typedef char playlist_profile_mode_header_size_assert[
	sizeof(struct playlist_profile_mode_header) == 12 ? 1 : -1];
static boolean playlist_profile_mode_decode(byte const *block, unsigned long block_size,
	unsigned long offset, unsigned long signature_size,
	playlist_profile_mode_checksum_proc checksum_fn, short *preset, short *matchup)
{
	struct playlist_profile_mode_header header;
	byte expected[32];
	byte const *data;
	if (preset) *preset = 0;
	if (matchup) *matchup = 0;
	if (!block || !checksum_fn || signature_size == 0 || signature_size > sizeof(expected) ||
		offset > block_size || sizeof(header) + signature_size > block_size - offset)
		return FALSE;
	data = block + offset;
	csmemcpy(&header, data, sizeof(header));
	if (header.magic_low != 0x474E || header.magic_high != 0x4D54 ||
		header.version != PLAYLIST_PROFILE_MODE_CODEC_VERSION || header.size != 4 ||
		header.preset < 1 || header.preset > 7 ||
		header.matchup < 0 || header.matchup > 2)
		return FALSE;
	checksum_fn((byte *)data, (word)sizeof(header), expected);
	if (csmemcmp(expected, data + sizeof(header), signature_size))
		return FALSE;
	if (preset) *preset = header.preset;
	if (matchup) *matchup = header.matchup;
	return TRUE;
}
static boolean playlist_profile_mode_encode(byte *block, unsigned long block_size,
	unsigned long offset, unsigned long signature_size, short preset, short matchup,
	playlist_profile_mode_checksum_proc checksum_fn)
{
	struct playlist_profile_mode_header header;
	if (!block || !checksum_fn || signature_size == 0 || signature_size > 32 || offset > block_size ||
		sizeof(header) + signature_size > block_size - offset ||
		preset < 1 || preset > 7 || matchup < 0 || matchup > 2)
		return FALSE;
	header.magic_low = 0x474E;
	header.magic_high = 0x4D54;
	header.version = PLAYLIST_PROFILE_MODE_CODEC_VERSION;
	header.size = 4;
	header.preset = preset;
	header.matchup = matchup;
	csmemcpy(block + offset, &header, sizeof(header));
	checksum_fn(block + offset, (word)sizeof(header), block + offset + sizeof(header));
	return TRUE;
}
#endif

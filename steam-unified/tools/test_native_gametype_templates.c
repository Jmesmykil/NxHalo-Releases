#include <stdio.h>
#include <stdint.h>
#include <string.h>
typedef uint8_t byte; typedef uint16_t word; typedef int boolean;
#define TRUE 1
#define FALSE 0
#define NONE (-1L)
#define csmemcpy memcpy
#define csmemset memset
#define csmemcmp memcmp
#define MATCH_RULES_TEMPLATE_PROFILE_TEST_TYPES
#define PLAYLIST_PROFILE_MODE_CODEC_TEST_TYPES
#define TEST(expr) do { if (!(expr)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)
#include "match_rules_template_profile.h"
#include "saved games/playlist_profile_mode_codec.h"

static void checksum(byte *data, word size, byte *signature)
{
	uint32_t value = 2166136261U;
	word index;
	short slot;
	for (index = 0; index < size; index++)
	{
		value ^= data[index];
		value *= 16777619U;
	}
	for (slot = 0; slot < 8; slot++)
	{
		signature[slot] = (byte)(value >> ((slot & 3) * 8));
		value = value * 16777619U + slot;
	}
}

int main(void)
{
	long ids[MATCH_RULES_TEMPLATE_PROFILE_COUNT];
	byte block[512];
	short preset, matchup, decoded_preset, decoded_matchup;
	int index;
	csmemset(block, 0, sizeof(block));
	for (index = 0; index < MATCH_RULES_TEMPLATE_PROFILE_COUNT; index++)
	{
		ids[index] = match_rules_template_profile_encode((short)index);
		TEST(ids[index] != NONE);
		TEST(match_rules_template_profile_decode(ids[index]) == index);
		{
			short prior;
			for (prior = 0; prior < index; prior++) TEST(ids[prior] != ids[index]);
		}
		TEST((ids[index] & 0xC0000000UL) == 0xC0000000UL);
		TEST(match_rules_template_profile_decode(ids[index] ^ 0x40000000UL) == NONE);
		TEST(match_rules_template_profile_decode(ids[index] | 0x10000000UL) == NONE);
		TEST(match_rules_template_profile_decode(ids[index] | 0x00000100UL) == NONE);
		TEST(match_rules_template_profile_decode(ids[index] | 0x10UL) == NONE);
		TEST(match_rules_template_profile_decode(ids[index] | 0x80UL) == NONE);
		TEST(match_rules_template_profile_decode(ids[index] | 0x1000UL) == NONE);
		TEST(match_rules_template_profile_reserved_namespace(ids[index] | 0x10UL));
		TEST(match_rules_template_profile_reserved_namespace(ids[index]));
	}
	TEST(match_rules_template_profile_encode(-1) == NONE);
	TEST(match_rules_template_profile_encode(MATCH_RULES_TEMPLATE_PROFILE_COUNT) == NONE);
	TEST(match_rules_template_profile_decode(0xE0000002L) == NONE);
	TEST(!playlist_profile_mode_decode(block, sizeof(block), 0x140, 8, checksum, &preset, &matchup));
	block[0x100] = 0xA5; /* Existing GPVO region remains untouched. */
	TEST(playlist_profile_mode_encode(block, sizeof(block), 0x140, 8, 7, 2, checksum));
	TEST(block[0x100] == 0xA5);
	TEST(playlist_profile_mode_decode(block, sizeof(block), 0x140, 8, checksum, &preset, &matchup));
	TEST(preset == 7 && matchup == 2);
	TEST(!playlist_profile_mode_decode(block, sizeof(block), 0x140, 40, checksum, &preset, &matchup));
	TEST(!playlist_profile_mode_decode(block, sizeof(block), 0x140, 0, checksum, &preset, &matchup));
	TEST(!playlist_profile_mode_encode(block, sizeof(block), 0x140, 0, 1, 0, checksum));
	block[0x140 + 12] ^= 1;
	TEST(!playlist_profile_mode_decode(block, sizeof(block), 0x140, 8, checksum, &preset, &matchup));
	block[0x140 + 12] ^= 1;
	for (preset = 1; preset <= 7; preset++)
		for (matchup = 0; matchup <= 2; matchup++)
		{
			csmemset(block, 0, sizeof(block));
			TEST(playlist_profile_mode_encode(block, sizeof(block), 0x140, 8, preset, matchup, checksum));
			TEST(playlist_profile_mode_decode(block, sizeof(block), 0x140, 8, checksum, &decoded_preset, &decoded_matchup));
			TEST(decoded_preset == preset && decoded_matchup == matchup);
		}
	csmemset(block, 0, sizeof(block));
	TEST(playlist_profile_mode_encode(block, sizeof(block), 0x140, 8, 1, 0, checksum));
	block[0x144] = 2;
	TEST(!playlist_profile_mode_decode(block, sizeof(block), 0x140, 8, checksum, &preset, &matchup));
	csmemset(block, 0, sizeof(block));
	TEST(playlist_profile_mode_encode(block, sizeof(block), 0x140, 8, 1, 0, checksum));
	block[0x146] = 5;
	TEST(!playlist_profile_mode_decode(block, sizeof(block), 0x140, 8, checksum, &preset, &matchup));
	csmemset(block, 0, sizeof(block));
	TEST(playlist_profile_mode_encode(block, sizeof(block), 0x140, 8, 1, 0, checksum));
	block[0x148] = 0;
	block[0x149] = 0;
	TEST(!playlist_profile_mode_decode(block, sizeof(block), 0x140, 8, checksum, &preset, &matchup));
	csmemset(block, 0, sizeof(block));
	TEST(playlist_profile_mode_encode(block, sizeof(block), 0x140, 8, 1, 0, checksum));
	block[0x14A] = 3;
	TEST(!playlist_profile_mode_decode(block, sizeof(block), 0x140, 8, checksum, &preset, &matchup));
	TEST(!playlist_profile_mode_encode(block, sizeof(block), 500, 8, 1, 0, checksum));
	TEST(!playlist_profile_mode_encode(block, sizeof(block), 0x140, 8, 0, 0, checksum));
	puts("PASS: nine virtual IDs, alias rejection, legacy default, metadata bounds/checksum and round-trip");
	return 0;
}

#ifndef MATCH_RULES_TEMPLATE_PROFILE_H
#define MATCH_RULES_TEMPLATE_PROFILE_H
#ifndef MATCH_RULES_TEMPLATE_PROFILE_TEST_TYPES
#include "cseries.h"
#endif
#define MATCH_RULES_TEMPLATE_PROFILE_COUNT 9
static long match_rules_template_profile_encode(short index)
{
	if (index < 0 || index >= MATCH_RULES_TEMPLATE_PROFILE_COUNT) return NONE;
	return (long)0xE0000001UL | ((long)index << 16);
}
static boolean match_rules_template_profile_reserved_namespace(long profile)
{
	return (((unsigned long)profile & 0xF0000000UL) == 0xE0000000UL);
}
static short match_rules_template_profile_decode(long profile)
{
	unsigned long value = (unsigned long)profile;
	unsigned long index;
	if ((value & 0xF0000000UL) != 0xE0000000UL || (value & 0xF) != 1 ||
		(value & 0x0000FF00UL) != 0) return NONE;
	index = (value >> 16) & 0x0FFF;
	if (index >= MATCH_RULES_TEMPLATE_PROFILE_COUNT) return NONE;
	return match_rules_template_profile_encode((short)index) == profile ? (short)index : NONE;
}
#endif

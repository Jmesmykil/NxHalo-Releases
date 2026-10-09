#include <string.h>
#include <strings.h>
#include <assert.h>
#include <stdio.h>
typedef int boolean;
#define TRUE 1
#define FALSE 0
#define HALO_CUSTOM_EDITION 1
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
static unsigned int active_profile_version = 21;
static int network_profile_active_version(void) { return (int)active_profile_version; }
static boolean network_game_client_map_name_is_valid(
	char const *map_name,
	long size)
{
	/* (a scenario's tag path, of which the cache takes the name after the
	last backslash: letters, digits and a few more, none that a path reads
	otherwise) */
	char const *character;
	char const *leaf;
	char const *family;

	if (!memchr(map_name, '\0', size))
		return FALSE;
	if (!_strnicmp(map_name, "custom_maps\\", 12))
	{
		if (network_profile_active_version() != 24)
			return FALSE;
		leaf = map_name + 12;
		if (!*leaf || strlen(leaf) >= 64 || strchr(leaf, '\\') || strchr(leaf, '/') || strchr(leaf, '@') ||
			strstr(leaf, "..") || leaf[strlen(leaf) - 1] == '.' || leaf[strlen(leaf) - 1] == ' ')
			return FALSE;
		for (character = leaf; *character; character++)
		{
			if (!((*character >= 'a' && *character <= 'z') || (*character >= 'A' && *character <= 'Z') ||
				(*character >= '0' && *character <= '9') || *character == '_' || *character == '-' ||
				*character == '.' || *character == ' '))
				return FALSE;
		}
		return leaf[strspn(leaf, ". ")] != 0;
	}
	leaf = strrchr(map_name, '\\');
	leaf = leaf ? leaf+1 : map_name;
	family = strchr(map_name, '@');
	if(family) {
#ifdef HALO_CUSTOM_EDITION
        if(family<leaf || family==leaf || (_stricmp(family,"@ce") && _stricmp(family,"@md"))) return FALSE;
        for(character=leaf;character<family && (*character=='.' || *character==' ');character++) {}
        if(character==family) return FALSE;
#else
        return FALSE;
#endif
    }
	for (character = map_name; *character; character++)
	{
		if (!((*character >= 'a' && *character <= 'z') || (*character >= 'A' && *character <= 'Z') ||
			(*character >= '0' && *character <= '9') || *character == '_' || *character == '-' ||
			*character == '.' || *character == ' ' || *character == '\\' || (*character=='@' && character==family)))
		{
			return FALSE;
		}
	}
	if (strstr(map_name, ".."))
		return FALSE;
	leaf = strrchr(map_name, '\\');
	leaf = leaf ? leaf + 1 : map_name;
	return *leaf && leaf[strspn(leaf, ". ")] != 0;
}

int main(void) {
 const char *good[]={"bloodgulch","deathisland@ce","levels\\test\\deathisland@ce","map@md","Space Map@CE"};
 const char *bad[]={"@ce",".@ce","map@@ce","map@unknown","map@ce\\other","../map@ce","..\\map@ce","foo/bar@ce"};
 int i;char unterminated[32];memset(unterminated,'a',sizeof(unterminated));
 for(i=0;i<5;i++){char field[32]={0};strncpy(field,good[i],31);assert(network_game_client_map_name_is_valid(field,32));}
 for(i=0;i<8;i++){char field[32]={0};strncpy(field,bad[i],31);assert(!network_game_client_map_name_is_valid(field,32));}
 assert(!network_game_client_map_name_is_valid(unterminated,sizeof(unterminated)));
 active_profile_version=21;assert(!network_game_client_map_name_is_valid("custom_maps\\bloodgulch",32));
 active_profile_version=24;assert(network_game_client_map_name_is_valid("custom_maps\\bloodgulch",32));
 assert(network_game_client_map_name_is_valid("custom_maps\\Blood Gulch_2",32));
 assert(!network_game_client_map_name_is_valid("custom_maps\\..\\other",32));
 assert(!network_game_client_map_name_is_valid("custom_maps\\nested\\map",32));
 assert(!network_game_client_map_name_is_valid("custom_maps\\name.",32));
 assert(!network_game_client_map_name_is_valid("custom_maps\\name@ce",32));
 { char long_map[12+65+1];memcpy(long_map,"custom_maps\\",12);memset(long_map+12,'x',65);long_map[77]=0;
   assert(!network_game_client_map_name_is_valid(long_map,sizeof(long_map))); }
 puts("PASS: map validation preserves legacy suffixes and gates flat custom_maps identifiers to profile 24");return 0;
}

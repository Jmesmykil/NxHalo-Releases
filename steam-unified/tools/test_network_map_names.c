#include <string.h>
#include <strings.h>
#include <assert.h>
#include <stdio.h>
typedef int boolean;
#define TRUE 1
#define FALSE 0
#define HALO_CUSTOM_EDITION 1
#define _stricmp strcasecmp
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
 puts("PASS: production network map validation permits known CE/MD suffixes and rejects traversal/unknown suffixes");return 0;
}

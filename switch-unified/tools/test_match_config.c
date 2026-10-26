#include "../port/linux/src/port_config.c"
#include <assert.h>
void platform_log(const char *format,...) { (void)format; }
const char *SDLCALL SDL_GetBasePath(void) { return getenv("HALO_CONFIG_TEST_BASE"); }
char *_strdup(const char *s) { size_t n=strlen(s)+1;char *p=malloc(n);if(p)memcpy(p,s,n);return p;}
int main(void) {
 assert(config_integer("mods.match_preset")==0);
 assert(config_integer("mods.faction_matchup")==0);
 assert(config_write("mods.match_preset","5"));
 assert(config_write("mods.faction_matchup","2"));
 assert(config_integer("mods.match_preset")==5 && config_integer("mods.faction_matchup")==2);
 config_loaded=0;
 assert(config_integer("mods.match_preset")==5 && config_integer("mods.faction_matchup")==2);
 assert(config_integer("mods.character")==0);
 assert(!config_write("mods.nonexistent_probe","1"));
 puts("PASS: preset and faction registry defaults, write, reload and standard character preservation"); return 0;
}

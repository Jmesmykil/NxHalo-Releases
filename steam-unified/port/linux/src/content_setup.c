#include "content_setup.h"
#include "SDL3/SDL.h"
#include "../game/community_map_download.h"
#include "platform.h"
#include <stdio.h>
#include <string.h>
#define CONTENT_TARGET_DIRECTORY "d:\\maps\\ce\\"
static char content_status_text[192]="CE content: .map / ZIP + resource companions\r\nOpenSauce .yelo / DLL extensions need separate ports.";
static char const *content_basename(char const *path) { char const *base=path,*p; for(p=path;*p;p++) if(*p=='/'||*p=='\\')base=p+1; return base; }
static void content_file_selected(void *ctx,char const * const *files,int count) {
 char const *base;(void)ctx;(void)count;if(!files||!files[0])return;base=content_basename(files[0]);
 if(!SDL_strcasecmp(base,"bitmaps.map")||!SDL_strcasecmp(base,"sounds.map")||!SDL_strcasecmp(base,"loc.map")) community_map_download_import_resource(files[0],CONTENT_TARGET_DIRECTORY);
 else community_map_download_install_local(files[0],CONTENT_TARGET_DIRECTORY);
}
void content_setup_import_local(void) {
 static const SDL_DialogFileFilter filters[]={{"Halo CE maps and ZIP archives","map;zip"},{"Resource maps","map"}};
 SDL_ShowOpenFileDialog(content_file_selected,NULL,NULL,filters,2,NULL,false);
}
static int content_safe_map_name(char const *in,char *out,size_t size) {
 char const *start=in,*end;size_t n,i;if(!in)return 0;end=strstr(in,"map=");
 if(end){start=end+4;end=start;while(*end&&*end!='&'&&*end!='#'&&*end!=' '&&*end!='\r'&&*end!='\n')end++;}
 else{while(*start==' '||*start=='\t'||*start=='\r'||*start=='\n')start++;end=start+strlen(start);while(end>start&&(end[-1]==' '||end[-1]=='\t'||end[-1]=='\r'||end[-1]=='\n'))end--;}
 n=(size_t)(end-start);if(n>4&&!SDL_strncasecmp(start+n-4,".map",4))n-=4;if(!n||n>=size)return 0;
 for(i=0;i<n;i++){char c=start[i];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))return 0;out[i]=c;}
 out[n]=0;return strcmp(out,".")&&strcmp(out,"..");
}
int content_setup_download_map(const char *map_name) {
 char name[64],target[128];if(!content_safe_map_name(map_name,name,sizeof(name))){snprintf(content_status_text,sizeof(content_status_text),"Enter a valid Halo CE map name or locator link.");return 0;}
 snprintf(target,sizeof(target),"d:\\maps\\ce\\%s.map",name);return community_map_download_start(name,target);
}
void content_setup_download_clipboard(void){char *p=SDL_GetClipboardText();if(p){content_setup_download_map(p);SDL_free(p);}}
void content_setup_open_folder(void) {
 char folder[1024],url[3072],parent[1024];const char *root=platform_data_root();size_t i,n=0;if(!root)return;
 snprintf(parent,sizeof(parent),"%s/maps",root);SDL_CreateDirectory(parent);snprintf(folder,sizeof(folder),"%s/maps/ce",root);SDL_CreateDirectory(folder);
 memcpy(url,"file://",7);n=7;for(i=0;folder[i]&&n+4<sizeof(url);i++){unsigned char c=(unsigned char)folder[i];if(c==' '){url[n++]='%';url[n++]='2';url[n++]='0';}else url[n++]=(char)c;}url[n]=0;
 if(!SDL_OpenURL(url))snprintf(content_status_text,sizeof(content_status_text),"Could not open CE maps folder: %s",SDL_GetError());
}
char const *content_setup_status(void){char msg[192];int state=community_map_download_status(msg,sizeof(msg));if(state!=COMMUNITY_MAP_DOWNLOAD_IDLE)snprintf(content_status_text,sizeof(content_status_text),"%s",msg);return content_status_text;}
void content_setup_clear_status(void){community_map_download_clear();content_status_text[0]=0;}

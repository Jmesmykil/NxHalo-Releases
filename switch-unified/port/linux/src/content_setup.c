#include "content_setup.h"
#include "SDL3/SDL.h"
#include "../game/community_map_download.h"
#include "platform.h"
#include <stdio.h>
#include <string.h>
#ifdef HALO_SWITCH_GUEST
#include <dirent.h>
#endif
#define CONTENT_TARGET_DIRECTORY "d:\\maps\\ce\\"
static char content_status_text[192]="CE content: .map / ZIP + resource companions\r\nOpenSauce .yelo / DLL extensions need separate ports.";
static char const *content_basename(char const *path) { char const *base=path,*p; for(p=path;*p;p++) if(*p=='/'||*p=='\\')base=p+1; return base; }
static void content_file_selected(void *ctx,char const * const *files,int count) {
 char const *base;(void)ctx;(void)count;if(!files||!files[0])return;base=content_basename(files[0]);
 if(!SDL_strcasecmp(base,"bitmaps.map")||!SDL_strcasecmp(base,"sounds.map")||!SDL_strcasecmp(base,"loc.map")) community_map_download_import_resource(files[0],CONTENT_TARGET_DIRECTORY);
 else community_map_download_install_local(files[0],CONTENT_TARGET_DIRECTORY);
}
void content_setup_import_local(void) {
#ifdef HALO_SWITCH_GUEST
	static char const directory[] = "sdmc:/switch/halo/import";
	char names[64][128];
	char selected_path[320];
	SDL_MessageBoxButtonData buttons[8];
	SDL_MessageBoxData dialog;
	struct dirent *entry;
	DIR *dir;
	int found = 0, page = 0;
	SDL_CreateDirectory(directory);
	dir = opendir(directory);
	if (!dir) {
		snprintf(content_status_text, sizeof(content_status_text), "Cannot read %s. Create it on the SD card and add .map or .zip files.", directory);
		SDL_ShowSimpleMessageBox(0, "Import Halo CE content", content_status_text, NULL);
		return;
	}
	while ((entry = readdir(dir)) != NULL && found < 64) {
		size_t length = strlen(entry->d_name);
		if (length < 5 || length >= sizeof(names[0])) continue;
		if (SDL_strcasecmp(entry->d_name + length - 4, ".map") &&
			SDL_strcasecmp(entry->d_name + length - 4, ".zip")) continue;
		snprintf(names[found++], sizeof(names[0]), "%s", entry->d_name);
	}
	closedir(dir);
	if (!found) {
		snprintf(content_status_text, sizeof(content_status_text), "Place a .map or .zip in %s, then choose Import again.", directory);
		SDL_ShowSimpleMessageBox(0, "No import files found", content_status_text, NULL);
		return;
	}
	while (page * 6 < found) {
		int count = 0, selected;
		int start = page * 6, end = start + 6 < found ? start + 6 : found;
		for (int i = start; i < end; i++) {
			buttons[count].flags = 0;
			buttons[count].buttonID = i;
			buttons[count++].text = names[i];
		}
		if (end < found) {
			buttons[count].flags = 0;
			buttons[count].buttonID = 1000;
			buttons[count++].text = "Next page";
		}
		buttons[count].flags = 0;
		buttons[count].buttonID = 1001;
		buttons[count++].text = "Cancel";
		memset(&dialog, 0, sizeof(dialog));
		dialog.title = "Import Halo CE content from SD";
		dialog.message = directory;
		dialog.numbuttons = count;
		dialog.buttons = buttons;
		selected = -1;
		if (!SDL_ShowMessageBox(&dialog, &selected) || selected == 1001) return;
		if (selected == 1000) { page++; continue; }
		if (selected < start || selected >= end) return;
		snprintf(selected_path, sizeof(selected_path), "%s/%s", directory, names[selected]);
		{
			char const *file = selected_path;
			content_file_selected(NULL, &file, 1);
		}
		return;
	}
#else
	static const SDL_DialogFileFilter filters[]={{"Halo CE maps and ZIP archives","map;zip"},{"Resource maps","map"}};
	SDL_ShowOpenFileDialog(content_file_selected,NULL,NULL,filters,2,NULL,false);
#endif
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
 char name[64],target[128];
 if(!content_safe_map_name(map_name,name,sizeof(name))){
  snprintf(content_status_text,sizeof(content_status_text),"Enter a map name or HaloNet locator URL containing map=; other catalog URLs are not direct map IDs.");
  fprintf(stderr,"halo-linux: CE map download request rejected: expected a map name or HaloNet locator URL with map=\n");
  return 0;
 }
 snprintf(target,sizeof(target),"d:\\maps\\ce\\%s.map",name);
 if(!community_map_download_start(name,target)){
  snprintf(content_status_text,sizeof(content_status_text),"Could not start the map download; another content job may already be running.");
  fprintf(stderr,"halo-linux: could not start CE map download for %s\n",name);
  return 0;
 }
 return 1;
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

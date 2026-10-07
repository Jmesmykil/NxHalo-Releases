#include "content_setup.h"
#include "SDL3/SDL.h"
#include "../game/community_map_download.h"
#include "platform.h"
#include "update.h"
#include "texture_pack.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
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

/* HaloNet's repository index is fetched off-thread through bounded HTTPS. */
#define CONTENT_CATALOG_URL "https://maps.halonet.net/maplist.php?fulllist=y"
#define CONTENT_CATALOG_MAX_BYTES (8*1024*1024)
#define CONTENT_CATALOG_MAX_ROWS 8192
#define CONTENT_CATALOG_NAME_SIZE 64
#define CONTENT_CATALOG_REFRESH_MS (6ULL*60*60*1000)
struct content_catalog_item { char name[64],type[16]; };
static struct content_catalog_item catalog[CONTENT_CATALOG_MAX_ROWS];
static int catalog_count,catalog_loaded,catalog_running,catalog_page;
static char catalog_query[48],catalog_message[160]="Map catalog has not been loaded",catalog_cache[512];
static SDL_Thread *catalog_thread; static Uint64 catalog_next;
static pthread_mutex_t catalog_lock=PTHREAD_MUTEX_INITIALIZER;
static int catalog_hex(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
static int catalog_name(const char *s,size_t n,char *out,size_t cap){
 char v[64];size_t i,k=0;if(n<5||n>=sizeof(v)||SDL_strncasecmp(s+n-4,".zip",4))return 0;
 n-=4;
 for(i=0;i<n;i++){unsigned char c=(unsigned char)s[i];if(c=='%'){int a,b;if(i+2>=n||(a=catalog_hex(s[i+1]))<0||(b=catalog_hex(s[i+2]))<0)return 0;c=a*16+b;i+=2;}
  if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return 0;v[k++]=c;}
 v[k]=0;if(!k||k>=cap)return 0;memcpy(out,v,k+1);return 1;
}
static int catalog_cell(const char *a,const char *b,const char *want){
 char v[32];int tag=0;size_t n=0;while(a<b&&*a){if(*a=='<'){tag=1;a++;continue;}if(*a=='>'){tag=0;a++;continue;}if(!tag&&!isspace((unsigned char)*a)&&n+1<sizeof(v))v[n++]=*a++;else a++;}v[n]=0;return !SDL_strcasecmp(v,want);
}
static int catalog_has(const char *h,const char *n){size_t z=strlen(n);for(;*h;h++)if(!strncasecmp(h,n,z))return 1;return !z;}
/* Allow only safe single-component CE Multiplayer/Campaign map rows. */
static int catalog_parse(const char *html,size_t size,struct content_catalog_item *out){
 const char *p=html,*lim=html+size;int count=0;
 while(p<lim){const char *r=strstr(p,"<tr"),*end,*h,*q,*base,*x,*a,*cell,*ce,*tc,*te;char name[64];
  if(!r||r>=lim)break;end=strstr(r,"</tr>");if(!end||end>lim)break;h=strstr(r,"href=\"/maps/");
  if(h&&h<end){h+=11;q=strchr(h,'\"');if(q&&q<end){base=h;for(x=h;x<q;x++)if(*x=='/')base=x+1;
   if(q-base>=4&&!SDL_strncasecmp(q-4,".zip",4)&&catalog_name(base,q-base,name,sizeof(name))){a=strstr(q,"</a>");cell=a?strstr(a,"<td"):NULL;
    if(cell&&cell<end&&(cell=strchr(cell,'>'))&&cell<end){cell++;ce=strstr(cell,"</td>");if(ce&&ce<end&&catalog_cell(cell,ce,"CE")){
     tc=strstr(ce,"<td");te=NULL;if(tc&&tc<end&&(tc=strchr(tc,'>'))&&tc<end){tc++;te=strstr(tc,"</td>");}
     if(te&&te<end&&(catalog_cell(tc,te,"Multiplayer")||catalog_cell(tc,te,"Campaign"))&&count<CONTENT_CATALOG_MAX_ROWS){
      snprintf(out[count].name,sizeof(out[count].name),"%s",name);snprintf(out[count].type,sizeof(out[count].type),"%s",catalog_cell(tc,te,"Campaign")?"Campaign":"Multiplayer");count++;}
    }}
   }}
  }p=end+5;
 }return count;
}
static int catalog_file(const char *path,struct content_catalog_item *out){
 FILE *f=fopen(path,"rb");char *buf;long n;int count;if(!f)return -1;
 if(fseek(f,0,SEEK_END)||(n=ftell(f))<0||n>CONTENT_CATALOG_MAX_BYTES||fseek(f,0,SEEK_SET)){fclose(f);return -1;}
 buf=malloc((size_t)n+1);if(!buf){fclose(f);return -1;}if(fread(buf,1,n,f)!=(size_t)n){free(buf);fclose(f);return -1;}
 fclose(f);buf[n]=0;count=catalog_parse(buf,n,out);free(buf);return count;
}
static void catalog_publish(struct content_catalog_item *v,int n,const char *msg){
 pthread_mutex_lock(&catalog_lock);if(n>=0){memcpy(catalog,v,(size_t)n*sizeof(v[0]));catalog_count=n;catalog_loaded=1;}
 snprintf(catalog_message,sizeof(catalog_message),"%s",msg);pthread_mutex_unlock(&catalog_lock);
}
static int catalog_worker(void *unused){
 struct content_catalog_item *v=malloc(sizeof(catalog));char err[160]="",temp[560];struct _stat st;int n=-1,stale=1,had=0;time_t now=time(NULL);(void)unused;
 if(!v)goto finish;n=catalog_file(catalog_cache,v);if(n>0){char m[120];had=1;snprintf(m,sizeof(m),"HaloNet CE catalog cached (%d maps)",n);catalog_publish(v,n,m);
  if(_stat(catalog_cache,&st)==0&&now>=st.st_mtime&&now-st.st_mtime<21600)stale=0;}else n=-1;
 if(!stale){goto finish;}if(snprintf(temp,sizeof(temp),"%s.pending",catalog_cache)>=(int)sizeof(temp))goto failed;
#if defined(__linux__) && !defined(HALO_ANDROID)
 if(!update_fetch_text_limited(CONTENT_CATALOG_URL,temp,CONTENT_CATALOG_MAX_BYTES,NULL,NULL,err,sizeof(err)))goto failed;
#else
 if(!update_download_limited(CONTENT_CATALOG_URL,temp,CONTENT_CATALOG_MAX_BYTES,NULL,NULL,err,sizeof(err)))goto failed;
#endif
 n=catalog_file(temp,v);if(n<=0){snprintf(err,sizeof(err),"catalog contained no supported CE maps");unlink(temp);goto failed;}
 if(rename(temp,catalog_cache)){snprintf(err,sizeof(err),"could not save validated catalog");unlink(temp);goto failed;}
 {char m[120];snprintf(m,sizeof(m),"HaloNet CE catalog ready (%d maps)",n);catalog_publish(v,n,m);}goto finish;
failed: if(had){pthread_mutex_lock(&catalog_lock);catalog_next=SDL_GetTicks()+600000;snprintf(catalog_message,sizeof(catalog_message),"Cached catalog retained; refresh failed: %.72s",err[0]?err:"network error");pthread_mutex_unlock(&catalog_lock);}
 else {char m[160];snprintf(m,sizeof(m),"Map catalog unavailable: %.110s",err[0]?err:"cache/network error");catalog_publish(v,0,m);pthread_mutex_lock(&catalog_lock);catalog_next=SDL_GetTicks()+15000;pthread_mutex_unlock(&catalog_lock);}
finish:free(v);pthread_mutex_lock(&catalog_lock);catalog_running=0;pthread_mutex_unlock(&catalog_lock);return 0;
}
void content_setup_catalog_poll(void){
 SDL_Thread *old=NULL;Uint64 now=SDL_GetTicks();char *pref;
 pthread_mutex_lock(&catalog_lock);if(catalog_thread&&!catalog_running){old=catalog_thread;catalog_thread=NULL;}pthread_mutex_unlock(&catalog_lock);if(old)SDL_WaitThread(old,NULL);
 pthread_mutex_lock(&catalog_lock);if(catalog_running||now<catalog_next){pthread_mutex_unlock(&catalog_lock);return;}
 pref=SDL_GetPrefPath("OpenCE","MapCatalog");if(!pref){snprintf(catalog_message,sizeof(catalog_message),"Could not prepare catalog cache");catalog_next=now+CONTENT_CATALOG_REFRESH_MS;pthread_mutex_unlock(&catalog_lock);return;}
 if(snprintf(catalog_cache,sizeof(catalog_cache),"%shalonet-maplist.html",pref)>=(int)sizeof(catalog_cache)){SDL_free(pref);snprintf(catalog_message,sizeof(catalog_message),"Catalog cache path too long");catalog_next=now+CONTENT_CATALOG_REFRESH_MS;pthread_mutex_unlock(&catalog_lock);return;}
 SDL_free(pref);catalog_running=1;catalog_next=now+CONTENT_CATALOG_REFRESH_MS;catalog_thread=SDL_CreateThread(catalog_worker,"HaloNet map catalog",NULL);
 if(!catalog_thread){catalog_running=0;snprintf(catalog_message,sizeof(catalog_message),"Could not start catalog request");}pthread_mutex_unlock(&catalog_lock);
}
void content_setup_catalog_set_query(const char *q){pthread_mutex_lock(&catalog_lock);snprintf(catalog_query,sizeof(catalog_query),"%.47s",q?q:"");catalog_page=0;pthread_mutex_unlock(&catalog_lock);}
const char *content_setup_catalog_query(void){return catalog_query;}
static int catalog_match(const char *n){return !catalog_query[0]||catalog_has(n,catalog_query);}
int content_setup_catalog_page_move(int d){int count=0,i,max;pthread_mutex_lock(&catalog_lock);for(i=0;i<catalog_count;i++)if(catalog_match(catalog[i].name))count++;
 max=count?(count-1)/CONTENT_CATALOG_PAGE_SIZE:0;catalog_page+=d;if(catalog_page<0)catalog_page=0;if(catalog_page>max)catalog_page=max;i=catalog_page;pthread_mutex_unlock(&catalog_lock);return i;}
int content_setup_catalog_result(int row,char *name,size_t ns,char *type,size_t ts){
 int i,match=0,target;if(row<0||row>=CONTENT_CATALOG_PAGE_SIZE||!name||!ns)return 0;pthread_mutex_lock(&catalog_lock);target=catalog_page*CONTENT_CATALOG_PAGE_SIZE+row;
 for(i=0;i<catalog_count;i++)if(catalog_match(catalog[i].name)&&match++==target){snprintf(name,ns,"%s",catalog[i].name);if(type&&ts)snprintf(type,ts,"%s",catalog[i].type);pthread_mutex_unlock(&catalog_lock);return 1;}
 pthread_mutex_unlock(&catalog_lock);return 0;
}

int content_setup_catalog_installed(const char *name) {
 char path[1200];const char *root=platform_data_root();struct _stat st;
 if(!root||!name||!name[0])return 0;
 if(snprintf(path,sizeof(path),"%s/maps/ce/%s.map",root,name)>=(int)sizeof(path))return 0;
 return _stat(path,&st)==0&&(st.st_mode&0170000)==0100000;
}
int content_setup_catalog_download_row(int row){char n[64],t[16];if(!content_setup_catalog_result(row,n,sizeof(n),t,sizeof(t)))return 0;return content_setup_download_map(n);}
const char *content_setup_catalog_status(void){static _Thread_local char out[192];int n=0,i;pthread_mutex_lock(&catalog_lock);for(i=0;i<catalog_count;i++)if(catalog_match(catalog[i].name))n++;
 snprintf(out,sizeof(out),"%s | %d matches | page %d/%d",catalog_message,n,catalog_page+1,n?(n-1)/CONTENT_CATALOG_PAGE_SIZE+1:1);pthread_mutex_unlock(&catalog_lock);return out;}



#define CONTENT_PACK_ROWS 24
static char content_pack_names[CONTENT_PACK_ROWS][64];
static int content_pack_count;
static Uint64 content_pack_next_refresh;
static char content_pack_message[128]="No texture packs found";
static pthread_mutex_t content_pack_lock=PTHREAD_MUTEX_INITIALIZER;
static SDL_Thread *content_pack_thread;
static int content_pack_running,content_pack_finished,content_pack_import_ok;
static char content_pack_import_path[1024],content_pack_import_name[64];
static void content_pack_collect(const char *name,void *context) {
 (void)context;if(content_pack_count<CONTENT_PACK_ROWS)snprintf(content_pack_names[content_pack_count++],sizeof(content_pack_names[0]),"%s",name);
}
static int content_pack_import_worker(void *unused) {
 char path[1024],name[64];int ok;(void)unused;
 pthread_mutex_lock(&content_pack_lock);snprintf(path,sizeof(path),"%s",content_pack_import_path);snprintf(name,sizeof(name),"%s",content_pack_import_name);pthread_mutex_unlock(&content_pack_lock);
 ok=texture_pack_install(name,path);
 pthread_mutex_lock(&content_pack_lock);content_pack_import_ok=ok;content_pack_finished=1;content_pack_running=0;pthread_mutex_unlock(&content_pack_lock);return 0;
}
static void content_pack_apply_pending(void) {
 SDL_Thread *finished=NULL;int apply=0,ok=0;char name[64];
 pthread_mutex_lock(&content_pack_lock);
 if(content_pack_thread&&content_pack_finished){finished=content_pack_thread;content_pack_thread=NULL;apply=1;ok=content_pack_import_ok;snprintf(name,sizeof(name),"%s",content_pack_import_name);content_pack_finished=0;}
 pthread_mutex_unlock(&content_pack_lock);
 if(finished)SDL_WaitThread(finished,NULL);
 if(apply){
  if(ok&&texture_pack_select(name)){texture_pack_set_enabled(1);snprintf(content_pack_message,sizeof(content_pack_message),"Imported, selected and enabled: %.80s",name);}
  else if(ok)snprintf(content_pack_message,sizeof(content_pack_message),"Imported %.70s; choose it below to activate",name);
  else snprintf(content_pack_message,sizeof(content_pack_message),"Import failed; use relative tag keys and PNG/TGA/DDS replacements");
  content_pack_next_refresh=0;
 }
}
void content_setup_texture_pack_refresh(void) {
 Uint64 now=SDL_GetTicks();content_pack_apply_pending();if(now<content_pack_next_refresh)return;content_pack_next_refresh=now+1000;
 content_pack_count=0;texture_pack_list(content_pack_collect,NULL);
 if(!content_pack_count)snprintf(content_pack_message,sizeof(content_pack_message),"No installed texture packs");
}
int content_setup_texture_pack_count(void){content_setup_texture_pack_refresh();return content_pack_count;}
int content_setup_texture_pack_name(int row,char *name,size_t size){content_setup_texture_pack_refresh();if(row<0||row>=content_pack_count||!name||!size)return 0;snprintf(name,size,"%s",content_pack_names[row]);return 1;}
int content_setup_texture_pack_select_row(int row){char name[64];if(!content_setup_texture_pack_name(row,name,sizeof(name)))return 0;return texture_pack_select(name);}
int content_setup_texture_pack_toggle(void){return texture_pack_set_enabled(!texture_pack_enabled());}
const char *content_setup_texture_pack_status(void) {
 static _Thread_local char line[256];char message[128];content_setup_texture_pack_refresh();
 pthread_mutex_lock(&content_pack_lock);if(content_pack_running)snprintf(content_pack_message,sizeof(content_pack_message),"Importing texture pack in background");
 snprintf(message,sizeof(message),"%s",content_pack_message);pthread_mutex_unlock(&content_pack_lock);
 snprintf(line,sizeof(line),"Texture packs: %s | Selected: %s | %s. Pack keys: relative tag path plus __bitmap-index.png/.tga/.dds.",
  texture_pack_enabled()?"ON":"OFF",texture_pack_selected()[0]?texture_pack_selected():"none",message);return line;
}
static void content_pack_folder_selected(void *ctx,const char * const *paths,int count) {
 char path[1024],name[64];const char *base;size_t n;(void)ctx;if(!paths||count<1||!paths[0])return;
 base=content_basename(paths[0]);n=strlen(base);while(n&&base[n-1]=='/')n--;if(!n||n>=sizeof(name)){pthread_mutex_lock(&content_pack_lock);snprintf(content_pack_message,sizeof(content_pack_message),"Texture pack folder name is not usable");pthread_mutex_unlock(&content_pack_lock);return;}
 memcpy(name,base,n);name[n]=0;snprintf(path,sizeof(path),"%s",paths[0]);
 pthread_mutex_lock(&content_pack_lock);
 if(content_pack_running){snprintf(content_pack_message,sizeof(content_pack_message),"A texture pack import is already running");pthread_mutex_unlock(&content_pack_lock);return;}
 snprintf(content_pack_import_path,sizeof(content_pack_import_path),"%s",path);snprintf(content_pack_import_name,sizeof(content_pack_import_name),"%s",name);
 content_pack_running=1;content_pack_finished=0;content_pack_thread=SDL_CreateThread(content_pack_import_worker,"texture pack import",NULL);
 if(!content_pack_thread){content_pack_running=0;snprintf(content_pack_message,sizeof(content_pack_message),"Could not start texture pack import");}
 else snprintf(content_pack_message,sizeof(content_pack_message),"Importing texture pack in background");
 pthread_mutex_unlock(&content_pack_lock);
}
void content_setup_texture_pack_import(void) { SDL_ShowOpenFolderDialog(content_pack_folder_selected,NULL,NULL,NULL,false); }

int content_setup_deck_upscaling_available(void) {
 const char *role=getenv("HALO_DEVICE_ROLE");char vendor[128]="",product[128]="";FILE *f;
 if(role&&!strcmp(role,"steam_deck"))return 1;
 f=fopen("/sys/class/dmi/id/sys_vendor","r");if(f){fgets(vendor,sizeof(vendor),f);fclose(f);}
 f=fopen("/sys/class/dmi/id/product_name","r");if(f){fgets(product,sizeof(product),f);fclose(f);}
 vendor[strcspn(vendor,"\r\n")]=0;product[strcspn(product,"\r\n")]=0;
 return !strcasecmp(vendor,"Valve")&&(!strcasecmp(product,"Jupiter")||!strcasecmp(product,"Galileo"));
}

char const *content_setup_status(void){char msg[192];int state=community_map_download_status(msg,sizeof(msg));if(state!=COMMUNITY_MAP_DOWNLOAD_IDLE)snprintf(content_status_text,sizeof(content_status_text),"%s",msg);return content_status_text;}
void content_setup_clear_status(void){community_map_download_clear();content_status_text[0]=0;}

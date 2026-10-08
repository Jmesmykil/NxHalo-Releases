#include "content_setup.h"
#include "SDL3/SDL.h"
#include "../game/community_map_download.h"
#include "platform.h"
#include "update.h"
#include "texture_pack.h"
#include "port_config.h"
#include "posix.h"
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <limits.h>
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
static int catalog_category,catalog_install_filter,catalog_sort_desc;
static unsigned char catalog_installed[CONTENT_CATALOG_MAX_ROWS];
static Uint64 catalog_installed_next;
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
static int catalog_compare(const void *a,const void *b){
 const struct content_catalog_item *x=a,*y=b;int n=strcasecmp(x->name,y->name);return n?n:strcmp(x->name,y->name);
}
static void catalog_publish(struct content_catalog_item *v,int n,const char *msg){
 pthread_mutex_lock(&catalog_lock);if(n>=0){memcpy(catalog,v,(size_t)n*sizeof(v[0]));catalog_count=n;catalog_loaded=1;qsort(catalog,(size_t)n,sizeof(catalog[0]),catalog_compare);catalog_installed_next=0;}
 snprintf(catalog_message,sizeof(catalog_message),"%s",msg);pthread_mutex_unlock(&catalog_lock);
}
static int catalog_worker(void *unused){
 struct content_catalog_item *v=malloc(sizeof(catalog));char err[160]="",temp[560];struct posix_file_information st;int n=-1,stale=1,had=0;time_t now=time(NULL);(void)unused;
 if(!v)goto finish;n=catalog_file(catalog_cache,v);if(n>0){char m[120];had=1;snprintf(m,sizeof(m),"HaloNet CE catalog cached (%d maps)",n);catalog_publish(v,n,m);
  if(posix_stat(catalog_cache,&st)==0&&now>=st.modification_seconds&&now-st.modification_seconds<21600)stale=0;}else n=-1;
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
const char *content_setup_catalog_query(void){static _Thread_local char q[48];pthread_mutex_lock(&catalog_lock);snprintf(q,sizeof(q),"%s",catalog_query);pthread_mutex_unlock(&catalog_lock);return q;}
void content_setup_catalog_category_cycle(int d){pthread_mutex_lock(&catalog_lock);catalog_category=(catalog_category+(d<0?2:1))%3;catalog_page=0;pthread_mutex_unlock(&catalog_lock);}
const char *content_setup_catalog_category(void){static const char *labels[]={"All","Multiplayer","Campaign"};return labels[catalog_category];}
void content_setup_catalog_install_filter_cycle(int d){pthread_mutex_lock(&catalog_lock);catalog_install_filter=(catalog_install_filter+(d<0?2:1))%3;catalog_page=0;catalog_installed_next=0;pthread_mutex_unlock(&catalog_lock);}
const char *content_setup_catalog_install_filter(void){static const char *labels[]={"All","Installed","Not installed"};return labels[catalog_install_filter];}
void content_setup_catalog_sort_cycle(void){pthread_mutex_lock(&catalog_lock);catalog_sort_desc=!catalog_sort_desc;catalog_page=0;pthread_mutex_unlock(&catalog_lock);}
const char *content_setup_catalog_sort(void){return catalog_sort_desc?"Name Z-A":"Name A-Z";}
int content_setup_catalog_installed(const char *name) {
 char path[1200],safe[64];const char *root=platform_data_root();int fd,regular;
 if(!root||!content_safe_map_name(name,safe,sizeof(safe))||strcmp(name,safe))return 0;
 if(snprintf(path,sizeof(path),"%s/maps/ce/%s.map",root,safe)>=(int)sizeof(path))return 0;
 /* Installed maps may use the same read-only links as the engine's map root.
    Native host paths must not pass through the Xbox-path _stat wrapper. */
 fd=open(path,O_RDONLY|O_CLOEXEC|O_NONBLOCK);if(fd<0)return 0;
 regular=posix_file_is_regular(fd);close(fd);return regular;
}
/* Called under catalog_lock. Cache filesystem checks for one second while filtering. */
static void catalog_refresh_installed(void){int i;Uint64 now=SDL_GetTicks();if(!catalog_install_filter||now<catalog_installed_next)return;
 for(i=0;i<catalog_count;i++)catalog_installed[i]=(unsigned char)content_setup_catalog_installed(catalog[i].name);
 catalog_installed_next=now+1000;
}
static int catalog_match(int i){return (!catalog_query[0]||catalog_has(catalog[i].name,catalog_query))&&
 (!catalog_category||!strcmp(catalog[i].type,catalog_category==1?"Multiplayer":"Campaign"))&&
 (!catalog_install_filter||(catalog_install_filter==1?catalog_installed[i]:!catalog_installed[i]));}
static int catalog_matches(void){int n=0,i,max;catalog_refresh_installed();for(i=0;i<catalog_count;i++)if(catalog_match(i))n++;
 max=n?(n-1)/CONTENT_CATALOG_PAGE_SIZE:0;if(catalog_page>max)catalog_page=max;if(catalog_page<0)catalog_page=0;return n;}
int content_setup_catalog_page_move(int d){int n,max,page;pthread_mutex_lock(&catalog_lock);n=catalog_matches();max=n?(n-1)/CONTENT_CATALOG_PAGE_SIZE:0;
 if(d>0&&catalog_page<max)catalog_page++;else if(d<0&&catalog_page>0)catalog_page--;page=catalog_page;pthread_mutex_unlock(&catalog_lock);return page;}
int content_setup_catalog_result(int row,char *name,size_t ns,char *type,size_t ts){
 int i,k,match=0,target;if(row<0||row>=CONTENT_CATALOG_PAGE_SIZE||!name||!ns)return 0;pthread_mutex_lock(&catalog_lock);catalog_matches();target=catalog_page*CONTENT_CATALOG_PAGE_SIZE+row;
 for(k=0;k<catalog_count;k++){i=catalog_sort_desc?catalog_count-1-k:k;if(catalog_match(i)&&match++==target){snprintf(name,ns,"%s",catalog[i].name);if(type&&ts)snprintf(type,ts,"%s",catalog[i].type);pthread_mutex_unlock(&catalog_lock);return 1;}}
 name[0]=0;if(type&&ts)type[0]=0;pthread_mutex_unlock(&catalog_lock);return 0;
}
int content_setup_catalog_download_row(int row){char n[64],t[16];if(!content_setup_catalog_result(row,n,sizeof(n),t,sizeof(t)))return 0;return content_setup_download_map(n);}
const char *content_setup_catalog_status(void){static _Thread_local char out[192];int n;pthread_mutex_lock(&catalog_lock);n=catalog_matches();
 snprintf(out,sizeof(out),"%.130s | %d matches | page %d/%d",catalog_message,n,catalog_page+1,n?(n-1)/CONTENT_CATALOG_PAGE_SIZE+1:1);pthread_mutex_unlock(&catalog_lock);return out;}



#define CONTENT_PACK_NAME_SIZE 256
static char (*content_pack_names)[CONTENT_PACK_NAME_SIZE];
static int content_pack_count,content_pack_capacity,content_pack_collect_failed,content_pack_page;
static char content_pack_query[48],content_pack_removal_name[CONTENT_PACK_NAME_SIZE];
static struct posix_directory_identity content_pack_removal_identity;
static Uint64 content_pack_next_refresh;
static char content_pack_message[128]="No texture packs found";
static pthread_mutex_t content_pack_lock=PTHREAD_MUTEX_INITIALIZER;
static SDL_Thread *content_pack_thread;
static int content_pack_running,content_pack_finished,content_pack_import_ok;
static char content_pack_import_path[1024],content_pack_import_name[64];
static void content_pack_collect(const char *name,void *context) {
 (void)context;
 if(content_pack_collect_failed)return;
 if(content_pack_count==content_pack_capacity){int cap;void *p;
  if(content_pack_capacity>INT_MAX/2){content_pack_collect_failed=1;return;}cap=content_pack_capacity?content_pack_capacity*2:32;
  if(cap<content_pack_capacity||(size_t)cap>SIZE_MAX/sizeof(content_pack_names[0])){content_pack_collect_failed=1;return;}
  p=realloc(content_pack_names,(size_t)cap*sizeof(content_pack_names[0]));if(!p){content_pack_collect_failed=1;return;}content_pack_names=p;content_pack_capacity=cap;
 }
 snprintf(content_pack_names[content_pack_count++],sizeof(content_pack_names[0]),"%s",name);
}
static int content_pack_compare(const void *a,const void *b){int n=strcasecmp(a,b);return n?n:strcmp(a,b);}
static int content_pack_match(int i){return !content_pack_query[0]||catalog_has(content_pack_names[i],content_pack_query);}
static int content_pack_matches(void){int n=0,i,max;for(i=0;i<content_pack_count;i++)if(content_pack_match(i))n++;
 max=n?(n-1)/CONTENT_TEXTURE_PACK_PAGE_SIZE:0;if(content_pack_page>max)content_pack_page=max;if(content_pack_page<0)content_pack_page=0;return n;}
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
  content_pack_removal_name[0]=0;
  if(ok&&texture_pack_select(name)){texture_pack_set_enabled(1);snprintf(content_pack_message,sizeof(content_pack_message),"Imported, selected and enabled: %.80s",name);}
  else if(ok)snprintf(content_pack_message,sizeof(content_pack_message),"Imported %.70s; choose it below to activate",name);
  else snprintf(content_pack_message,sizeof(content_pack_message),"Import failed; use relative tag keys and PNG/TGA/DDS replacements");
  content_pack_next_refresh=0;
 }
}
void content_setup_texture_pack_refresh(void) {
 Uint64 now=SDL_GetTicks();content_pack_apply_pending();if(now<content_pack_next_refresh)return;content_pack_next_refresh=now+1000;
 content_pack_count=0;content_pack_collect_failed=0;texture_pack_list(content_pack_collect,NULL);
 if(content_pack_count>1)qsort(content_pack_names,(size_t)content_pack_count,sizeof(content_pack_names[0]),content_pack_compare);
 if(content_pack_collect_failed)snprintf(content_pack_message,sizeof(content_pack_message),"Texture list incomplete: not enough memory; retry later");
 else if(!strcmp(content_pack_message,"No texture packs found")||!strcmp(content_pack_message,"No installed texture packs")||!strcmp(content_pack_message,"Choose an installed texture pack"))
  snprintf(content_pack_message,sizeof(content_pack_message),"%s",content_pack_count?"Choose an installed texture pack":"No installed texture packs");
 content_pack_matches();
}
int content_setup_texture_pack_count(void){content_setup_texture_pack_refresh();return content_pack_count;}
void content_setup_texture_pack_set_query(const char *query){snprintf(content_pack_query,sizeof(content_pack_query),"%.47s",query?query:"");content_pack_page=0;}
const char *content_setup_texture_pack_query(void){return content_pack_query;}
int content_setup_texture_pack_page_move(int d){int n,max;content_setup_texture_pack_refresh();n=content_pack_matches();max=n?(n-1)/CONTENT_TEXTURE_PACK_PAGE_SIZE:0;
 if(d>0&&content_pack_page<max)content_pack_page++;else if(d<0&&content_pack_page>0)content_pack_page--;return content_pack_page;}
int content_setup_texture_pack_name(int row,char *name,size_t size){int i,match=0,target;content_setup_texture_pack_refresh();content_pack_matches();
 if(row<0||row>=CONTENT_TEXTURE_PACK_PAGE_SIZE||!name||!size)return 0;
 target=content_pack_page*CONTENT_TEXTURE_PACK_PAGE_SIZE+row;
 for(i=0;i<content_pack_count;i++)if(content_pack_match(i)&&match++==target){snprintf(name,size,"%s",content_pack_names[i]);return 1;}
 name[0]=0;return 0;}
int content_setup_texture_pack_select_row(int row){char name[CONTENT_PACK_NAME_SIZE];if(!content_setup_texture_pack_name(row,name,sizeof(name)))return 0;
 if(!texture_pack_select(name)){snprintf(content_pack_message,sizeof(content_pack_message),"Cannot select this pack; managed pack names must be under 64 characters");return 0;}
 content_pack_removal_name[0]=0;snprintf(content_pack_message,sizeof(content_pack_message),"Selected: %.100s",name);return 1;}
int content_setup_texture_pack_toggle(void){return texture_pack_set_enabled(!texture_pack_enabled());}
/* Only managed optional pack directories can be disabled or staged for recovery. */
static int content_pack_same_identity(const struct posix_directory_identity *a,const struct posix_directory_identity *b){
 return a->device_low==b->device_low&&a->device_high==b->device_high&&a->inode_low==b->inode_low&&a->inode_high==b->inode_high;
}
static int content_pack_managed_open(const char *name,int *basefd,struct posix_directory_identity *st){
 char folder[1024],base[1200];int fd=-1,pack=-1,i,found=0;size_t n;
 if(!name||!(n=strlen(name))||n>=CONTENT_PACK_NAME_SIZE||!strcmp(name,".")||!strcmp(name,".."))return 0;
 for(i=0;name[i];i++)if(!((name[i]>='a'&&name[i]<='z')||(name[i]>='A'&&name[i]<='Z')||(name[i]>='0'&&name[i]<='9')||name[i]=='_'||name[i]=='-'||name[i]=='.'||name[i]==' '))return 0;
 content_setup_texture_pack_refresh();for(i=0;i<content_pack_count;i++)if(!strcmp(name,content_pack_names[i])){found=1;break;}if(!found)return 0;
 config_folder(folder,sizeof(folder));if(snprintf(base,sizeof(base),"%stexture-packs",folder)>=(int)sizeof(base))return 0;
 fd=open(base,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);if(fd<0)return 0;
 pack=openat(fd,name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);if(pack<0||posix_directory_identity_fd(pack,st)){if(pack>=0)close(pack);close(fd);return 0;}close(pack);*basefd=fd;return 1;
}
static int content_pack_selected_managed(char *name,size_t size,int *basefd,struct posix_directory_identity *st){
 char folder[1024],expected[1400];const char *selected=texture_pack_selected(),*leaf=content_basename(selected);
 if(!selected[0]||!leaf[0]||strlen(leaf)>=size)return 0;
 snprintf(name,size,"%s",leaf);config_folder(folder,sizeof(folder));
 if(snprintf(expected,sizeof(expected),"%stexture-packs/%s",folder,name)>=(int)sizeof(expected)||strcmp(expected,selected))return 0;
 return content_pack_managed_open(name,basefd,st);
}
int content_setup_texture_pack_disable_selected(void){char name[CONTENT_PACK_NAME_SIZE];struct posix_directory_identity st;int fd;
 if(!content_pack_selected_managed(name,sizeof(name),&fd,&st)){snprintf(content_pack_message,sizeof(content_pack_message),"Choose an installed managed texture pack first");return 0;}close(fd);
 if(!texture_pack_set_enabled(0))return 0;
 snprintf(content_pack_message,sizeof(content_pack_message),"Disabled: %.90s",name);return 1;
}
int content_setup_texture_pack_stage_selected_removal(void){char name[CONTENT_PACK_NAME_SIZE];struct posix_directory_identity st;int fd;
 content_pack_removal_name[0]=0;
 if(content_pack_running){snprintf(content_pack_message,sizeof(content_pack_message),"Wait for texture import to finish before removing a pack");return 0;}
 if(!content_pack_selected_managed(name,sizeof(name),&fd,&st)){snprintf(content_pack_message,sizeof(content_pack_message),"Choose an installed managed texture pack first");return 0;}close(fd);
 if(texture_pack_enabled()){snprintf(content_pack_message,sizeof(content_pack_message),"Disable the selected pack before removing it");return 0;}
 snprintf(content_pack_removal_name,sizeof(content_pack_removal_name),"%s",name);content_pack_removal_identity=st;
 snprintf(content_pack_message,sizeof(content_pack_message),"Confirm removal of %.65s; files will move to recovery",name);return 1;
}
int content_setup_texture_pack_removal_pending(void){return content_pack_removal_name[0]!=0;}
void content_setup_texture_pack_cancel_removal(void){content_pack_removal_name[0]=0;snprintf(content_pack_message,sizeof(content_pack_message),"Pack removal cancelled");}
int content_setup_texture_pack_confirm_removal(void){
 char folder[1024],selected_path[1400],recovery[64];struct posix_directory_identity st,active;int base=-1,parent=-1,trash=-1,slot=-1,ok=0,attempt,clear_selected;static unsigned long serial;
 if(!content_pack_removal_name[0])return 0;
 config_folder(folder,sizeof(folder));
 if(content_pack_running||!content_pack_managed_open(content_pack_removal_name,&base,&st)||!content_pack_same_identity(&st,&content_pack_removal_identity))goto failed;
 if(snprintf(selected_path,sizeof(selected_path),"%stexture-packs/%s",folder,content_pack_removal_name)>=(int)sizeof(selected_path))goto failed;
 clear_selected=!strcmp(texture_pack_selected(),selected_path);
 if(texture_pack_enabled()&&(clear_selected||(!posix_directory_identity_path(texture_pack_selected(),&active)&&content_pack_same_identity(&active,&st)))){snprintf(content_pack_message,sizeof(content_pack_message),"Disable the pack before confirming removal");goto done;}
 /* Open both stores by directory descriptors; never traverse a pack symlink. */
 {size_t len=strlen(folder);while(len>1&&folder[len-1]=='/')folder[--len]=0;}
 parent=open(folder,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);if(parent<0)goto failed;
 if(posix_make_private_directory_at(parent,"texture-packs-removed")&&errno!=EEXIST)goto failed;
 trash=openat(parent,"texture-packs-removed",O_RDONLY|O_DIRECTORY|O_NOFOLLOW);if(trash<0)goto failed;
 for(attempt=0;attempt<16;attempt++){snprintf(recovery,sizeof(recovery),"%llu-%lu",(unsigned long long)SDL_GetTicks(),++serial);if(!posix_make_private_directory_at(trash,recovery))break;if(errno!=EEXIST)goto failed;}
 if(attempt==16)goto failed;
 slot=openat(trash,recovery,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);if(slot<0)goto failed;
 /* Recheck directory identity immediately before the move. */
 if(posix_directory_identity_at(base,content_pack_removal_name,&st)||!content_pack_same_identity(&st,&content_pack_removal_identity))goto failed;
 if(renameat(base,content_pack_removal_name,slot,"pack"))goto failed;
 if(clear_selected&&!config_write("display.texture_pack_path","")){
  if(!renameat(slot,"pack",base,content_pack_removal_name))goto failed;
  snprintf(content_pack_message,sizeof(content_pack_message),"Pack moved to texture-packs-removed/%.48s/pack; could not clear selection",recovery);
  content_pack_removal_name[0]=0;content_pack_next_refresh=0;ok=1;goto done;
 }
 snprintf(content_pack_message,sizeof(content_pack_message),"Removed %.20s; recover from texture-packs-removed/%.48s/pack",content_pack_removal_name,recovery);
 content_pack_removal_name[0]=0;content_pack_next_refresh=0;ok=1;goto done;
failed:snprintf(content_pack_message,sizeof(content_pack_message),"Removal stopped: pack identity or managed recovery directory changed");
done:if(slot>=0)close(slot);if(trash>=0)close(trash);if(parent>=0)close(parent);if(base>=0)close(base);return ok;
}
const char *content_setup_texture_pack_status(void) {
 static _Thread_local char line[384];char message[128];int n;content_setup_texture_pack_refresh();n=content_pack_matches();
 pthread_mutex_lock(&content_pack_lock);if(content_pack_running)snprintf(content_pack_message,sizeof(content_pack_message),"Importing texture pack in background");
 snprintf(message,sizeof(message),"%s",content_pack_message);pthread_mutex_unlock(&content_pack_lock);
 snprintf(line,sizeof(line),"%d packs | %d matches | page %d/%d | Selected: %.63s (%s)\r\n%s",content_pack_count,n,content_pack_page+1,n?(n-1)/CONTENT_TEXTURE_PACK_PAGE_SIZE+1:1,texture_pack_selected()[0]?content_basename(texture_pack_selected()):"none",texture_pack_enabled()?"ON":"OFF",message);return line;
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

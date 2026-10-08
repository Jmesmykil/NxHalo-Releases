/* Curated native image packs, not a general executable mod loader.
   Strict bounded TOML and ZIP; all filesystem structure crosses posix.h. */
#include "native_mod_catalog.h"
#if !defined(__linux__) || defined(HALO_ANDROID)
/* Shared platform-source builds must not import Linux-only HTTPS/at APIs. */
#include <string.h>
void native_mod_catalog_poll(void){}
void native_mod_catalog_refresh(void){}
void native_mod_catalog_set_query(const char *query){(void)query;}
const char *native_mod_catalog_query(void){return "";}
int native_mod_catalog_page_move(int direction){(void)direction;return 0;}
int native_mod_catalog_result(int row,struct native_mod_catalog_entry *entry){(void)row;if(entry)memset(entry,0,sizeof(*entry));return 0;}
int native_mod_catalog_install_entry(const struct native_mod_catalog_entry *entry){(void)entry;return 0;}
int native_mod_catalog_install_row(int row){(void)row;return 0;}
int native_mod_catalog_install_running(void){return 0;}
const char *native_mod_catalog_status(void){return "Online native texture installation is unavailable on this platform";}
#else
#include "posix.h"
#include "texture_pack.h"
#include "port_config.h"
#include "update.h"
#include "../../third_party/tomlc17/tomlc17.h"
#include "../../third_party/zlib/zlib_prefixed.h"
#include "SDL3/SDL.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <time.h>
/* One-shot bounded SHA implementation already used by internet play. */
void p2p_sha256(const void *, int, unsigned char *);
#define DEFAULT_URL "https://raw.githubusercontent.com/Jmesmykil/NxHalo-Releases/main/catalog/native-mods.toml"
#define FILE_LIMIT (32U<<20)
#define CREDIT_LIMIT (64U<<10)
#define PATH_LIMIT 1024
#define REFRESH_MS (6ULL*60*60*1000)
static int failure(char *e,size_t cap,const char *message) { if(e&&cap)snprintf(e,cap,"%s",message);return 0; }
static int https(const char *s) {
 const char *p;if(!s||strncmp(s,"https://",8)||!s[8]||s[8]=='/')return 0;
 for(p=s+8;*p;p++)if((unsigned char)*p<=32||(unsigned char)*p==127||*p=='\\')return 0;
 for(p=s+8;*p&&*p!='/'&&*p!='?'&&*p!='#';p++)if(*p=='@')return 0;
 return p>s+8;
}
static int component(const char *s) {
 size_t i,n=strlen(s);if(!n||s[0]=='.'||!strcmp(s,".")||!strcmp(s,".."))return 0;
 for(i=0;i<n;i++)if(!isalnum((unsigned char)s[i])&&s[i]!='-'&&s[i]!='_'&&s[i]!='.')return 0;
 return 1;
}
static int string_field(toml_datum_t t,const char *key,char *out,size_t cap) {
 toml_datum_t d=toml_get(t,key);int i;
 if(d.type!=TOML_STRING||d.u.str.len<1||(size_t)d.u.str.len>=cap)return 0;
 for(i=0;i<d.u.str.len;i++)if((unsigned char)d.u.str.ptr[i]<32||(unsigned char)d.u.str.ptr[i]==127)return 0;
 memcpy(out,d.u.str.ptr,d.u.str.len);out[d.u.str.len]=0;return 1;
}
static int number_field(toml_datum_t t,const char *key,unsigned int *out,unsigned int limit) {
 toml_datum_t d=toml_get(t,key);if(d.type!=TOML_INT64||d.u.int64<1||d.u.int64>limit)return 0;
 *out=(unsigned int)d.u.int64;return 1;
}
static int parse(const char *text,size_t size,struct native_mod_catalog_entry *out,int *count,char *error,size_t cap) {
 toml_result_t r;toml_datum_t schema,mods;int i,j,ok=0;*count=0;
 if(!text||size>NATIVE_MOD_CATALOG_MAX_TEXT||memchr(text,0,size))return failure(error,cap,"Catalogue size or embedded NUL invalid");
 r=toml_parse(text,(int)size);if(!r.ok){toml_free(r);return failure(error,cap,"Malformed TOML catalogue");}
 schema=toml_get(r.toptab,"schema_version");mods=toml_get(r.toptab,"mods");
 if(schema.type!=TOML_INT64||schema.u.int64!=1||mods.type!=TOML_ARRAY||mods.u.arr.size>NATIVE_MOD_CATALOG_MAX_ENTRIES)goto end;
 for(i=0;i<mods.u.arr.size;i++){
  toml_datum_t t=mods.u.arr.elem[i];struct native_mod_catalog_entry *v=&out[i];memset(v,0,sizeof(*v));
  if(t.type!=TOML_TABLE||!string_field(t,"id",v->id,sizeof(v->id))||!component(v->id)||
   !string_field(t,"version",v->version,sizeof(v->version))||!component(v->version)||
   snprintf(v->pack_name,sizeof(v->pack_name),"%s-%s",v->id,v->version)>=(int)sizeof(v->pack_name)||
   !string_field(t,"title",v->title,sizeof(v->title))||!string_field(t,"author",v->author,sizeof(v->author))||
   !string_field(t,"license",v->license,sizeof(v->license))||!string_field(t,"source",v->source,sizeof(v->source))||!https(v->source)||
   !string_field(t,"archive",v->archive,sizeof(v->archive))||!https(v->archive)||
   !string_field(t,"sha256",v->sha256,sizeof(v->sha256))||strlen(v->sha256)!=64||
   !string_field(t,"native_format",v->native_format,sizeof(v->native_format))||strcmp(v->native_format,"tag-ordinal-images-v1")||
   !string_field(t,"map_scope",v->map_scope,sizeof(v->map_scope))||
   !number_field(t,"compressed_limit",&v->compressed_limit,NATIVE_MOD_CATALOG_MAX_ARCHIVE)||
   !number_field(t,"unpacked_limit",&v->unpacked_limit,NATIVE_MOD_CATALOG_MAX_UNPACKED)||
   !number_field(t,"file_count",&v->file_count,NATIVE_MOD_CATALOG_MAX_FILES))goto end;
  for(j=0;j<64;j++){if(!isxdigit((unsigned char)v->sha256[j]))goto end;v->sha256[j]=(char)tolower((unsigned char)v->sha256[j]);}
  for(j=0;j<i;j++)if(!strcmp(out[j].pack_name,v->pack_name))goto end;
 }
 *count=mods.u.arr.size;ok=1;
end:toml_free(r);if(!ok)failure(error,cap,"Invalid or unsupported catalogue entry (native images only)");return ok;
}
static unsigned int word(const unsigned char *p){return p[0]|(unsigned int)p[1]<<8;}
static unsigned int dword(const unsigned char *p){return word(p)|word(p+2)<<16;}
static int extra_valid(const unsigned char *p,unsigned int n) {
 unsigned int k=0;while(k<n){unsigned int len;if(n-k<4)return 0;len=word(p+k+2);if(word(p+k)==1||len>n-k-4)return 0;k+=4+len;}return k==n;
}
static int native_path(const char *p) {
 const char *part=p,*q,*ext,*ordinal;int depth=0;size_t len=strlen(p);
 if(!len||len>=PATH_LIMIT||p[0]=='/'||p[len-1]=='/')return 0;
 for(q=p;;q++){unsigned char c=(unsigned char)*q;
  if(c=='/'||!c){size_t n=q-part;if(!n||n>128||(n==1&&part[0]=='.')||(n==2&&part[0]=='.'&&part[1]=='.')||++depth>12)return 0;part=q+1;if(!c)break;}
  else if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'||c==' '))return 0;
 }
 ext=strrchr(p,'.');if(!ext||(strcmp(ext,".png")&&strcmp(ext,".tga")&&strcmp(ext,".dds")))return 0;
 ordinal=ext;while(ordinal>p&&ordinal[-1]>='0'&&ordinal[-1]<='9')ordinal--;
 if(ordinal==ext||ext-ordinal>5||ordinal-p<3||ordinal[-1]!='_'||ordinal[-2]!='_')return 0;
 return 1;
}
static int image_header(const char *name,const unsigned char *p,unsigned int n) {
 const char *ext=strrchr(name,'.');unsigned int w,h;
 if(!strcasecmp(ext,".png")){
  if(n<33||memcmp(p,"\211PNG\r\n\032\n",8)||memcmp(p+12,"IHDR",4)||dword(p+8)!=0x0d000000)return 0;
  w=(unsigned)p[16]<<24|(unsigned)p[17]<<16|(unsigned)p[18]<<8|p[19];h=(unsigned)p[20]<<24|(unsigned)p[21]<<16|(unsigned)p[22]<<8|p[23];
  return w&&h&&w<=4096&&h<=4096&&p[28]==0;
 }
 if(!strcasecmp(ext,".tga")){
  if(n<18||p[1]||p[2]!=2||(p[16]!=24&&p[16]!=32))return 0;
  w=word(p+12);h=word(p+14);return w&&h&&w<=4096&&h<=4096&&18U+p[0]+w*h*(p[16]/8)==n;
 }
 if(n<128||memcmp(p,"DDS ",4)||dword(p+4)!=124||dword(p+76)!=32)return 0;
 w=dword(p+16);h=dword(p+12);return w&&h&&w<=4096&&h<=4096&&!(dword(p+112)&0x0020FE00)&&
  (!memcmp(p+84,"DXT1",4)||!memcmp(p+84,"DXT3",4)||!memcmp(p+84,"DXT5",4));
}
struct zip_item {char name[PATH_LIMIT];unsigned int local,start,compressed,size,crc,method,end;int credit;};
static int path_write(int root,const char *name,const unsigned char *data,unsigned int n) {
 char path[PATH_LIMIT],*p,*part;int fd=dup(root),next=-1,out=-1,ok=0;unsigned int used=0;
 if(fd<0||strlen(name)>=sizeof(path))goto end;strcpy(path,name);part=path;
 for(p=path;*p;p++)if(*p=='/'){
  *p=0;if(posix_make_private_directory_at(fd,part)&&errno!=EEXIST)goto end;
  next=openat(fd,part,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(next<0)goto end;close(fd);fd=next;part=p+1;
 }
 out=openat(fd,part,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);if(out<0)goto end;
 while(used<n){ssize_t z=write(out,data+used,n-used);if(z<0&&errno==EINTR)continue;if(z<=0)goto end;used+=(unsigned int)z;}
 if(fsync(out))goto end;ok=1;
end:if(out>=0)close(out);if(fd>=0)close(fd);return ok;
}
/* Caller supplies an empty, private stage. All ZIP declarations validate
   before any payload file is created. No links, executables or directory entries. */
static int extract(const unsigned char *zip,size_t size,const struct native_mod_catalog_entry *entry,const char *stage,char *error,size_t cap) {
 struct zip_item *items=NULL;unsigned int eocd,cd,cdsize,n,k,cursor,payload=0,credits=0,total=0,covered=0;int root=-1,ok=0;unsigned char digest[32];char hex[65];
 if(size<22||size>entry->compressed_limit||size>NATIVE_MOD_CATALOG_MAX_ARCHIVE)return failure(error,cap,"Archive exceeds declared bounds");
 p2p_sha256(zip,(int)size,digest);for(k=0;k<32;k++)snprintf(hex+2*k,3,"%02x",digest[k]);
 if(strcmp(hex,entry->sha256))return failure(error,cap,"Archive SHA-256 does not match catalogue");
 eocd=(unsigned int)size-22;if(dword(zip+eocd)!=0x06054b50||word(zip+eocd+20))return failure(error,cap,"ZIP comment/ZIP64 or missing end record");
 n=word(zip+eocd+10);cdsize=dword(zip+eocd+12);cd=dword(zip+eocd+16);
 if(word(zip+eocd+4)||word(zip+eocd+6)||word(zip+eocd+8)!=n||!n||n>entry->file_count+3||n>NATIVE_MOD_CATALOG_MAX_FILES+3||cd>eocd||cdsize!=eocd-cd)goto invalid;
 items=calloc(n,sizeof(*items));if(!items)goto invalid;cursor=cd;
 for(k=0;k<n;k++){
  struct zip_item *v=&items[k];unsigned int names,extras,comments,flags,local,mode,j;
  if(cursor>eocd||eocd-cursor<46||dword(zip+cursor)!=0x02014b50)goto invalid;
  flags=word(zip+cursor+8);v->method=word(zip+cursor+10);v->crc=dword(zip+cursor+16);v->compressed=dword(zip+cursor+20);v->size=dword(zip+cursor+24);
  names=word(zip+cursor+28);extras=word(zip+cursor+30);comments=word(zip+cursor+32);local=dword(zip+cursor+42);mode=dword(zip+cursor+38)>>16;
  if((flags&~0x0800)||word(zip+cursor+34)||word(zip+cursor+6)>20||(v->method!=0&&v->method!=8)||
   !names||names>=sizeof(v->name)||names+extras+comments>eocd-cursor-46||
   v->compressed==0xffffffff||v->size==0xffffffff||local==0xffffffff||
   ((mode&0170000)&&(mode&0170000)!=0100000)||(dword(zip+cursor+38)&0x10)||
   !extra_valid(zip+cursor+46+names,extras))goto invalid;
  memcpy(v->name,zip+cursor+46,names);v->name[names]=0;if(memchr(v->name,0,names))goto invalid;
  if(!strcmp(v->name,"CREDITS.txt"))v->credit=1;
  else if(!strcmp(v->name,"LICENSE-NOTICE.txt"))v->credit=2;
  else if(!strcmp(v->name,"UPSTREAM-README.md"))v->credit=4;
  else if(!strncmp(v->name,"payload/",8)&&native_path(v->name+8)){payload++;if(v->size>FILE_LIMIT)goto invalid;}
  else goto invalid;
  if(v->credit){if(v->size>CREDIT_LIMIT||!v->size)goto invalid;credits|=v->credit;}
  if(!v->size||v->size>entry->unpacked_limit-total)goto invalid;total+=v->size;
  for(j=0;j<k;j++)if(!strcasecmp(items[j].name,v->name))goto invalid;
  if(local>cd||cd-local<30||dword(zip+local)!=0x04034b50||word(zip+local+4)>20||word(zip+local+6)!=flags||
   word(zip+local+8)!=v->method||dword(zip+local+14)!=v->crc||dword(zip+local+18)!=v->compressed||dword(zip+local+22)!=v->size||
   word(zip+local+26)!=names||30+names+word(zip+local+28)>cd-local||
   memcmp(zip+local+30,v->name,names)||!extra_valid(zip+local+30+names,word(zip+local+28)))goto invalid;
  v->local=local;v->start=local+30+names+word(zip+local+28);if(v->compressed>cd-v->start)goto invalid;v->end=v->start+v->compressed;
  /* Curated archives have exactly the indexed local entries, no hidden body. */
  cursor+=46+names+extras+comments;
 }
 if(cursor!=eocd||payload!=entry->file_count||(credits&3)!=3)goto invalid;
 /* Validate sorted local spans without relying on central entry order. */
 for(k=0;k<n;k++){
  unsigned int j,chosen=n,best=0xffffffff;
  for(j=0;j<n;j++)if(items[j].local>=covered&&items[j].local<best){best=items[j].local;chosen=j;}
  if(chosen==n||best!=covered)goto invalid;covered=items[chosen].end;
 }if(covered!=cd)goto invalid;
 root=open(stage,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(root<0)goto invalid;
 for(k=0;k<n;k++){
  struct zip_item *v=&items[k];unsigned char *data=malloc(v->size);int valid=0;
  if(!data)goto invalid;
  if(v->method==0){if(v->size==v->compressed){memcpy(data,zip+v->start,v->size);valid=1;}}
  else {z_stream z;int rc;memset(&z,0,sizeof(z));z.next_in=(Bytef*)zip+v->start;z.avail_in=v->compressed;z.next_out=data;z.avail_out=v->size;
   if(inflateInit2(&z,-MAX_WBITS)==Z_OK){rc=inflate(&z,Z_FINISH);valid=rc==Z_STREAM_END&&z.total_in==v->compressed&&z.total_out==v->size;inflateEnd(&z);}}
  if(valid)valid=(unsigned int)crc32(0,data,v->size)==v->crc;
  if(valid&&!v->credit)valid=image_header(v->name,data,v->size);
  if(valid&&v->credit){unsigned int j;for(j=0;j<v->size;j++)if(!data[j]||(data[j]<32&&data[j]!='\n'&&data[j]!='\r'&&data[j]!='\t')){valid=0;break;}}
  if(valid)valid=path_write(root,v->name,data,v->size);free(data);if(!valid)goto invalid;
 }
 ok=1;goto end;
invalid:failure(error,cap,"Unsafe, corrupt or unsupported native texture ZIP");
end:if(root>=0)close(root);free(items);return ok;
}
/* Recurses only through directories opened without following links. */
static void cleanup_fd(int fd) {
 void *d=posix_directory_open_fd(fd);char name[256];if(!d)return;
 while(posix_directory_next(d,name,sizeof(name))){
  int sub=openat(fd,name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
  if(sub>=0){cleanup_fd(sub);close(sub);unlinkat(fd,name,AT_REMOVEDIR);}else unlinkat(fd,name,0);
 }posix_directory_close(d);
}
static void cleanup(const char *path){int fd=open(path,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(fd>=0){cleanup_fd(fd);close(fd);rmdir(path);}}
static unsigned char *read_file(const char *path,unsigned int limit,size_t *size){
 struct posix_file_information st;unsigned char *p;size_t used=0;int fd=open(path,O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK);
 if(fd<0)return NULL;
 if(posix_fstat(fd,&st)||!posix_file_is_regular(fd)||st.size_high||st.size_low>limit){close(fd);return NULL;}
 *size=st.size_low;p=malloc(*size+1);if(!p){close(fd);return NULL;}
 while(used<*size){ssize_t n=read(fd,p+used,*size-used);if(n<0&&errno==EINTR)continue;if(n<=0){free(p);close(fd);return NULL;}used+=(size_t)n;}
 p[*size]=0;close(fd);return p;
}
#ifdef NATIVE_MOD_CATALOG_TEST
int native_mod_catalog_test_parse(const char *t,size_t n,struct native_mod_catalog_entry *v,int *count,char *e,size_t cap){return parse(t,n,v,count,e,cap);}
int native_mod_catalog_test_extract(const unsigned char *z,size_t n,const struct native_mod_catalog_entry *v,const char *s,char *e,size_t cap){return extract(z,n,v,s,e,cap);}
#endif

static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static struct native_mod_catalog_entry entries[NATIVE_MOD_CATALOG_MAX_ENTRIES],job;
static char query[48],message[192]="Native texture catalogue has not loaded";
static char cache_path[PATH_LIMIT],cache_root[PATH_LIMIT],catalog_url[512];
static int count,page,running,install_running,force_refresh;
static Uint64 next_fetch,installed_next;
static SDL_Thread *catalog_thread,*install_thread;
static int contains(const char *h,const char *needle){size_t n=strlen(needle);for(;*h;h++)if(!strncasecmp(h,needle,n))return 1;return !n;}
static int matches(int i){return contains(entries[i].title,query)||contains(entries[i].author,query)||contains(entries[i].id,query)||contains(entries[i].map_scope,query);}
static int matched(void){int i,n=0,max;for(i=0;i<count;i++)if(matches(i))n++;max=n?(n-1)/9:0;if(page>max)page=max;return n;}
static void publish(struct native_mod_catalog_entry *v,int n,const char *msg){
 pthread_mutex_lock(&lock);if(n>=0){memcpy(entries,v,n*sizeof(*v));count=n;installed_next=0;matched();}snprintf(message,sizeof(message),"%s",msg);pthread_mutex_unlock(&lock);
}
static int catalog_worker(void *unused){
 struct native_mod_catalog_entry *v=calloc(NATIVE_MOD_CATALOG_MAX_ENTRIES,sizeof(*v));unsigned char *text=NULL;size_t size;int n,had=0,refresh;char err[160]="",stage[PATH_LIMIT]="",temp[PATH_LIMIT];struct posix_file_information st;(void)unused;
 pthread_mutex_lock(&lock);refresh=force_refresh;force_refresh=0;pthread_mutex_unlock(&lock);
 if(!v){snprintf(err,sizeof(err),"Out of memory");goto fail;}
 text=read_file(cache_path,NATIVE_MOD_CATALOG_MAX_TEXT,&size);
 if(text&&parse((char*)text,size,v,&n,err,sizeof(err))){char m[160];had=1;snprintf(m,sizeof(m),"Cached native texture catalogue: %d authored packs",n);publish(v,n,m);
  if(!refresh&&!posix_stat(cache_path,&st)&&time(NULL)>=st.modification_seconds&&time(NULL)-st.modification_seconds<21600){goto done;}}
 free(text);text=NULL;
 if(snprintf(stage,sizeof(stage),"%s/fetch-XXXXXX",cache_root)>=(int)sizeof(stage)||!mkdtemp(stage)){stage[0]=0;snprintf(err,sizeof(err),"Could not create private catalogue stage");goto fail;}
 snprintf(temp,sizeof(temp),"%s/catalog.toml",stage);
 if(!update_fetch_text_limited(catalog_url,temp,NATIVE_MOD_CATALOG_MAX_TEXT,NULL,NULL,err,sizeof(err)))goto fail;
 text=read_file(temp,NATIVE_MOD_CATALOG_MAX_TEXT,&size);if(!text||!parse((char*)text,size,v,&n,err,sizeof(err)))goto fail;
 if(rename(temp,cache_path)){snprintf(err,sizeof(err),"Could not save validated catalogue");goto fail;}
 {char m[160];snprintf(m,sizeof(m),n?"Native textures ready: %d authored packs":"No native texture packs published; DLL/Lua mods require ports",n);publish(v,n,m);}goto done;
fail:{char m[192];snprintf(m,sizeof(m),"%s: %.135s",had?"Cached catalogue retained; refresh failed":"Native catalogue unavailable",err[0]?err:"Invalid cache/download");publish(v,-1,m);pthread_mutex_lock(&lock);next_fetch=SDL_GetTicks()+600000;pthread_mutex_unlock(&lock);}
done:free(text);free(v);if(stage[0])cleanup(stage);pthread_mutex_lock(&lock);running=0;pthread_mutex_unlock(&lock);return 0;
}
static void progress(void *ctx,unsigned long long received,unsigned long long total){
 (void)ctx;pthread_mutex_lock(&lock);snprintf(message,sizeof(message),"Downloading %.70s: %llu / %llu KiB",job.title,received/1024,total/1024);pthread_mutex_unlock(&lock);
}
/* Descriptor-relative reads reject links, empty files and oversized metadata. */
static unsigned char *read_at(int directory,const char *name,unsigned int limit,size_t *size,int *absent){
 struct posix_file_information st;unsigned char *p;size_t used=0;int fd;
 *size=0;if(absent)*absent=0;
 fd=openat(directory,name,O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK);
 if(fd<0){if(absent&&errno==ENOENT)*absent=1;return NULL;}
 if(posix_fstat(fd,&st)||!posix_file_is_regular(fd)||st.size_high||!st.size_low||st.size_low>limit){close(fd);return NULL;}
 *size=st.size_low;p=malloc(*size+1);if(!p){close(fd);return NULL;}
 while(used<*size){ssize_t n=read(fd,p+used,*size-used);if(n<0&&errno==EINTR)continue;if(n<=0){free(p);close(fd);return NULL;}used+=(size_t)n;}
 p[*size]=0;close(fd);return p;
}
static int receipt_text(const struct native_mod_catalog_entry *v,char *text,size_t cap){
 int n=snprintf(text,cap,"ID: %s\nVersion: %s\nPack: %s\nTitle: %s\nAuthor: %s\nLicense: %s\nSource: %s\nMap/tag scope: %s\nNative format: %s\nArchive: %s\nSHA256: %s\nCompressed limit: %u\nUnpacked limit: %u\nImage count: %u\n",
 v->id,v->version,v->pack_name,v->title,v->author,v->license,v->source,v->map_scope,v->native_format,v->archive,v->sha256,v->compressed_limit,v->unpacked_limit,v->file_count);
 return n>0&&(size_t)n<cap;
}
/* A reused receipt must still carry the complete attribution from the newly
   verified archive and exactly the current catalogue metadata. Never repair
   or overwrite a damaged receipt implicitly. */
static int receipt_existing(int existing,int stage,const struct native_mod_catalog_entry *v,const char *text){
 const char *names[]={"CREDITS.txt","LICENSE-NOTICE.txt","UPSTREAM-README.md"};
 unsigned char *old=NULL,*fresh=NULL;size_t old_n,fresh_n;int i,old_absent,fresh_absent,ok=0;
 old=read_at(existing,"archive.sha256",64,&old_n,NULL);if(!old||old_n!=64||memcmp(old,v->sha256,64))goto end;free(old);old=NULL;
 old=read_at(existing,"RECEIPT.txt",4096,&old_n,NULL);if(!old||old_n!=strlen(text)||memcmp(old,text,old_n))goto end;free(old);old=NULL;
 for(i=0;i<3;i++){
  fresh=read_at(stage,names[i],CREDIT_LIMIT,&fresh_n,&fresh_absent);
  old=read_at(existing,names[i],CREDIT_LIMIT,&old_n,&old_absent);
  if(i==2&&fresh_absent&&old_absent)continue;
  if(!fresh||!old||fresh_n!=old_n||memcmp(fresh,old,fresh_n))goto end;
  free(fresh);fresh=NULL;free(old);old=NULL;
 }
 ok=1;
end:free(fresh);free(old);return ok;
}
/* Retain attribution separately from the image-only managed pack directory. */
static int receipt(const struct native_mod_catalog_entry *v,const char *stage,char *error,size_t cap) {
 char folder[PATH_LIMIT],base[PATH_LIMIT],temp[PATH_LIMIT],text[4096];int fd=-1,parent=-1,stage_fd=-1,ok=0,i;
 const char *names[]={"CREDITS.txt","LICENSE-NOTICE.txt","UPSTREAM-README.md"};
 if(!receipt_text(v,text,sizeof(text)))goto end;
 stage_fd=open(stage,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(stage_fd<0)goto end;
 config_folder(folder,sizeof(folder));
 if(snprintf(base,sizeof(base),"%snative-mod-receipts",folder)>=(int)sizeof(base))goto end;
 if(posix_make_directory(base)&&errno!=EEXIST)goto end;
 parent=open(base,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(parent<0)goto end;
 {int existing=openat(parent,v->pack_name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
  if(existing>=0){ok=receipt_existing(existing,stage_fd,v,text);close(existing);goto end;}
  else if(errno!=ENOENT)goto end;
 }
 if(snprintf(temp,sizeof(temp),"%s/.receipt-XXXXXX",base)>=(int)sizeof(temp)||!mkdtemp(temp))goto end;
 fd=open(temp,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)goto temp_end;
 for(i=0;i<3;i++){size_t n;int absent;unsigned char *p=read_at(stage_fd,names[i],CREDIT_LIMIT,&n,&absent);
  if(!p){if(i==2&&absent)continue;goto temp_end;}{int copied=path_write(fd,names[i],p,(unsigned int)n);free(p);if(!copied)goto temp_end;}}
 if(!path_write(fd,"RECEIPT.txt",(unsigned char*)text,(unsigned int)strlen(text))||!path_write(fd,"archive.sha256",(unsigned char*)v->sha256,64))goto temp_end;
 close(fd);fd=-1;ok=posix_rename_noreplace_at(parent,strrchr(temp,'/')+1,v->pack_name)==0;
temp_end:if(fd>=0){close(fd);fd=-1;}cleanup(temp);
end:if(stage_fd>=0)close(stage_fd);if(parent>=0)close(parent);if(!ok)failure(error,cap,"Attribution receipt is damaged, changed or could not be preserved");return ok;
}
#ifdef NATIVE_MOD_CATALOG_TEST
int native_mod_catalog_test_receipt(const struct native_mod_catalog_entry *v,const char *stage,char *e,size_t cap){return receipt(v,stage,e,cap);}
#endif
static int install_worker(void *unused){
 struct native_mod_catalog_entry v;char folder[PATH_LIMIT],stage[PATH_LIMIT]="",archive[PATH_LIMIT],payload[PATH_LIMIT],err[160]="";unsigned char *zip=NULL;size_t size;int ok=0;(void)unused;
 pthread_mutex_lock(&lock);v=job;pthread_mutex_unlock(&lock);
 config_folder(folder,sizeof(folder));
 if(snprintf(stage,sizeof(stage),"%s.native-mod-XXXXXX",folder)>=(int)sizeof(stage)||!mkdtemp(stage)){stage[0]=0;snprintf(err,sizeof(err),"Could not prepare private download stage");goto end;}
 snprintf(archive,sizeof(archive),"%s/download.zip",stage);
 if(!update_download_limited(v.archive,archive,v.compressed_limit,progress,NULL,err,sizeof(err)))goto end;
 zip=read_file(archive,v.compressed_limit,&size);if(!zip||!extract(zip,size,&v,stage,err,sizeof(err)))goto end;
 free(zip);zip=NULL;
 if(!receipt(&v,stage,err,sizeof(err)))goto end;
 snprintf(payload,sizeof(payload),"%s/payload",stage);
 if(!texture_pack_install(v.pack_name,payload)){snprintf(err,sizeof(err),"Atomic import refused; pack may already exist or images are invalid");goto end;}ok=1;
end:free(zip);if(stage[0])cleanup(stage);pthread_mutex_lock(&lock);snprintf(message,sizeof(message),ok?"Installed %.90s; choose it in Installed Packs to enable":"Install failed: %.145s",ok?v.title:err);install_running=0;installed_next=0;pthread_mutex_unlock(&lock);return 0;
}
void native_mod_catalog_poll(void){
 SDL_Thread *a=NULL,*b=NULL;char *pref;unsigned char digest[32];char hash[17];int i;Uint64 now=SDL_GetTicks();
 pthread_mutex_lock(&lock);if(catalog_thread&&!running){a=catalog_thread;catalog_thread=NULL;}if(install_thread&&!install_running){b=install_thread;install_thread=NULL;}pthread_mutex_unlock(&lock);
 if(a)SDL_WaitThread(a,NULL);if(b)SDL_WaitThread(b,NULL);
 pthread_mutex_lock(&lock);if(running||install_running||now<next_fetch){pthread_mutex_unlock(&lock);return;}
 if(!catalog_url[0]){const char *url=getenv("HALO_NATIVE_MOD_CATALOG_URL");if(url&&(!https(url)||strlen(url)>=sizeof(catalog_url))){snprintf(message,sizeof(message),"Catalogue override must be HTTPS");next_fetch=now+REFRESH_MS;pthread_mutex_unlock(&lock);return;}snprintf(catalog_url,sizeof(catalog_url),"%s",url?url:DEFAULT_URL);}
 pref=SDL_GetPrefPath("OpenCE","NativeModCatalog");if(!pref||strlen(pref)+32>=sizeof(cache_root)){if(pref)SDL_free(pref);snprintf(message,sizeof(message),"Could not prepare native catalogue cache");next_fetch=now+600000;pthread_mutex_unlock(&lock);return;}
 snprintf(cache_root,sizeof(cache_root),"%s",pref);SDL_free(pref);
 p2p_sha256(catalog_url,(int)strlen(catalog_url),digest);for(i=0;i<8;i++)snprintf(hash+2*i,3,"%02x",digest[i]);snprintf(cache_path,sizeof(cache_path),"%s%s.toml",cache_root,hash);
 running=1;next_fetch=now+REFRESH_MS;catalog_thread=SDL_CreateThread(catalog_worker,"native texture catalogue",NULL);
 if(!catalog_thread){running=0;snprintf(message,sizeof(message),"Could not start catalogue worker");}pthread_mutex_unlock(&lock);
}
void native_mod_catalog_refresh(void){pthread_mutex_lock(&lock);force_refresh=1;next_fetch=0;pthread_mutex_unlock(&lock);native_mod_catalog_poll();}
void native_mod_catalog_set_query(const char *q){pthread_mutex_lock(&lock);snprintf(query,sizeof(query),"%.47s",q?q:"");page=0;pthread_mutex_unlock(&lock);}
const char *native_mod_catalog_query(void){static _Thread_local char out[48];pthread_mutex_lock(&lock);strcpy(out,query);pthread_mutex_unlock(&lock);return out;}
int native_mod_catalog_page_move(int direction){int n;pthread_mutex_lock(&lock);n=matched();if(direction>0&&page<(n-1)/9)page++;else if(direction<0&&page>0)page--;n=page;pthread_mutex_unlock(&lock);return n;}
static void installed_collect(const char *name,void *context){int i;(void)context;for(i=0;i<count;i++)if(!strcmp(name,entries[i].pack_name))entries[i].installed=1;}
static void refresh_installed(void){int i;Uint64 now=SDL_GetTicks();if(now<installed_next)return;for(i=0;i<count;i++)entries[i].installed=0;texture_pack_list(installed_collect,NULL);installed_next=now+500;}
static void installed_name(const char *name,void *context){struct native_mod_catalog_entry *v=context;if(!strcmp(name,v->pack_name))v->installed=1;}
int native_mod_catalog_result(int row,struct native_mod_catalog_entry *out){
 int i,target,seen=0,found=0;if(!out||row<0||row>=9)return 0;pthread_mutex_lock(&lock);refresh_installed();matched();target=page*9+row;
 for(i=0;i<count;i++)if(matches(i)&&seen++==target){*out=entries[i];found=1;break;}pthread_mutex_unlock(&lock);
 if(!found)memset(out,0,sizeof(*out));return found;
}
/* Consent is for all displayed and security metadata, not just archive bytes. */
static int entry_equal(const struct native_mod_catalog_entry *a,const struct native_mod_catalog_entry *b){
 return !strcmp(a->id,b->id)&&!strcmp(a->version,b->version)&&!strcmp(a->pack_name,b->pack_name)&&
 !strcmp(a->title,b->title)&&!strcmp(a->author,b->author)&&!strcmp(a->license,b->license)&&
 !strcmp(a->source,b->source)&&!strcmp(a->archive,b->archive)&&!strcmp(a->sha256,b->sha256)&&
 !strcmp(a->native_format,b->native_format)&&!strcmp(a->map_scope,b->map_scope)&&
 a->compressed_limit==b->compressed_limit&&a->unpacked_limit==b->unpacked_limit&&a->file_count==b->file_count;
}
#ifdef NATIVE_MOD_CATALOG_TEST
int native_mod_catalog_test_entry_equal(const struct native_mod_catalog_entry *a,const struct native_mod_catalog_entry *b){return entry_equal(a,b);}
#endif
int native_mod_catalog_install_entry(const struct native_mod_catalog_entry *snapshot){
 struct native_mod_catalog_entry v;int i,found=0,started;
 if(!snapshot)return 0;native_mod_catalog_poll();
 pthread_mutex_lock(&lock);
 for(i=0;i<count;i++)if(entry_equal(&entries[i],snapshot)){v=entries[i];found=1;break;}
 if(!found||running||install_running){snprintf(message,sizeof(message),found?"Another catalogue/install operation is running":"Pack changed or disappeared; reopen its details");pthread_mutex_unlock(&lock);return 0;}
 v.installed=0;texture_pack_list(installed_name,&v);if(v.installed){snprintf(message,sizeof(message),"This exact pack version is already installed");pthread_mutex_unlock(&lock);return 0;}
 job=v;install_running=1;snprintf(message,sizeof(message),"Preparing %.110s",v.title);install_thread=SDL_CreateThread(install_worker,"native texture install",NULL);
 started=install_thread!=NULL;if(!started){install_running=0;snprintf(message,sizeof(message),"Could not start installer");}pthread_mutex_unlock(&lock);return started;
}
int native_mod_catalog_install_row(int row){struct native_mod_catalog_entry v;if(!native_mod_catalog_result(row,&v))return 0;return native_mod_catalog_install_entry(&v);}
int native_mod_catalog_install_running(void){int n;pthread_mutex_lock(&lock);n=install_running;pthread_mutex_unlock(&lock);return n;}
const char *native_mod_catalog_status(void){static _Thread_local char out[256];int n;pthread_mutex_lock(&lock);n=matched();snprintf(out,sizeof(out),"%.190s | %d matches | page %d/%d",message,n,page+1,n?(n-1)/9+1:1);pthread_mutex_unlock(&lock);return out;}

#endif /* native Linux installer */

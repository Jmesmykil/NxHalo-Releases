#include "platform.h"
#include "update.h"
#include "community_directory.h"
#include "p2p_internal.h"
#include <errno.h>
#include <SDL3/SDL.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define URL "https://halo.milenko.org/v1/games"
#define LIMIT (1024 * 1024)
#define MAX_GAMES 512
#define POLL_MS 10000
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static struct community_directory_game games[MAX_GAMES];
static int game_count, done = 1;
static SDL_Thread *thread;
static Uint64 next_poll;
static char status[96] = "Community directory has not been checked";
static char path[256];
static struct community_directory_game classic_games[2][MAX_GAMES];
static int classic_count[2], classic_loaded[2];
static time_t classic_mtime[2];
static long classic_size[2];
static Uint64 classic_next_check;
struct cursor { const char *p, *end; int depth; };
static int hd(char c) { if(c>='0'&&c<='9')return c-'0'; if(c>='a'&&c<='f')return c-'a'+10; if(c>='A'&&c<='F')return c-'A'+10; return -1; }
static void ws(struct cursor *c) { while(c->p<c->end&&(*c->p==' '||*c->p=='\t'||*c->p=='\r'||*c->p=='\n'))c->p++; }
static int str(struct cursor *c,char *out,size_t cap) {
 size_t n=0; if(c->p>=c->end||*c->p++!='"')return 0;
 while(c->p<c->end&&*c->p!='"') { unsigned char ch=(unsigned char)*c->p++;
  if(ch=='\\') { if(c->p>=c->end)return 0; ch=(unsigned char)*c->p++; switch(ch) {
   case '"':case '\\':case '/':break; case 'b':case 'f':case 'n':case 'r':case 't':ch=' ';break;
   case 'u': if(c->end-c->p<4||hd(c->p[0])<0||hd(c->p[1])<0||hd(c->p[2])<0||hd(c->p[3])<0)return 0; c->p+=4;ch='?';break; default:return 0; } }
  if(ch<32||ch>126)ch='?'; if(cap&&n+1<cap)out[n++]=(char)ch;
 }
 if(c->p>=c->end)return 0; c->p++; if(cap)out[n]=0; return 1;
}
static int skip(struct cursor *c) {
 char scratch[2]; ws(c); if(c->p>=c->end||c->depth>=32)return 0;
 if(*c->p=='"')return str(c,scratch,sizeof(scratch));
 if(*c->p=='{'||*c->p=='[') { char close=*c->p++=='{'?'}':']'; c->depth++; ws(c); if(c->p<c->end&&*c->p==close){c->p++;c->depth--;return 1;}
  for(;;){if(close=='}'){if(!str(c,scratch,sizeof(scratch)))return 0;ws(c);if(c->p>=c->end||*c->p++!=':')return 0;} if(!skip(c))return 0;ws(c);if(c->p>=c->end)return 0;if(*c->p==close){c->p++;c->depth--;return 1;}if(*c->p++!=',')return 0;ws(c);}
 }
 while(c->p<c->end&&!strchr(",]} \t\r\n",*c->p))c->p++; return 1;
}
static int integer(struct cursor *c,int *v) { char *e; long n; ws(c);errno=0;n=strtol(c->p,&e,10);if(e==c->p||e>c->end||errno||n<0||n>100000)return 0;c->p=e;*v=(int)n;return 1; }
static int json_boolean(struct cursor *c,int *v) { ws(c);if(c->end-c->p>=4&&!memcmp(c->p,"true",4)){c->p+=4;*v=1;return 1;}if(c->end-c->p>=5&&!memcmp(c->p,"false",5)){c->p+=5;*v=0;return 1;}if(c->p<c->end&&(*c->p==48||*c->p==49)){*v=*c->p++-48;return 1;}return 0; }
static int invite_id(const char *invite,struct community_directory_game *g) {
 static const char pre[]="halo://join/"; const char *h=invite; unsigned char hash[P2P_KEY_HASH_SIZE]; int i;
 if(!strncmp(h,pre,sizeof(pre)-1))h+=sizeof(pre)-1; if(strlen(h)!=64)return 0; for(i=0;i<64;i++)if(hd(h[i])<0)return 0;
 for(i=0;i<P2P_KEY_HASH_SIZE;i++){int a=hd(h[2*i]),b=hd(h[2*i+1]);if(a<0||b<0)return 0;hash[i]=(unsigned char)(a*16+b);}
 p2p_identifier_from_hash(hash,g->listing.identifier);snprintf(g->listing.invite,sizeof(g->listing.invite),"halo://join/%s",h);return 1;
}
static int parse_game(struct cursor *c,struct community_directory_game *g) {
 char key[40],inv[P2P_LISTING_INVITE_SIZE]="",src[24]="";int hn=0,hm=0,hp=0,hx=0,hi=0,first=1,v;
 memset(g,0,sizeof(*g));g->listing.ping=-1;g->listing.engine_type=1;g->listing.open=1;
 ws(c);if(c->p>=c->end||*c->p++!='{')return 0;
 for(;;){ws(c);if(c->p<c->end&&*c->p=='}'){c->p++;break;}if(!first){if(c->p>=c->end||*c->p++!=',')return 0;ws(c);}first=0;
  if(!str(c,key,sizeof(key)))return 0;ws(c);if(c->p>=c->end||*c->p++!=':')return 0;ws(c);
  if(!strcmp(key,"invite")){if(c->p<c->end&&*c->p=='"'){if(!str(c,inv,sizeof(inv)))return 0;hi=1;}else if(!skip(c))return 0;}
  else if(!strcmp(key,"name")){if(!str(c,g->listing.name,sizeof(g->listing.name)))return 0;hn=1;}
  else if(!strcmp(key,"map")){if(!str(c,g->listing.map,sizeof(g->listing.map)))return 0;hm=1;}
  else if(!strcmp(key,"gametype")){if(!str(c,g->listing.gametype,sizeof(g->listing.gametype)))return 0;}
  else if(!strcmp(key,"mode")){if(!str(c,g->mode,sizeof(g->mode)))return 0;}
  else if(!strcmp(key,"source")){if(!str(c,src,sizeof(src)))return 0;}
  else if(!strcmp(key,"version")){if(!integer(c,&g->version))return 0;}
  else if(!strcmp(key,"engine")){if(!integer(c,&v))return 0;g->listing.engine_type=(unsigned char)v;}
  else if(!strcmp(key,"players")){if(!integer(c,&v))return 0;g->listing.player_count=(unsigned char)(v>255?255:v);hp=1;}
  else if(!strcmp(key,"maximum_players")){if(!integer(c,&v))return 0;g->listing.maximum_player_count=(unsigned char)(v>255?255:v);hx=1;}
  else if(!strcmp(key,"open")){if(!json_boolean(c,&v))return 0;g->listing.open=(unsigned char)v;}
  else if(!strcmp(key,"in_progress")){if(!json_boolean(c,&v))return 0;g->listing.in_progress=(unsigned char)v;}
  else if(!strcmp(key,"teams")){if(!json_boolean(c,&v))return 0;g->listing.has_teams=(unsigned char)v;}
  else if(!skip(c))return 0;
 }
 if(!hn||!hm||!hp||!hx)return 0;g->source=!strcmp(src,"broker")?1:(src[0]?2:0);
 /* Missing or malformed invite leaves the row visible and unjoinable. */ if(!hi||!invite_id(inv,g))g->listing.invite[0]=0;
 return 1;
}
static int parse_feed(const char *data,size_t size,struct community_directory_game *out) {
 struct cursor c={data,data+size,0};char key[32];int first=1,n=0;ws(&c);if(c.p>=c.end||*c.p++!='{')return -1;
 for(;;){ws(&c);if(c.p<c.end&&*c.p=='}'){c.p++;break;}if(!first){if(c.p>=c.end||*c.p++!=',')return -1;ws(&c);}first=0;
  if(!str(&c,key,sizeof(key)))return -1;ws(&c);if(c.p>=c.end||*c.p++!=':')return -1;ws(&c);
  if(!strcmp(key,"games")){int f=1;if(c.p>=c.end||*c.p++!='[')return -1;for(;;){ws(&c);if(c.p<c.end&&*c.p==']'){c.p++;break;}if(!f){if(c.p>=c.end||*c.p++!=',')return -1;ws(&c);}f=0;if(n<MAX_GAMES){if(!parse_game(&c,&out[n]))return -1;n++;}else if(!skip(&c))return -1;}}
  else if(!skip(&c))return -1;
 }
 ws(&c);return c.p==c.end?n:-1;
}
static int parse_classic_row(struct cursor *c,struct community_directory_game *g,int pc) {
 char key[32],address[80],game[32];int port=0,have_address=0,have_port=0,first=1,i;
 memset(g,0,sizeof(*g));address[0]=game[0]=0;ws(c);if(c->p>=c->end||*c->p++!='{')return 0;
 for(;;){ws(c);if(c->p<c->end&&*c->p=='}'){c->p++;break;}if(!first){if(c->p>=c->end||*c->p++!=',')return 0;ws(c);}first=0;
  if(!str(c,key,sizeof(key)))return 0;ws(c);if(c->p>=c->end||*c->p++!=':')return 0;ws(c);
  if(!strcmp(key,"address")){if(!str(c,address,sizeof(address)))return 0;have_address=1;}
  else if(!strcmp(key,"port")){if(!integer(c,&port))return 0;have_port=1;}
  else if(!strcmp(key,"game")){if(!str(c,game,sizeof(game)))return 0;}
  else if(!skip(c))return 0;
 }
 if(!have_address||!have_port||port<1||port>65535||!address[0]||strlen(address)>=26)return 0;
 for(i=0;address[i];i++)if(!((address[i]>='0'&&address[i]<='9')||(address[i]>='a'&&address[i]<='f')||(address[i]>='A'&&address[i]<='F')||address[i]=='.'||address[i]==':'))return 0;
 snprintf(g->listing.name,sizeof(g->listing.name),"%s:%d",address,port);
 snprintf(g->listing.map,sizeof(g->listing.map),"Unknown");
 g->listing.engine_type=1;g->listing.open=0;g->listing.ping=-1;
 g->version=pc?-3:-2;g->source=3;snprintf(g->mode,sizeof(g->mode),pc?"classic_pc":"classic_ce");
 return 1;
}
static int parse_classic_feed(const char *data,size_t size,struct community_directory_game *out,int pc) {
 struct cursor c={data,data+size,0};int first=1,n=0;ws(&c);if(c.p>=c.end||*c.p++!='[')return -1;
 for(;;){ws(&c);if(c.p<c.end&&*c.p==']'){c.p++;break;}if(!first){if(c.p>=c.end||*c.p++!=',')return -1;ws(&c);}first=0;if(n>=MAX_GAMES||!parse_classic_row(&c,&out[n],pc))return -1;n++;}
 ws(&c);return c.p==c.end?n:-1;
}
static void refresh_classic(void) {
 char executable[1024],directory[1024],file[2][1100];char *slash;int i;
 Uint64 now=SDL_GetTicks();if(now<classic_next_check)return;classic_next_check=now+2000;
 if(!update_executable_path(executable,sizeof(executable)))return;
 slash=strrchr(executable,'/');if(!slash)return;*slash=0;if(snprintf(directory,sizeof(directory),"%s",executable)>=(int)sizeof(directory))return;
 if(snprintf(file[0],sizeof(file[0]),"%s/halo-ce-master.json",directory)>=(int)sizeof(file[0])||snprintf(file[1],sizeof(file[1]),"%s/halo-pc-master.json",directory)>=(int)sizeof(file[1]))return;
 for(i=0;i<2;i++){
  struct _stat st;FILE *f;char *data;size_t size;int n;
  if(_stat(file[i],&st)!=0){classic_count[i]=0;classic_loaded[i]=1;classic_mtime[i]=0;classic_size[i]=-1;continue;}
  if(classic_loaded[i]&&classic_mtime[i]==st.st_mtime&&classic_size[i]==st.st_size)continue;
  if(st.st_size<0||st.st_size>LIMIT){classic_loaded[i]=1;classic_mtime[i]=st.st_mtime;classic_size[i]=st.st_size;classic_count[i]=0;continue;}
  f=fopen(file[i],"rb");if(!f)continue;size=(size_t)st.st_size;data=(char*)malloc(size+1);
  if(!data){fclose(f);continue;}
  if(fread(data,1,size,f)!=size){free(data);fclose(f);continue;}fclose(f);data[size]=0;
  n=parse_classic_feed(data,size,classic_games[i],i==1);free(data);
  classic_loaded[i]=1;classic_mtime[i]=st.st_mtime;classic_size[i]=st.st_size;
  if(n>=0)classic_count[i]=n;
 }
}

static int fetch_thread(void *unused) {
 char err[160]="";FILE *f=NULL;char *buf=NULL;long length=-1;int n=-1,ok=0;struct community_directory_game parsed[MAX_GAMES];(void)unused;
 if(update_download(URL,path,NULL,NULL,err,sizeof(err))){f=fopen(path,"rb");if(f&&fseek(f,0,SEEK_END)==0&&(length=ftell(f))>=0&&length<=LIMIT&&fseek(f,0,SEEK_SET)==0){size_t z=(size_t)length;buf=(char*)malloc(z+1);if(buf&&fread(buf,1,z,f)==z){buf[z]=0;n=parse_feed(buf,z,parsed);ok=n>=0;}}if(f)fclose(f);}
 free(buf);unlink(path);pthread_mutex_lock(&lock);if(ok){memcpy(games,parsed,(size_t)n*sizeof(parsed[0]));game_count=n;snprintf(status,sizeof(status),"Community directory updated (%d games)",n);}else snprintf(status,sizeof(status),"Community directory unavailable");done=1;pthread_mutex_unlock(&lock);return 0;
}
void community_directory_poll(void) {
 Uint64 now=SDL_GetTicks();pthread_mutex_lock(&lock);if(done&&now>=next_poll){char *pref;int length;if(thread){SDL_WaitThread(thread,NULL);thread=NULL;}next_poll=now+POLL_MS;pref=SDL_GetPrefPath("OpenCE","CommunityDirectory");if(!pref){snprintf(status,sizeof(status),"Could not prepare community directory cache");pthread_mutex_unlock(&lock);return;}length=snprintf(path,sizeof(path),"%scommunity-feed-%ld.json",pref,(long)getpid());SDL_free(pref);if(length<0||(size_t)length>=sizeof(path)){snprintf(status,sizeof(status),"Community directory cache path is too long");pthread_mutex_unlock(&lock);return;}done=0;thread=SDL_CreateThread(fetch_thread,"community directory",NULL);if(!thread){done=1;snprintf(status,sizeof(status),"Could not start community directory request");}}pthread_mutex_unlock(&lock);
}
int community_directory_games(struct community_directory_game *out,int cap) {int n;if(!out||cap<=0)return 0;pthread_mutex_lock(&lock);n=game_count<cap?game_count:cap;memcpy(out,games,(size_t)n*sizeof(games[0]));pthread_mutex_unlock(&lock);return n;}
const char *community_directory_status(void) {static _Thread_local char copy[96];pthread_mutex_lock(&lock);snprintf(copy,sizeof(copy),"%s",status);pthread_mutex_unlock(&lock);return copy;}

int community_directory_classic_games(struct community_directory_game *out,int capacity) {
 int copied=0,i,j;if(!out||capacity<=0)return 0;pthread_mutex_lock(&lock);refresh_classic();
 for(i=0;i<2&&copied<capacity;i++)for(j=0;j<classic_count[i]&&copied<capacity;j++)out[copied++]=classic_games[i][j];
 pthread_mutex_unlock(&lock);return copied;
}

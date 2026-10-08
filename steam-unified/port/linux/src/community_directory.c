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
#include "posix.h"
#include <strings.h>
#define URL "https://halo.milenko.org/v1/games"
#define LIMIT (1024 * 1024)
#define TEST_WINSOCK_SO_ERROR 0x1007 /* translated to host SO_ERROR by posix_socket_getsockopt */
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


/* Test-only directory transport. The regular directory uses the verified HTTPS updater. */
static int test_loopback_url(const char *url, unsigned short *port, char *path_part, size_t path_size)
{
 static const char prefix[] = "http://127.0.0.1:";
 const char *number, *slash, *part;
 unsigned long value = 0;
 size_t length;
 if (!url || strncmp(url, prefix, sizeof(prefix) - 1)) return 0;
 number = url + sizeof(prefix) - 1;
 slash = strchr(number, '/');
 if (!slash || slash == number) return 0;
 while (number < slash) {
  if (*number < '0' || *number > '9') return 0;
  value = value * 10 + (unsigned long)(*number++ - '0');
  if (value > 65535) return 0;
 }
 if (!value) return 0;
 length = strlen(slash);
 if (!length || length >= path_size) return 0;
 for (part = slash; *part; part++)
  if ((unsigned char)*part <= 32 || *part == 127) return 0;
 memcpy(path_part, slash, length + 1);
 *port = (unsigned short)value;
 return 1;
}

/* One bounded HTTP/1.1 response, no redirects or chunked bodies. */

static int test_wait_socket(int socket, int writing, Uint64 deadline)
{
 while (SDL_GetTicks() < deadline) {
  int readable = socket, writable = socket, read_count = 1, write_count = 1;
  Uint64 remaining = deadline - SDL_GetTicks();
  int result = posix_socket_select(writing ? NULL : &readable, writing ? NULL : &read_count,
   writing ? &writable : NULL, writing ? &write_count : NULL, NULL, NULL,
   0, (posix_long)(remaining * 1000), 0);
  if (result > 0) return 1;
  return 0;
 }
 return 0;
}

/* One bounded HTTP/1.1 response, no redirects or chunked bodies. */
static int test_loopback_get(const char *url, char **body, size_t *body_size)
{
 char request[3072], path_part[2048];
 unsigned short port;
 struct sockaddr_in address;
 char *response = NULL, *separator, *line, *line_end;
 size_t used = 0, capacity = LIMIT + 8193, header_size, body_offset, expected = 0;
 int socket = -1, status = 0, have_length = 0, result = 0, request_size;
 int socket_error = 0, socket_error_size = sizeof(socket_error);
 Uint64 deadline;
 *body = NULL;
 *body_size = 0;
 if (!test_loopback_url(url, &port, path_part, sizeof(path_part))) return 0;
 socket = posix_socket(AF_INET, SOCK_STREAM, 0);
 if (socket < 0) goto done;
 posix_socket_set_nonblocking(socket, 1);
 memset(&address, 0, sizeof(address));
 address.sin_family = AF_INET;
 address.sin_port = SDL_Swap16(port);
 address.sin_addr.s_addr = SDL_Swap32(0x7f000001U);
 if (posix_socket_connect(socket, &address, sizeof(address)) < 0) {
  int error = posix_socket_last_error();
  if (error != WSAEWOULDBLOCK && error != WSAEINPROGRESS) goto done;
  deadline = SDL_GetTicks() + 5000;
  if (!test_wait_socket(socket, 1, deadline) ||
      posix_socket_getsockopt(socket, SOL_SOCKET, TEST_WINSOCK_SO_ERROR, &socket_error, &socket_error_size) < 0 ||
      socket_error) goto done;
 }
 request_size = snprintf(request, sizeof(request),
  "GET %s HTTP/1.1\r\nHost: 127.0.0.1:%u\r\nAccept: application/json\r\nConnection: close\r\n\r\n",
  path_part, (unsigned int)port);
 if (request_size < 0 || (size_t)request_size >= sizeof(request)) goto done;
 deadline = SDL_GetTicks() + 5000;
 {
  size_t sent = 0;
  while (sent < (size_t)request_size) {
   int amount = posix_socket_send(socket, request + sent, request_size - (int)sent, 0);
   if (amount > 0) sent += (size_t)amount;
   else if (posix_socket_last_error() == WSAEWOULDBLOCK) {
    if (!test_wait_socket(socket, 1, deadline)) goto done;
   } else goto done;
  }
 }
 response = (char *)malloc(capacity + 1);
 if (!response) goto done;
 deadline = SDL_GetTicks() + 5000;
 while (used < capacity) {
  int amount;
  if (!test_wait_socket(socket, 0, deadline)) goto done;
  amount = posix_socket_recv(socket, response + used, (int)(capacity - used), 0);
  if (amount < 0 && posix_socket_last_error() == WSAEWOULDBLOCK) continue;
  if (amount < 0) goto done;
  if (!amount) break;
  used += (size_t)amount;
 }
 if (used == capacity) goto done;
 response[used] = 0;
 separator = strstr(response, "\r\n\r\n");
 if (!separator || (size_t)(separator - response) > 8192 ||
     sscanf(response, "HTTP/1.%*d %d", &status) != 1 || status != 200) goto done;
 header_size = (size_t)(separator - response);
 body_offset = header_size + 4;
 line = strstr(response, "\r\n");
 if (!line || (size_t)(line - response) >= header_size) goto done;
 line += 2;
 while (line < separator) {
  unsigned long parsed_length = 0;
  char *value;
  line_end = strstr(line, "\r\n");
  if (!line_end || line_end > separator) goto done;
  if ((size_t)(line_end - line) >= 15 && !strncasecmp(line, "Content-Length:", 15)) {
   if (have_length) goto done;
   value = line + 15;
   while (value < line_end && (*value == ' ' || *value == '\t')) value++;
   if (value == line_end) goto done;
   while (value < line_end) {
    if (*value < '0' || *value > '9') goto done;
    {
     unsigned long digit = (unsigned long)(*value++ - '0');
     if (parsed_length > (LIMIT - digit) / 10) goto done;
     parsed_length = parsed_length * 10 + digit;
    }
   }
   expected = (size_t)parsed_length;
   have_length = 1;
  } else if ((size_t)(line_end - line) >= 18 && !strncasecmp(line, "Transfer-Encoding:", 18)) goto done;
  line = line_end + 2;
 }
 if (!have_length || used < body_offset || used - body_offset != expected) goto done;
 memmove(response, response + body_offset, expected);
 response[expected] = 0;
 *body = response;
 *body_size = expected;
 response = NULL;
 result = 1;
done:
 if (socket >= 0) posix_socket_close(socket);
 free(response);
 return result;
}

static int fetch_https_feed(struct community_directory_game *parsed, int *count)
{
 char err[160] = ""; FILE *f = NULL; char *buf = NULL; long length = -1; int ok = 0;
 if (update_download(URL, path, NULL, NULL, err, sizeof(err))) {
  f = fopen(path, "rb");
  if (f && fseek(f, 0, SEEK_END) == 0 && (length = ftell(f)) >= 0 &&
      length <= LIMIT && fseek(f, 0, SEEK_SET) == 0) {
   size_t size = (size_t)length;
   buf = (char *)malloc(size + 1);
   if (buf && fread(buf, 1, size, f) == size) {
    buf[size] = 0;
    *count = parse_feed(buf, size, parsed);
    ok = *count >= 0;
   }
  }
  if (f) fclose(f);
 }
 free(buf);
 return ok;
}

static int fetch_thread(void *unused)
{
 const char *test_url = getenv("HALO_COMMUNITY_DIRECTORY_TEST_URL");
 char *buf = NULL;
 size_t size = 0;
 int count = -1, ok = 0;
 struct community_directory_game parsed[MAX_GAMES];
 (void)unused;
 if (test_url && *test_url) {
  if (test_loopback_get(test_url, &buf, &size)) {
   count = parse_feed(buf, size, parsed);
   ok = count >= 0;
  }
 } else ok = fetch_https_feed(parsed, &count);
 free(buf);
 unlink(path);
 pthread_mutex_lock(&lock);
 if (ok) {
  memcpy(games, parsed, (size_t)count * sizeof(parsed[0]));
  game_count = count;
  snprintf(status, sizeof(status), "Community directory updated (%d games)", count);
 } else snprintf(status, sizeof(status), "Community directory unavailable");
 done = 1;
 pthread_mutex_unlock(&lock);
 return 0;
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

/* Content directory bridge. Horizon sdmc/romfs have no symlink objects.
 * Virtual directory tokens carry bounded absolute paths; relative names are
 * single components. Native filesystem rename never replaces destinations. */
#include "host.h"
#include "host_file_io.h"
#include "posix.h"
#include <switch.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
struct content_request { uint32_t version,op; int32_t fd,fd2; uint32_t path,path2,flags,output; };
struct content_identity { uint32_t device_low,device_high,inode_low,inode_high; };
_Static_assert(sizeof(struct content_request)==32,"content fs ABI");
#define BASE 0x40000000
#define COUNT 64
#define CAP 1024
static char directories[COUNT][CAP];
static Mutex lock;
static struct { char path[CAP]; uint32_t revision; } identities[128];
static uint32_t revision=1;
static void changed(const char *p){for(unsigned j=0;j<128;j++)if(!strcasecmp(identities[j].path,p))identities[j].revision=++revision;}
static int slot(int fd){int i=fd-BASE;return i>=0&&i<COUNT&&directories[i][0]?i:-1;}
static int duplicate_directory(int fd){
 int i=slot(fd);if(i<0)return -EBADF;
 for(int j=0;j<COUNT;j++)if(!directories[j][0]){strcpy(directories[j],directories[i]);return BASE+j;}
 return -EMFILE;
}
static int validate_open_flags(uint32_t flags){
 if((flags&~(3u|0x40u|0x80u|0x200u|0x400u|0x10000u))||(flags&3)==3)return EINVAL;
 if((flags&0x10000)&&(flags&(3|0x40|0x80|0x200|0x400)))return EINVAL;
 return 0;
}
static const char *guest_string(uint32_t p) {
 if(!p||p>UINT32_MAX-CAP)return NULL;
 for(size_t i=0;i<CAP;i++){if(!host_low_owns(p+i,1))return NULL;if(!((char *)(uintptr_t)p)[i])return (char *)(uintptr_t)p;}
 return NULL;
}
static int component(const char *p){return p&&*p&&strcmp(p,".")&&strcmp(p,"..")&&!strchr(p,'/')&&!strchr(p,'\\')&&!strchr(p,':');}
static int allowed(const char *p) {
 const char *s;
 if(!strncmp(p,"sdmc:/switch/halo/",18))s=p+18;
 else if(!strcmp(p,"sdmc:/switch/halo"))return 1;
 else if(!strncmp(p,"romfs:/",7))s=p+7;
 else return 0;
 while(*s){const char *end=strchr(s,'/');size_t n=end?(size_t)(end-s):strlen(s);
  if(!n||(n==1&&s[0]=='.')||(n==2&&s[0]=='.'&&s[1]=='.')||memchr(s,'\\',n)||memchr(s,':',n))return 0;
  if(!end)return 1;
  s=end+1;
 }return 1;
}
static int path_for(int fd,const char *name,char out[CAP]){
 int i=slot(fd),n;
 if(fd==-100){if(!name||!allowed(name))return EACCES;n=snprintf(out,CAP,"%s",name);}
 else {if(i<0)return EBADF;if(!name)n=snprintf(out,CAP,"%s",directories[i]);else {if(!component(name))return EINVAL;n=snprintf(out,CAP,"%s/%s",directories[i],name);}}
 if(n<0||n>=CAP)return ENAMETOOLONG;
 size_t len=strlen(out);while(len>7&&out[len-1]=='/')out[--len]=0;
 return 0;
}
static int stat_directory(const char *p,struct stat *st){if(stat(p,st))return errno;return S_ISDIR(st->st_mode)?0:ENOTDIR;}
static int service(struct content_request *r) {
 char p[CAP],q[CAP];struct stat st;int e,i=slot(r->fd),ret;
 const char *a=r->path?guest_string(r->path):NULL,*b=r->path2?guest_string(r->path2):NULL;
 if((r->path&&!a)||(r->path2&&!b))return -EFAULT;
 if(r->op==2){if(i>=0){directories[i][0]=0;return 0;}ret=host_file_close(r->fd);return ret<0?-errno:ret;}
 if(r->op==11){if(i>=0)return duplicate_directory(r->fd);
  ret=host_file_dup(r->fd);return ret<0?-errno:ret;}
 if(r->op==3){if(i>=0){Result rc=fsdevCommitDevice("sdmc");return R_SUCCEEDED(rc)?0:-EIO;}ret=host_file_sync(r->fd);return ret<0?-errno:ret;}
 if(r->op==6){if(!host_low_owns(r->output,sizeof(struct posix_file_information)))return -EFAULT;
  if(i>=0)ret=posix_stat(directories[i],(void *)(uintptr_t)r->output);else ret=posix_fstat(r->fd,(void *)(uintptr_t)r->output);return ret<0?-errno:ret;}
 if(r->op==7){if(i>=0)return 0;if(host_file_fstat(r->fd,&st))return -errno;return S_ISREG(st.st_mode);}
 if((e=path_for(r->fd,a,p)))return -e;
 switch(r->op){
 case 1:
  if((e=validate_open_flags(r->flags)))return -e;
  if(r->flags&0x10000){if((e=stat_directory(p,&st)))return -e;for(int j=0;j<COUNT;j++)if(!directories[j][0]){strcpy(directories[j],p);return BASE+j;}return -EMFILE;}
  if(!stat(p,&st)&&!S_ISREG(st.st_mode))return -EISDIR;
  {int f=r->flags&3;if(r->flags&0x40)f|=O_CREAT;if(r->flags&0x80)f|=O_EXCL;if(r->flags&0x200)f|=O_TRUNC;if(r->flags&0x400)f|=O_APPEND;
   if((f&(O_CREAT|O_TRUNC))||(f&O_ACCMODE)!=O_RDONLY){if(strncmp(p,"sdmc:/",6))return -EROFS;}
   ret=host_file_open(p,f,0600);return ret<0?-errno:ret;}
 case 4:
  if(strncmp(p,"sdmc:/",6))return -EROFS;
  if(r->flags&~0x200)return -EINVAL;
  ret=(r->flags&0x200)?rmdir(p):unlink(p);if(!ret)changed(p);return ret<0?-errno:ret;
 case 5:
  if((e=path_for(r->fd2,b,q)))return -e;
  if(strncmp(p,"sdmc:/",6)||strncmp(q,"sdmc:/",6))return -EXDEV;
  if(stat(p,&st))return -errno;
  {struct stat target;if(!stat(q,&target))return -EEXIST;if(errno!=ENOENT)return -errno;
   FsFileSystem *fs=fsdevGetDeviceFileSystem("sdmc");if(!fs)return -ENODEV;
   Result rc=S_ISDIR(st.st_mode)?fsFsRenameDirectory(fs,p+5,q+5):fsFsRenameFile(fs,p+5,q+5);
   if(R_FAILED(rc)){host_logf(HOST_LOG_WARN,"content rename failed rc=%x",rc);return -EIO;}changed(p);changed(q);return 0;}
 case 8:if(strncmp(p,"sdmc:/",6))return -EROFS;ret=mkdir(p,0700);if(!ret)changed(p);return ret<0?-errno:ret;
 case 9:if((e=stat_directory(p,&st)))return -e;{errno=0;void *d=posix_directory_open(p);return d?(int)(uintptr_t)d:-(errno?errno:EMFILE);}
 case 10:
  if(!host_low_owns(r->output,sizeof(struct content_identity)))return -EFAULT;
  if((e=stat_directory(p,&st)))return -e;
  /* FAT inode values are not stable identifiers. Canonical device/path hash
     identifies aliases for the lifetime of a pending remove confirmation. */
  {uint64_t h=1469598103934665603ULL;for(const unsigned char *s=(void *)p;*s;s++){unsigned char c=*s;if(c>='A'&&c<='Z')c+=32;h=(h^c)*1099511628211ULL;}
   int identity=-1;for(unsigned j=0;j<128;j++)if(!strcasecmp(identities[j].path,p)){identity=j;break;}
   if(identity<0)for(unsigned j=0;j<128;j++)if(!identities[j].path[0]){identity=j;strcpy(identities[j].path,p);identities[j].revision=++revision;break;}
   if(identity<0)return -ENOSPC;
   struct content_identity *out=(void *)(uintptr_t)r->output;out->device_low=1;out->device_high=identities[identity].revision;out->inode_low=(uint32_t)h;out->inode_high=(uint32_t)(h>>32);return 0;}
 default:return -ENOSYS;
 }
}
/* Fixed wire errno numbers are Linux/musl values, not devkitPro newlib. */
static int wire_result(int result){
 if(result>=0)return result;
 int e=-result;if(e>=1&&e<=34)return -e;
 switch(e){
 case ENAMETOOLONG:return -36;case ENOSYS:return -38;
 case ENOTEMPTY:return -39;case ELOOP:return -40;
 case ENOTSUP:return -95;case EOVERFLOW:return -75;
 case ECANCELED:return -125;case EDEADLK:return -35;
 default:return -5;
 }
}
int host_content_fs(uint32_t address){
 if(!host_low_owns(address,sizeof(struct content_request)))return -EFAULT;
 struct content_request r=*(struct content_request *)(uintptr_t)address;if(r.version!=2)return -EINVAL;
 mutexLock(&lock);int result=service(&r);mutexUnlock(&lock);return wire_result(result);
}

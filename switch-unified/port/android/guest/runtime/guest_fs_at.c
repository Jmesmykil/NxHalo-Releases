/* Fixed-width content service. Host returns negative errno; never host pointers. */
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include "../../../linux/src/posix.h"
extern int host_content_fs(uint32_t);
struct content_request { uint32_t version,op; int32_t fd,fd2; uint32_t path,path2,flags,output; };
_Static_assert(sizeof(struct content_request)==32,"content fs ABI");
static int call(unsigned op,int fd,int fd2,const char *p,const char *q,unsigned flags,void *out) {
 struct content_request r={2,op,fd,fd2,(uint32_t)(uintptr_t)p,(uint32_t)(uintptr_t)q,flags,(uint32_t)(uintptr_t)out};
 int result=host_content_fs((uint32_t)(uintptr_t)&r);if(result<0){errno=-result;return -1;}return result;
}
/* Wire flags are explicit and independent of both arm64_32 musl and newlib. */
static int open_wire(int fd,const char *p,int flags){
 const int supported=O_ACCMODE|O_CREAT|O_EXCL|O_TRUNC|O_APPEND|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK;
 unsigned wire=flags&O_ACCMODE;
 if((flags&~supported)||wire==3){errno=EINVAL;return -1;}
 if(flags&O_CREAT)wire|=0x40;if(flags&O_EXCL)wire|=0x80;
 if(flags&O_TRUNC)wire|=0x200;if(flags&O_APPEND)wire|=0x400;
 if(flags&O_DIRECTORY)wire|=0x10000;
 return call(1,fd,0,p,0,wire,0);
}
int content_open(const char *p,int flags,...){return open_wire(-100,p,flags);}
int content_openat(int fd,const char *p,int flags,...){return open_wire(fd,p,flags);}
int content_close(int fd){return call(2,fd,0,0,0,0,0);}
int content_dup(int fd){return call(11,fd,0,0,0,0,0);}
char *content_mkdtemp(char *p){
 static const char chars[]="abcdefghijklmnopqrstuvwxyz0123456789";unsigned char random[6];
 size_t n=p?strlen(p):0;if(n<6||strcmp(p+n-6,"XXXXXX")){errno=EINVAL;return NULL;}
 for(int attempt=0;attempt<128;attempt++){
  posix_random_bytes(random,sizeof(random));for(unsigned i=0;i<6;i++)p[n-6+i]=chars[random[i]%36];
  if(call(8,-100,0,p,0,0,0)==0)return p;
  if(errno!=EEXIST)return NULL;
 }errno=EEXIST;return NULL;
}
int content_fsync(int fd){return call(3,fd,0,0,0,0,0);}
int content_unlinkat(int fd,const char *p,int flags){return call(4,fd,0,p,0,flags,0);}
int content_rmdir(const char *p){return call(4,-100,0,p,0,0x200,0);}
int content_renameat(int a,const char *p,int b,const char *q){return call(5,a,b,p,q,0,0);}
int content_fstat(int fd,struct posix_file_information *out){return call(6,fd,0,0,0,0,out);}
int content_regular(int fd){int r=call(7,fd,0,0,0,0,0);return r>0;}
int content_mkdirat(int fd,const char *p){return call(8,fd,0,p,0,0,0);}
int content_rename_noreplace(int fd,const char *p,const char *q){return call(5,fd,fd,p,q,0,0);}
void *content_diropenfd(int fd){int r=call(9,fd,0,0,0,0,0);return r<0?0:(void *)(uintptr_t)r;}
int content_identity_fd(int fd,struct posix_directory_identity *out){return call(10,fd,0,0,0,0,out);}
int content_identity_at(int fd,const char *p,struct posix_directory_identity *out){return call(10,fd,0,p,0,0,out);}
int content_identity_path(const char *p,struct posix_directory_identity *out){return call(10,-100,0,p,0,0,out);}

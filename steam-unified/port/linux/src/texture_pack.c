/*
Optional, local bitmap replacement packs for the native renderer.
Pack files are addressed by tag path and ordinal: <tag path>__<index>.<ext>.
Only PNG, uncompressed true-color TGA, and DDS BC1/2/3 are decoded.
*/
#include "texture_pack.h"
#include "hud_hires.h"
#include "gl.h"
void xgpu_gl_state_invalidate(void);
#include "port_config.h"
#include "posix.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PACK_CACHE_MAX 64
#define PACK_CACHE_BYTES_MAX (128UL << 20)
#define PACK_FILE_MAX (32UL << 20)
#define PACK_DIM_MAX 4096UL
#define PACK_ROOT_MAX 1024
#define PACK_NAME_MAX 64

struct cached_pack_texture { char path[PACK_ROOT_MAX]; GLuint texture; unsigned long levels, used, bytes; };
static struct cached_pack_texture cache[PACK_CACHE_MAX];
static unsigned long cache_clock, cache_bytes;
static char selected[PACK_ROOT_MAX];
static unsigned long config_seen = (unsigned long)-1;
static int enabled_seen;

static int safe_component(const char *s, size_t max)
{
	size_t n, i;
	if (!s || !(n = strlen(s)) || n >= max || !strcmp(s, ".") || !strcmp(s, "..")) return 0;
	for (i=0; i<n; i++) if (!((s[i]>='a'&&s[i]<='z')||(s[i]>='A'&&s[i]<='Z')||
		(s[i]>='0'&&s[i]<='9')||s[i]=='_'||s[i]=='-'||s[i]=='.'||s[i]==' ')) return 0;
	return 1;
}

static void refresh_config(void)
{
	unsigned long changes = config_changes();
	if (changes == config_seen) return;
	config_seen = changes;
	enabled_seen = config_boolean("display.texture_pack_enabled");
	snprintf(selected, sizeof(selected), "%s", config_string("display.texture_pack_path"));
}

const char *texture_pack_selected(void) { refresh_config(); return selected; }
int texture_pack_enabled(void) { refresh_config(); return enabled_seen; }

static int directory_nofollow(const char *path)
{
	int fd, ok; struct posix_file_information info;
	fd=open(path,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);
	if(fd<0)return 0;
	ok=posix_fstat(fd,&info)==0 && (info.flags&_posix_file_is_directory);
	close(fd); return ok;
}
static int regular_file_nofollow(const char *path, int *descriptor, struct posix_file_information *info)
{
	int fd=open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK);
	if(fd<0)return 0;
	if(posix_fstat(fd,info)||!posix_file_is_regular(fd)){close(fd);return 0;}
	if(descriptor)*descriptor=fd;else close(fd);
	return 1;
}

int texture_pack_set_enabled(int enabled)
{
	if (!config_write_boolean("display.texture_pack_enabled", enabled != 0)) return 0;
	refresh_config(); return 1;
}

static int pack_base(char *out, size_t size)
{
	char folder[PACK_ROOT_MAX];
	config_folder(folder, sizeof(folder));
	if (snprintf(out, size, "%stexture-packs", folder) >= (int)size) return 0;
	return 1;
}

static int path_tag(char *out, size_t size, const char *tag, long bitmap)
{
	size_t n=0, i, component=0, suffix;
	if (!tag || bitmap < 0) return 0;
	for (i=0; tag[i] && n+24<size; i++) {
		unsigned char c=(unsigned char)tag[i];
		if (c=='\\' || c=='/') {
			size_t length=n-component;
			if (!length || (length==1&&out[component]=='.') ||
				(length==2&&out[component]=='.'&&out[component+1]=='.')) return 0;
			out[n++]='/'; component=n;
		}
		else if ((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||
			c=='_'||c=='-'||c=='.'||c==' ') out[n++]=(char)c;
		else return 0;
	}
	if (tag[i] || n==component || (n-component==1&&out[component]=='.') ||
		(n-component==2&&out[component]=='.'&&out[component+1]=='.')) return 0;
	out[n]=0;
	suffix=(size_t)snprintf(out+n,size-n,"__%ld",bitmap);
	return suffix<size-n;
}

static unsigned int le32(const unsigned char *p) { return (unsigned int)p[0]|((unsigned int)p[1]<<8)|((unsigned int)p[2]<<16)|((unsigned int)p[3]<<24); }
static unsigned int be32(const unsigned char *p) { return ((unsigned int)p[0]<<24)|((unsigned int)p[1]<<16)|((unsigned int)p[2]<<8)|p[3]; }

/* Bound resident GL texture memory as well as entry count. PNG/TGA uploads
use RGBA8 plus generated mipmaps; DDS uses its exact BC block footprint. */
static unsigned long texture_memory_estimate(unsigned long width, unsigned long height,
	unsigned long levels, unsigned long block_bytes)
{
	unsigned long total = 0;
	while (levels--)
	{
		unsigned long level_bytes;
		if (block_bytes)
			level_bytes = ((width + 3) / 4) * ((height + 3) / 4) * block_bytes;
		else
			level_bytes = width * height * 4;
		if (level_bytes > PACK_CACHE_BYTES_MAX || total > PACK_CACHE_BYTES_MAX - level_bytes)
			return 0;
		total += level_bytes;
		if (width > 1) width >>= 1;
		if (height > 1) height >>= 1;
	}
	return total;
}

static int cache_oldest_slot(void)
{
	int i, slot = -1;
	unsigned long oldest = ~0UL;
	for (i = 0; i < PACK_CACHE_MAX; i++)
		if (cache[i].texture && cache[i].used < oldest)
		{
			oldest = cache[i].used;
			slot = i;
		}
	return slot;
}

static void cache_evict(int slot)
{
	if (slot < 0 || slot >= PACK_CACHE_MAX || !cache[slot].texture)
		return;
	glDeleteTextures(1, &cache[slot].texture);
	if (cache_bytes >= cache[slot].bytes) cache_bytes -= cache[slot].bytes;
	else cache_bytes = 0;
	memset(&cache[slot], 0, sizeof(cache[slot]));
	xgpu_gl_state_invalidate();
}

static unsigned char *read_file(const char *path, unsigned long *size)
{
	int fd; unsigned char *data; unsigned long total=0; struct posix_file_information info;
	*size=0;
	if(!regular_file_nofollow(path,&fd,&info)||info.size_high||!info.size_low||info.size_low>PACK_FILE_MAX){return NULL;}
	data=malloc(info.size_low); if(!data){close(fd);return NULL;}
	while(total<info.size_low){
		ssize_t n=read(fd,data+total,info.size_low-total);
		if(n<=0){free(data);close(fd);return NULL;} total+=(unsigned long)n;
	}
	close(fd); *size=total; return data;
}

static GLuint tga_texture(const unsigned char *d, unsigned long size, unsigned long *levels)
{
	unsigned long w,h,count,i,off; int bits,top; unsigned char *rgba; GLuint tex;
	if (size<18 || d[1]!=0 || d[2]!=2 || (d[16]!=24 && d[16]!=32)) return 0;
	w=d[12]|d[13]<<8; h=d[14]|d[15]<<8; bits=d[16];
	off=18+d[0]; count=w*h;
	if (!w||!h||w>PACK_DIM_MAX||h>PACK_DIM_MAX||count>16UL*1024*1024||off>size||count*(bits/8)>size-off) return 0;
	rgba=malloc(count*4); if (!rgba) return 0;
	top=(d[17]&0x20)!=0;
	for(i=0;i<count;i++) {
		unsigned long row=i/w,col=i%w,src=(top?row:h-1-row)*w+col;
		const unsigned char *p=d+off+src*(bits/8);
		rgba[i*4]=p[2]; rgba[i*4+1]=p[1]; rgba[i*4+2]=p[0]; rgba[i*4+3]=bits==32?p[3]:255;
	}
	glGenTextures(1,&tex); glBindTexture(GL_TEXTURE_2D,tex); xgpu_gl_state_invalidate();
	glPixelStorei(GL_UNPACK_ALIGNMENT,1); glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
	glGenerateMipmap(GL_TEXTURE_2D); xgpu_gl_state_invalidate(); free(rgba);
	*levels=1; for(i=w>h?w:h;i>1;i>>=1)(*levels)++; return tex;
}

static GLuint dds_texture(const unsigned char *d, unsigned long size, unsigned long *levels)
{
	unsigned long w,h,offset=128,level=0; unsigned int fourcc,mips,format; GLuint tex;
	if(size<128||memcmp(d,"DDS ",4)||le32(d+4)!=124||le32(d+76)!=32||!(le32(d+80)&4)) return 0;
	h=le32(d+12); w=le32(d+16); fourcc=le32(d+84); mips=le32(d+28); if(!mips)mips=1;
	if(!w||!h||w>PACK_DIM_MAX||h>PACK_DIM_MAX||mips>16) return 0;
	if(fourcc==0x31545844) format=0x83f1; else if(fourcc==0x33545844) format=0x83f2; else if(fourcc==0x35545844) format=0x83f3; else return 0;
	glGenTextures(1,&tex); glBindTexture(GL_TEXTURE_2D,tex); xgpu_gl_state_invalidate(); glPixelStorei(GL_UNPACK_ALIGNMENT,1);
	for(level=0;level<mips;level++) {
		unsigned long bw=(w+3)/4,bh=(h+3)/4,bytes=bw*bh*(format==0x83f1?8:16);
		if(bytes>size-offset) { glDeleteTextures(1,&tex); xgpu_gl_state_invalidate(); return 0; }
		glCompressedTexImage2D(GL_TEXTURE_2D,(GLint)level,(GLenum)format,(GLsizei)w,(GLsizei)h,0,(GLsizei)bytes,d+offset);
		offset+=bytes; if(w>1)w>>=1;if(h>1)h>>=1;
	}
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_BASE_LEVEL,0); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,(GLint)mips-1);
	xgpu_gl_state_invalidate(); *levels=mips; return tex;
}

static GLuint load_texture(const char *path, unsigned long *levels, unsigned long *memory_bytes)
{
	static char traced[16][PACK_ROOT_MAX];
	static unsigned int traced_count;
	unsigned long size=0; unsigned char *data=read_file(path,&size); GLuint tex=0;
	unsigned long width=0,height=0,block_bytes=0; const char *format="TGA";
	if (memory_bytes) *memory_bytes=0;
	if(!data)return 0;
	if(size>=24&&!memcmp(data,"\x89PNG\r\n\x1a\n",8) &&
		be32(data+16)>0 && be32(data+20)>0 && be32(data+16)<=PACK_DIM_MAX &&
		be32(data+20)<=PACK_DIM_MAX && (unsigned long long)be32(data+16)*be32(data+20)<=16000000ULL) {
		width=be32(data+16); height=be32(data+20); format="PNG";
		tex=hud_hires_png_texture(data,size,levels);
	}
	else if(size>=4&&!memcmp(data,"DDS ",4)) {
		width=le32(data+16); height=le32(data+12); format="DDS-BC1/2/3";
		if (le32(data+84)==0x31545844) block_bytes=8;
		else if (le32(data+84)==0x33545844 || le32(data+84)==0x35545844) block_bytes=16;
		tex=dds_texture(data,size,levels);
	}
	else {
		if(size>=18){width=data[12]|data[13]<<8;height=data[14]|data[15]<<8;}
		tex=tga_texture(data,size,levels);
	}
	if (tex && memory_bytes) {
		*memory_bytes = texture_memory_estimate(width, height, *levels, block_bytes);
		if (!*memory_bytes) { glDeleteTextures(1, &tex); tex = 0; xgpu_gl_state_invalidate(); }
	}
	if(tex && getenv("HALO_TEXTURE_PACK_TRACE") && getenv("HALO_TEXTURE_PACK_TRACE")[0]) {
		unsigned int i; int seen=0;
		for(i=0;i<traced_count;i++)if(!strcmp(traced[i],path)){seen=1;break;}
		if(!seen && traced_count<16){
			fprintf(stderr,"texture pack override loaded: %s (%s %lux%lu)\n",path,format,width,height);
			snprintf(traced[traced_count],sizeof(traced[traced_count]),"%s",path);traced_count++;
		}
	}
	free(data); return tex;
}

unsigned int texture_pack_override(const char *tag, long bitmap, unsigned long *levels)
{
	char key[PACK_ROOT_MAX], path[PACK_ROOT_MAX]; int i,slot=-1; GLuint texture;
	refresh_config(); if(!enabled_seen||!selected[0]||!levels)return 0;
	if(!path_tag(key,sizeof(key),tag,bitmap))return 0;
	for(i=0;i<3;i++) {
		char full[PACK_ROOT_MAX]; const char *ext=i==0?".png":i==1?".tga":".dds";
		if(snprintf(full,sizeof(full),"%s/%s%s",selected,key,ext)>=(int)sizeof(full))continue;
		for(int c=0;c<PACK_CACHE_MAX;c++) if(cache[c].texture&&!strcmp(cache[c].path,full)){cache[c].used=++cache_clock;*levels=cache[c].levels;return cache[c].texture;}
		unsigned long memory_bytes=0;
		texture=load_texture(full,levels,&memory_bytes); if(!texture)continue;
		while (memory_bytes > PACK_CACHE_BYTES_MAX - cache_bytes)
		{
			slot = cache_oldest_slot();
			if (slot < 0) break;
			cache_evict(slot);
		}
		for(slot=0;slot<PACK_CACHE_MAX && cache[slot].texture;slot++) {}
		if(slot==PACK_CACHE_MAX){slot=cache_oldest_slot();cache_evict(slot);}
		if(slot>=0 && memory_bytes<=PACK_CACHE_BYTES_MAX-cache_bytes){
			strncpy(cache[slot].path,full,sizeof(cache[slot].path)-1);cache[slot].path[sizeof(cache[slot].path)-1]=0;
			cache[slot].texture=texture;cache[slot].levels=*levels;cache[slot].used=++cache_clock;cache[slot].bytes=memory_bytes;
			cache_bytes+=memory_bytes;
		}
		xgpu_gl_state_invalidate(); return texture;
	}
	return 0;
}

/* Import/list/select are intentionally directory-only; no archive or script execution. */
static int pack_path(char *out,size_t cap,const char *name)
{
	char base[PACK_ROOT_MAX]; if(!safe_component(name,PACK_NAME_MAX)||!pack_base(base,sizeof(base)))return 0;
	return snprintf(out,cap,"%s/%s",base,name)<(int)cap;
}
int texture_pack_select(const char *name)
{
	char path[PACK_ROOT_MAX]; if(!pack_path(path,sizeof(path),name))return 0;
	if(!directory_nofollow(path)||!config_write("display.texture_pack_path",path))return 0;
	return 1;
}
int texture_pack_list(texture_pack_list_callback cb,void *ctx)
{
	char base[PACK_ROOT_MAX],name[256]; void *dir; int count=0;
	if(!cb||!pack_base(base,sizeof(base))||(dir=posix_directory_open(base))==NULL)return 0;
	while(posix_directory_next(dir,name,sizeof(name))){
		char p[PACK_ROOT_MAX];
		if(!safe_component(name,sizeof(name))||snprintf(p,sizeof(p),"%s/%s",base,name)>=(int)sizeof(p)||!directory_nofollow(p))continue;
		cb(name,ctx);count++;
	}
	posix_directory_close(dir);return count;
}

/* Bounded recursive importer, accepts image files only and rejects symlinks. */
static int copy_tree(const char *src,const char *dst,unsigned int depth,unsigned long *total,unsigned int *files)
{
	void *dir; char name[256];
	if(depth>12||!directory_nofollow(src)||(dir=posix_directory_open(src))==NULL)return 0;
	if(!posix_make_directory(dst)&&!directory_nofollow(dst)){posix_directory_close(dir);return 0;}
	while(posix_directory_next(dir,name,sizeof(name))){
		char a[PACK_ROOT_MAX],b[PACK_ROOT_MAX];int in_fd,out_fd;unsigned char buf[16384];ssize_t n;unsigned long copied=0;struct posix_file_information info;const char *ext;
		if(!safe_component(name,sizeof(name))||snprintf(a,sizeof(a),"%s/%s",src,name)>=(int)sizeof(a)||
			snprintf(b,sizeof(b),"%s/%s",dst,name)>=(int)sizeof(b)){posix_directory_close(dir);return 0;}
		if(directory_nofollow(a)){if(!copy_tree(a,b,depth+1,total,files)){posix_directory_close(dir);return 0;}continue;}
		if(!regular_file_nofollow(a,&in_fd,&info)||info.size_high||!info.size_low||
			++*files>512||(*total+=info.size_low)>256UL*1024*1024){posix_directory_close(dir);return 0;}
		ext=strrchr(name,'.');
		if(!ext||(strcmp(ext,".png")&&strcmp(ext,".tga")&&strcmp(ext,".dds"))){close(in_fd);posix_directory_close(dir);return 0;}
		out_fd=open(b,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
		if(out_fd<0){close(in_fd);posix_directory_close(dir);return 0;}
		while((n=read(in_fd,buf,sizeof(buf)))>0){
			ssize_t offset=0;
			while(offset<n){ssize_t written=write(out_fd,buf+offset,(size_t)(n-offset));if(written<=0){close(in_fd);close(out_fd);posix_directory_close(dir);return 0;}offset+=written;}
			copied+=(unsigned long)n;
		}
		close(in_fd);close(out_fd);
		if(n<0||copied!=info.size_low){posix_directory_close(dir);return 0;}
	}
	posix_directory_close(dir);return 1;
}

int texture_pack_install(const char *name,const char *source)
{
	char dst[PACK_ROOT_MAX],base[PACK_ROOT_MAX];unsigned long total=0;unsigned int files=0;
	if(!source||!pack_path(dst,sizeof(dst),name)||!pack_base(base,sizeof(base))||!directory_nofollow(source)||directory_nofollow(dst))return 0;
	if(!posix_make_directory(base)&&!directory_nofollow(base))return 0;
	if(!copy_tree(source,dst,0,&total,&files)||!files)return 0;
	return 1;
}

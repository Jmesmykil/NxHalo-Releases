/* Runtime fixture for optional texture-pack dispatch. Uses a fake GL context
to verify actual TGA/DDS upload calls without copyrighted assets or a display. */
#include "texture_pack.h"
#include "gl.h"
#include "port_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "posix.h"

static char root[512], selected[1024];
static int enabled, changes, upload_2d, upload_compressed, mipmaps, png_decodes, seen_w, seen_h;
static GLuint next_texture=10;
static GLenum seen_format;

static void fake_gen(GLsizei n, GLuint *ids) { while(n--) *ids++=next_texture++; }
static void fake_delete(GLsizei n, const GLuint *ids) { (void)n;(void)ids; }
static void fake_bind(GLenum target, GLuint id) { (void)target;(void)id; }
static void fake_pixel(GLenum p, GLint v) { (void)p;(void)v; }
static void fake_image(GLenum target, GLint level, GLint internal, GLsizei w, GLsizei h, GLint border, GLenum format, GLenum type, const void *data)
{ (void)target;(void)level;(void)internal;(void)border;(void)format;(void)type;(void)data; upload_2d++;seen_w=w;seen_h=h; }
static void fake_compressed(GLenum target, GLint level, GLenum format, GLsizei w, GLsizei h, GLint border, GLsizei size, const void *data)
{ (void)target;(void)level;(void)w;(void)h;(void)border;(void)data;upload_compressed++;seen_format=format;if(size!=8)abort(); }
static void fake_mipmap(GLenum target) { (void)target;mipmaps++; }
static void fake_parameter(GLenum target, GLenum pname, GLint value) { (void)target;(void)pname;(void)value; }

__typeof__(halo_glGenTextures) halo_glGenTextures=fake_gen;
__typeof__(halo_glDeleteTextures) halo_glDeleteTextures=fake_delete;
__typeof__(halo_glBindTexture) halo_glBindTexture=fake_bind;
__typeof__(halo_glPixelStorei) halo_glPixelStorei=fake_pixel;
__typeof__(halo_glTexImage2D) halo_glTexImage2D=fake_image;
__typeof__(halo_glCompressedTexImage2D) halo_glCompressedTexImage2D=fake_compressed;
__typeof__(halo_glGenerateMipmap) halo_glGenerateMipmap=fake_mipmap;
__typeof__(halo_glTexParameteri) halo_glTexParameteri=fake_parameter;
void xgpu_gl_state_invalidate(void) {}
unsigned int hud_hires_png_texture(const void *p,unsigned long n,unsigned long *levels) { (void)p;(void)n;png_decodes++;*levels=1;return 77; }

unsigned long config_changes(void) { return (unsigned long)changes; }
int config_boolean(const char *name) { return !strcmp(name,"display.texture_pack_enabled")?enabled:0; }
const char *config_string(const char *name) { return !strcmp(name,"display.texture_pack_path")?selected:""; }
void config_folder(char *out,size_t size) { snprintf(out,size,"%s/cfg/",root); }
int config_write(const char *name,const char *value) { if(strcmp(name,"display.texture_pack_path"))return 0;snprintf(selected,sizeof(selected),"%s",value);changes++;return 1; }
int config_write_boolean(const char *name,int value) { if(strcmp(name,"display.texture_pack_enabled"))return 0;enabled=value;changes++;return 1; }

static void list_found(const char *name,void *ctx) { if(!strcmp(name,"fixture"))*(int *)ctx=1; }
static int make_dir(const char *p) { return posix_make_directory(p)==0; }
static int write_bytes(const char *p,const unsigned char *b,size_t n) { FILE *f=fopen(p,"wb");int ok;if(!f)return 0;ok=fwrite(b,1,n,f)==n;fclose(f);return ok; }
static int require(int condition,const char *message) { if(!condition){fprintf(stderr,"FAIL: %s\n",message);return 0;}return 1; }

int main(void)
{
	char src[1024],sub[1024],file[1200],badsrc[1024],badsub[1024],badfile[1200],symsrc[1024],symfile[1200],linkpath[1200];
	unsigned char tga[34]={0},dds[144]={0},png[24]={0},bad[18]={0};
	unsigned long levels=0; unsigned int tex; int ok=1,listed=0;
	snprintf(root,sizeof(root),"/tmp/nxhalo-texture-pack-%ld",(long)getpid());
	snprintf(src,sizeof(src),"%s/source",root);snprintf(sub,sizeof(sub),"%s/ui",src);
	snprintf(file,sizeof(file),"%s/shell",sub);
	make_dir(root);make_dir(src);make_dir(sub);make_dir(file);
	snprintf(file,sizeof(file),"%s/ui/shell/synthetic texture__0.tga",src);
	tga[2]=2;tga[12]=2;tga[14]=2;tga[16]=32;tga[17]=0x20;
	for(int i=18;i<34;i++)tga[i]=(unsigned char)i;
	ok &= require(write_bytes(file,tga,sizeof(tga)),"write synthetic TGA fixture");
	snprintf(badsrc,sizeof(badsrc),"%s/bad-source",root);snprintf(badsub,sizeof(badsub),"%s/ui",badsrc);make_dir(badsrc);make_dir(badsub);
	snprintf(badfile,sizeof(badfile),"%s/ui/shell/synthetic texture__0.tga",badsrc);snprintf(file,sizeof(file),"%s/shell",badsub);make_dir(file);
	snprintf(badfile,sizeof(badfile),"%s/ui/shell/synthetic texture__0.tga",badsrc);write_bytes(badfile,bad,sizeof(bad));
	snprintf(symsrc,sizeof(symsrc),"%s/sym-source",root);snprintf(symfile,sizeof(symfile),"%s/synthetic__0.tga",symsrc);snprintf(linkpath,sizeof(linkpath),"%s/source/ui/shell/synthetic texture__0.tga",root);make_dir(symsrc);symlink(linkpath,symfile);
	snprintf(file,sizeof(file),"%s/cfg",root);make_dir(file);
	ok &= require(texture_pack_install("fixture",src),"import synthetic pack");
	ok &= require(texture_pack_list(list_found,&listed)>=1 && listed,"list installed fixture pack");
	ok &= require(texture_pack_select("fixture"),"select fixture pack");
	ok &= require(!texture_pack_enabled(),"packs default disabled");
	ok &= require(texture_pack_override("ui\\shell\\synthetic texture",0,&levels)==0,"disabled pack falls back");
	ok &= require(texture_pack_set_enabled(1),"enable fixture pack");
	tex=texture_pack_override("ui\\shell\\synthetic texture",0,&levels);
	ok &= require(tex!=0 && upload_2d==1 && seen_w==2 && seen_h==2 && mipmaps==1 && levels==2,"TGA replacement reaches GL with generated mip chain");
	/* A broad pack keeps more than the former eight-entry limit resident. */
	{
		char wide[1024], wide_ui[1100], wide_shell[1200], wide_file[1400], tag[64];
		unsigned int first=0; int uploads;
		snprintf(wide,sizeof(wide),"%s/wide-source",root);
		snprintf(wide_ui,sizeof(wide_ui),"%s/ui",wide);snprintf(wide_shell,sizeof(wide_shell),"%s/shell",wide_ui);
		make_dir(wide);make_dir(wide_ui);make_dir(wide_shell);
		for(int i=0;i<12;i++){
			snprintf(wide_file,sizeof(wide_file),"%s/cache%02d__0.tga",wide_shell,i);
			ok &= require(write_bytes(wide_file,tga,sizeof(tga)),"write multi-entry cache fixture");
		}
		ok &= require(texture_pack_install("wide",wide)&&texture_pack_select("wide"),"install/select multi-entry pack");
		for(int i=0;i<12;i++){
			snprintf(tag,sizeof(tag),"ui\\shell\\cache%02d",i);
			tex=texture_pack_override(tag,0,&levels);
			ok &= require(tex!=0,"load pack entry into GPU cache");
			if(i==0)first=tex;
		}
		uploads=upload_2d;
		tex=texture_pack_override("ui\\shell\\cache00",0,&levels);
		ok &= require(tex==first&&upload_2d==uploads,"64-entry cache retains early broad-pack texture");
	}
	ok &= require(texture_pack_override("../outside",0,&levels)==0,"tag traversal rejected");
	snprintf(file,sizeof(file),"%s/malformed",root);make_dir(file);
	snprintf(sub,sizeof(sub),"%s/ui",file);make_dir(sub);snprintf(badfile,sizeof(badfile),"%s/shell",sub);make_dir(badfile);
	snprintf(badfile,sizeof(badfile),"%s/ui/shell/synthetic texture__0.tga",file);write_bytes(badfile,bad,sizeof(bad));
	ok &= require(texture_pack_install("malformed",file),"import malformed fixture for fallback check");
	ok &= require(texture_pack_select("malformed"),"select malformed fixture");
	ok &= require(texture_pack_override("ui\\shell\\synthetic texture",0,&levels)==0,"malformed image falls back");
	ok &= require(!texture_pack_install("symlink",symsrc),"symlink import rejected");
	/* A valid RGBA PNG header dispatches to the existing port PNG decoder. */
	snprintf(file,sizeof(file),"%s/png",root);make_dir(file);snprintf(sub,sizeof(sub),"%s/ui",file);make_dir(sub);snprintf(badfile,sizeof(badfile),"%s/shell",sub);make_dir(badfile);
	snprintf(badfile,sizeof(badfile),"%s/ui/shell/synthetic texture__0.png",file);
	memcpy(png,"\x89PNG\r\n\x1a\n",8);memcpy(png+12,"IHDR",4);png[19]=2;png[23]=2;write_bytes(badfile,png,sizeof(png));
	ok &= require(texture_pack_install("png",file)&&texture_pack_select("png"),"install/select PNG fixture");
	tex=texture_pack_override("ui\\shell\\synthetic texture",0,&levels);
	ok &= require(tex==77&&png_decodes==1&&levels==1,"PNG replacement dispatches to RGBA decoder");
	/* BC1 DDS with one 4x4 block produces one compressed upload. */
	snprintf(file,sizeof(file),"%s/dds",root);make_dir(file);snprintf(sub,sizeof(sub),"%s/ui",file);make_dir(sub);snprintf(badfile,sizeof(badfile),"%s/shell",sub);make_dir(badfile);
	snprintf(badfile,sizeof(badfile),"%s/ui/shell/synthetic texture__0.dds",file);
	memcpy(dds,"DDS ",4);dds[4]=124;dds[76]=32;dds[80]=4;dds[84]='D';dds[85]='X';dds[86]='T';dds[87]='1';dds[12]=4;dds[16]=4;dds[28]=2;dds[128]=1;dds[136]=2;
	write_bytes(badfile,dds,sizeof(dds));
	ok &= require(texture_pack_install("dds",file)&&texture_pack_select("dds"),"install/select DDS fixture");
	tex=texture_pack_override("ui\\shell\\synthetic texture",0,&levels);
	ok &= require(tex!=0&&upload_compressed==2&&seen_format==0x83f1&&levels==2,"BC1 DDS reaches compressed GL upload");
	/* Import limits are enforced before a very large pack is accepted. */
	snprintf(file,sizeof(file),"%s/over-cap",root);make_dir(file);
	for(int i=0;i<513;i++){
		char entry[1200];unsigned char byte=0;
		snprintf(entry,sizeof(entry),"%s/item%03d.tga",file,i);
		if(!write_bytes(entry,&byte,1)){ok &= require(0,"write cap fixture");break;}
	}
	ok &= require(!texture_pack_install("over-cap",file),"512-file import cap enforced");

    /* Rejected imports never publish a partial directory; retry stays possible. */
    snprintf(file,sizeof(file),"%s/cfg/texture-packs/over-cap",root);
    ok &= require(access(file,F_OK)!=0,"over-cap rollback leaves no installed directory");
    snprintf(file,sizeof(file),"%s/cfg/texture-packs/symlink",root);
    ok &= require(access(file,F_OK)!=0,"symlink rollback leaves no installed directory");
    ok &= require(!texture_pack_install("fixture",src),"existing immutable pack is never replaced");
    ok &= require(texture_pack_install("symlink",src),"failed install can be retried with valid content");
    ok &= require(!texture_pack_install(".hidden",src),"hidden names reserved for private staging");
    snprintf(file,sizeof(file),"%s/empty",root);make_dir(file);
    ok &= require(!texture_pack_install("empty",file),"empty pack rejected");
    snprintf(file,sizeof(file),"%s/cfg/texture-packs/empty",root);
    ok &= require(access(file,F_OK)!=0,"empty rollback leaves no installed directory");
    /* A four-byte DDS magic must never cause a header read past its allocation. */
    snprintf(file,sizeof(file),"%s/shortdds",root);make_dir(file);
    snprintf(badfile,sizeof(badfile),"%s/short__0.dds",file);write_bytes(badfile,(const unsigned char *)"DDS ",4);
    ok &= require(texture_pack_install("shortdds",file)&&texture_pack_select("shortdds"),"install truncated DDS fallback fixture");
    ok &= require(texture_pack_override("short",0,&levels)==0,"truncated DDS safely falls back");
    /* A removed/reimported name must not keep old path-keyed GPU contents. */
    ok &= require(texture_pack_select("fixture"),"select original cache fixture");
    tex=texture_pack_override("ui/shell/synthetic texture",0,&levels);
    {
        unsigned int old_texture=tex;
        snprintf(file,sizeof(file),"%s/cfg/texture-packs/fixture",root);
        snprintf(sub,sizeof(sub),"%s/cfg/retired-fixture",root);
        ok &= require(rename(file,sub)==0,"simulate recoverable removal of old pack");
        ok &= require(texture_pack_install("fixture",src),"reimport same name");
        tex=texture_pack_override("ui/shell/synthetic texture",0,&levels);
        ok &= require(tex!=0&&tex!=old_texture,"same-name reimport invalidates old GL cache on render thread");
    }
    puts(ok?"PASS: native texture runtime and atomic installation":"FAIL");
	return ok?0:1;
}

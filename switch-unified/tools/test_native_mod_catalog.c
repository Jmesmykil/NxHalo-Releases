/* Standalone adversarial fixture. Compile this/module/parser with game ABI,
   posix_files.c with host ABI, and prefixed inflater with host ABI. */
#include "native_mod_catalog.h"
#include "posix.h"
#include "../port/third_party/zlib/zlib_prefixed.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
void p2p_sha256(const void*,int,unsigned char*);
static int checks;
static char config_root[1024];
void config_folder(char *out,size_t cap){snprintf(out,cap,"%s/",config_root);}
static void require(int ok,const char *why){checks++;if(!ok){fprintf(stderr,"FAIL %s\n",why);exit(1);}}
static void w16(unsigned char *p,unsigned int n){p[0]=n;p[1]=n>>8;}
static void w32(unsigned char *p,unsigned int n){w16(p,n);w16(p+2,n>>16);}
static unsigned int r16(unsigned char*p){return p[0]|p[1]<<8;}
static unsigned int r32(unsigned char*p){return r16(p)|r16(p+2)<<16;}
static void hash(const unsigned char *p,size_t n,char out[65]){unsigned char h[32];int i;p2p_sha256(p,(int)n,h);for(i=0;i<32;i++)snprintf(out+i*2,3,"%02x",h[i]);}
static unsigned char *fixture(const char *image,size_t *size){
 const char *name[3]={image,"CREDITS.txt","LICENSE-NOTICE.txt"};
 unsigned char tga[34]={0};const unsigned char *data[3]={tga,(const unsigned char*)"Author: fixture\n",(const unsigned char*)"License: fixture-only\n"};
 unsigned int len[3]={34,16,22},off[3],cd,k=0,i;unsigned char *p=calloc(1,4096);
 tga[2]=2;tga[12]=2;tga[14]=2;tga[16]=32;tga[17]=0x20;
 for(i=0;i<3;i++){unsigned int n=strlen(name[i]);off[i]=k;w32(p+k,0x04034b50);w16(p+k+4,20);w32(p+k+14,crc32(0,data[i],len[i]));w32(p+k+18,len[i]);w32(p+k+22,len[i]);w16(p+k+26,n);memcpy(p+k+30,name[i],n);memcpy(p+k+30+n,data[i],len[i]);k+=30+n+len[i];}
 cd=k;for(i=0;i<3;i++){unsigned int n=strlen(name[i]);w32(p+k,0x02014b50);w16(p+k+4,0x0314);w16(p+k+6,20);w32(p+k+16,crc32(0,data[i],len[i]));w32(p+k+20,len[i]);w32(p+k+24,len[i]);w16(p+k+28,n);w32(p+k+38,0100600U<<16);w32(p+k+42,off[i]);memcpy(p+k+46,name[i],n);k+=46+n;}
 w32(p+k,0x06054b50);w16(p+k+8,3);w16(p+k+10,3);w32(p+k+12,k-cd);w32(p+k+16,cd);*size=k+22;return p;
}
static int extract(unsigned char *zip,size_t n,struct native_mod_catalog_entry *v,const char *base,int expected,const char *why){
 char stage[1024],err[192]="";int ok;snprintf(stage,sizeof(stage),"%s/case-%d",base,checks);require(!posix_make_directory(stage),"create private test stage");
 hash(zip,n,v->sha256);ok=native_mod_catalog_test_extract(zip,n,v,stage,err,sizeof(err));if(ok!=expected)fprintf(stderr,"extract status: %s\n",err);require(ok==expected,why);return ok;
}
static void existing_receipt_damage(const struct native_mod_catalog_entry *v,const char *stage,const char *path,int mode){
 unsigned char original[65536];char err[192];int fd=open(path,O_RDONLY|O_NOFOLLOW),n,ok=0;
 require(fd>=0,"open owned original receipt");n=(int)read(fd,original,sizeof(original));close(fd);require(n>0,"save owned original receipt bytes");
 require(!unlink(path),"remove only owned receipt fixture");
 if(mode==0){/* missing */}
 else if(mode==1){fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);require(fd>=0,"empty owned receipt");close(fd);}
 else if(mode==2){require(!symlink("/dev/null",path),"symlink owned receipt");}
 else if(mode==3){fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);require(fd>=0&&write(fd,"changed",7)==7,"changed owned receipt bytes");if(fd>=0)close(fd);}
 else if(mode==4){unsigned char zeros[4096]={0};fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);require(fd>=0,"oversized owned receipt");for(int i=0;i<17;i++)require(write(fd,zeros,sizeof(zeros))==sizeof(zeros),"bounded oversized metadata fixture");close(fd);}
 else {require(!posix_make_directory(path),"directory replacing owned receipt");}
 ok=native_mod_catalog_test_receipt(v,stage,err,sizeof(err));require(!ok,"damaged existing same-digest receipt fails closed");
 if(mode==5)require(!rmdir(path),"remove owned metadata directory");else if(mode!=0)require(!unlink(path),"remove owned damaged metadata");
 fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);require(fd>=0&&write(fd,original,n)==n,"restore exact original owned receipt");if(fd>=0)close(fd);
 require(native_mod_catalog_test_receipt(v,stage,err,sizeof(err)),"restored complete attribution receipt reusable");
}
static void consent_checks(const struct native_mod_catalog_entry *v){
 const size_t fields[]={offsetof(struct native_mod_catalog_entry,id),offsetof(struct native_mod_catalog_entry,version),
 offsetof(struct native_mod_catalog_entry,pack_name),offsetof(struct native_mod_catalog_entry,title),offsetof(struct native_mod_catalog_entry,author),
 offsetof(struct native_mod_catalog_entry,license),offsetof(struct native_mod_catalog_entry,source),offsetof(struct native_mod_catalog_entry,archive),
 offsetof(struct native_mod_catalog_entry,sha256),offsetof(struct native_mod_catalog_entry,native_format),offsetof(struct native_mod_catalog_entry,map_scope),
 offsetof(struct native_mod_catalog_entry,compressed_limit),offsetof(struct native_mod_catalog_entry,unpacked_limit),offsetof(struct native_mod_catalog_entry,file_count)};
 struct native_mod_catalog_entry changed=*v;unsigned int i;
 require(native_mod_catalog_test_entry_equal(v,&changed),"identical detail snapshot accepted");
 changed.installed=!v->installed;require(native_mod_catalog_test_entry_equal(v,&changed),"mutable installed flag excluded from consent");
 for(i=0;i<sizeof(fields)/sizeof(fields[0]);i++){changed=*v;((unsigned char*)&changed)[fields[i]]^=1;
  require(!native_mod_catalog_test_entry_equal(v,&changed),"each changed displayed/security field invalidates consent");}
}
static const char *valid =
"schema_version = 1\n[[mods]]\nid = 'fixture'\nversion = '1'\ntitle = 'Fixture'\nauthor = 'Fixture Author'\nlicense = 'CC0-1.0'\n"
"source = 'https://example.invalid/source'\narchive = 'https://example.invalid/fixture.zip'\n"
"sha256 = '0000000000000000000000000000000000000000000000000000000000000000'\n"
"compressed_limit = 4096\nunpacked_limit = 65536\nfile_count = 1\nnative_format = 'tag-ordinal-images-v1'\nmap_scope = 'Only fixture tag'\n";
static void bad_text(const char *from,const char *to,struct native_mod_catalog_entry *v){
 char text[4096],err[192];const char *p=strstr(valid,from);int n;
 require(p!=NULL,"manifest mutation target");snprintf(text,sizeof(text),"%.*s%s%s",(int)(p-valid),valid,to,p+strlen(from));
 require(!native_mod_catalog_test_parse(text,strlen(text),v,&n,err,sizeof(err)),"reject malformed/unsupported manifest");
}
int main(int argc,char **argv){
 struct native_mod_catalog_entry *v=calloc(NATIVE_MOD_CATALOG_MAX_ENTRIES,sizeof(*v));char base[]="/tmp/nxhalo-native-mod-XXXXXX",error[192],path[1200];unsigned char *zip;size_t n;unsigned int cd;int count,fd;unsigned char tga[34];
 require(mkdtemp(base)!=NULL,"private fixture root");snprintf(config_root,sizeof(config_root),"%s/config",base);require(!posix_make_directory(config_root),"private config root");
 require(native_mod_catalog_test_parse(valid,strlen(valid),v,&count,error,sizeof(error))&&count==1,"parse native manifest");
 require(!strcmp(v[0].pack_name,"fixture-1"),"immutable id/version identity");consent_checks(v);
 bad_text("https://example.invalid/fixture.zip","http://example.invalid/fixture.zip",v);
 bad_text("file_count = 1","file_count = 513",v);
 bad_text("unpacked_limit = 65536","unpacked_limit = 268435457",v);
 bad_text("tag-ordinal-images-v1","dll-native-v1",v);
 bad_text("id = 'fixture'","id = '../escape'",v);
 bad_text("id = 'fixture'","id = '.hidden'",v);
 bad_text("license = 'CC0-1.0'","license = ''",v);
 bad_text("version = '1'","version = 1",v);
 require(!native_mod_catalog_test_parse("schema_version = 1\n[[mods]]",26,v,&count,error,sizeof(error)),"reject missing required fields");
 require(native_mod_catalog_test_parse("schema_version = 1\nmods = []\n",strlen("schema_version = 1\nmods = []\n"),v,&count,error,sizeof(error))&&count==0,"truthful empty catalogue");
 require(native_mod_catalog_test_parse(valid,strlen(valid),v,&count,error,sizeof(error)),"restore valid manifest");
 zip=fixture("payload/ui/shell/fixture__0.tga",&n);extract(zip,n,v,base,1,"valid multi-file archive");
 snprintf(path,sizeof(path),"%s/case-%d/payload/ui/shell/fixture__0.tga",base,checks-2);fd=open(path,O_RDONLY);
 require(fd>=0&&read(fd,tga,sizeof(tga))==sizeof(tga)&&tga[2]==2,"actual native payload bytes");if(fd>=0)close(fd);
 cd=r32(zip+n-6);
 w16(zip+6,1);w16(zip+cd+8,1);extract(zip,n,v,base,0,"reject encryption");w16(zip+6,0);w16(zip+cd+8,0);
 w16(zip+6,8);w16(zip+cd+8,8);extract(zip,n,v,base,0,"reject descriptors");w16(zip+6,0);w16(zip+cd+8,0);
 w32(zip+cd+38,0120777U<<16);extract(zip,n,v,base,0,"reject symlinks");w32(zip+cd+38,0100600U<<16);
 w32(zip+cd+24,0xffffffffU);extract(zip,n,v,base,0,"reject ZIP64");w32(zip+cd+24,34);
 zip[30+strlen("payload/ui/shell/fixture__0.tga")]=7;extract(zip,n,v,base,0,"reject CRC corruption");zip[30+strlen("payload/ui/shell/fixture__0.tga")]=0;
 w32(zip+14,123);extract(zip,n,v,base,0,"reject local/central mismatch");w32(zip+14,r32(zip+cd+16));
 v->file_count=2;extract(zip,n,v,base,0,"reject declared file count mismatch");v->file_count=1;
 v->unpacked_limit=35;extract(zip,n,v,base,0,"reject expanded size limit");v->unpacked_limit=65536;
 hash(zip,n,v->sha256);v->sha256[0]=v->sha256[0]=='0'?'1':'0';snprintf(path,sizeof(path),"%s/hash",base);posix_make_directory(path);
 require(!native_mod_catalog_test_extract(zip,n,v,path,error,sizeof(error)),"reject SHA mismatch");free(zip);
 zip=fixture("payload/ui/fixture__0.TGA",&n);extract(zip,n,v,base,0,"reject extension unsupported by importer");free(zip);
 zip=fixture("CREDITS.txt",&n);extract(zip,n,v,base,0,"reject exact duplicate names");free(zip);
 zip=fixture("payload/../fixture__0.tga",&n);extract(zip,n,v,base,0,"reject traversal");free(zip);
 zip=fixture("/absolute__0.tga",&n);extract(zip,n,v,base,0,"reject absolute");free(zip);
 zip=fixture("payload/ui/fixture__0.dll",&n);extract(zip,n,v,base,0,"reject executable payload");free(zip);
 zip=fixture("payload/ui/fixture__0.tga",&n);snprintf(path,sizeof(path),"%s/symlink-stage",base);posix_make_directory(path);
 {char link[1200];snprintf(link,sizeof(link),"%s/payload",path);require(!symlink("/tmp",link),"make adversarial stage symlink");}
 hash(zip,n,v->sha256);require(!native_mod_catalog_test_extract(zip,n,v,path,error,sizeof(error)),"no-follow stage rejects symlink escape");free(zip);
 if(argc==2){FILE *f=fopen(argv[1],"rb");require(f!=NULL,"open actual authored Alpine package");fseek(f,0,SEEK_END);n=ftell(f);rewind(f);zip=malloc(n);require(zip&&fread(zip,1,n,f)==n,"read bounded actual package");fclose(f);
  v->compressed_limit=261326;v->unpacked_limit=263221;v->file_count=1;
  hash(zip,n,v->sha256);require(!strcmp(v->sha256,"b7b6697105d01a33d2fa2005f147c58d116d5c217d7e479f0eea95651a2e1710"),"pinned final authored package SHA256");
  strcpy(v->id,"alpine-cliff-detail");strcpy(v->version,"1-e3daf57a1d6b");strcpy(v->pack_name,"alpine-cliff-detail-1-e3daf57a1d6b");
  strcpy(v->title,"Alpine authored cliff detail (one texture)");strcpy(v->author,"csauve");strcpy(v->license,"CC-BY-NC-4.0");strcpy(v->map_scope,"Requires Alpine cliff-detail bitmap tag");
  {int stage_index=checks;char stage[1200],receipt_file[1400];
   extract(zip,n,v,base,1,"actual permission-bearing Alpine PNG package");
   snprintf(stage,sizeof(stage),"%s/case-%d",base,stage_index);
   require(native_mod_catalog_test_receipt(v,stage,error,sizeof(error)),"preserve actual author/license/source receipt");
   snprintf(receipt_file,sizeof(receipt_file),"%s/native-mod-receipts/%s/CREDITS.txt",config_root,v->pack_name);
   fd=open(receipt_file,O_RDONLY|O_NOFOLLOW);require(fd>=0,"actual attribution copied");if(fd>=0)close(fd);
   require(native_mod_catalog_test_receipt(v,stage,error,sizeof(error)),"reuse identical immutable receipt");
   {const char *required[]={"CREDITS.txt","LICENSE-NOTICE.txt","RECEIPT.txt"};
    for(int i=0;i<3;i++)for(int mode=0;mode<6;mode++){
     snprintf(receipt_file,sizeof(receipt_file),"%s/native-mod-receipts/%s/%s",config_root,v->pack_name,required[i]);
     existing_receipt_damage(v,stage,receipt_file,mode);
    }
    {struct native_mod_catalog_entry changed=*v;strcpy(changed.author,"Changed Attribution");
     require(!native_mod_catalog_test_receipt(&changed,stage,error,sizeof(error)),"same archive digest with changed metadata requires new receipt consent");
    }
   }
   snprintf(receipt_file,sizeof(receipt_file),"%s/LICENSE-NOTICE.txt",stage);require(!unlink(receipt_file),"remove only scratch license fixture");
   strcpy(v->pack_name,"missing-license-1");
   require(!native_mod_catalog_test_receipt(v,stage,error,sizeof(error)),"missing mandatory license never reports success");
   snprintf(receipt_file,sizeof(receipt_file),"%s/native-mod-receipts/%s",config_root,v->pack_name);
   fd=open(receipt_file,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);require(fd<0,"failed receipt not published");if(fd>=0)close(fd);
   require(!symlink("/tmp",receipt_file),"make private malicious receipt symlink");
   require(!native_mod_catalog_test_receipt(v,stage,error,sizeof(error)),"nofollow reused receipt identity");
  }free(zip);}
 printf("PASS %d native catalogue/archive checks; private fixture %s\n",checks,base);free(v);return 0;
}

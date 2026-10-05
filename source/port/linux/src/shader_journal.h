/* NxHalo source/program journal. GLES objects never cross contexts.
 * Bounded, versioned text is recompiled on the current driver. */
#ifndef NX_SHADER_JOURNAL_H
#define NX_SHADER_JOURNAL_H
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define NX_JOURNAL_PAIRS 512
#define NX_JOURNAL_SOURCE 65536
#define NX_JOURNAL_BYTES (8*1024*1024)
struct nx_shader_pair { char *vertex, *fragment; };
struct nx_shader_journal { struct nx_shader_pair pairs[NX_JOURNAL_PAIRS]; unsigned count; size_t bytes; int dirty; };
static void nx_journal_clear(struct nx_shader_journal *j) {
 unsigned i; for(i=0;i<j->count;i++){free(j->pairs[i].vertex);free(j->pairs[i].fragment);} memset(j,0,sizeof(*j));
}
static int nx_journal_add(struct nx_shader_journal *j,const char *v,const char *f) {
 size_t vl=strlen(v)+1,fl=strlen(f)+1; unsigned i; char *vc,*fc;
 for(i=0;i<j->count;i++) if(!strcmp(v,j->pairs[i].vertex)&&!strcmp(f,j->pairs[i].fragment))return 1;
 if(j->count>=NX_JOURNAL_PAIRS||vl>NX_JOURNAL_SOURCE||fl>NX_JOURNAL_SOURCE||vl+fl>NX_JOURNAL_BYTES-j->bytes)return 0;
 vc=malloc(vl);fc=malloc(fl);if(!vc||!fc){free(vc);free(fc);return 0;}
 memcpy(vc,v,vl);memcpy(fc,f,fl);j->pairs[j->count].vertex=vc;j->pairs[j->count++].fragment=fc;j->bytes+=vl+fl;j->dirty=1;return 1;
}
/* A worker owns the clone while it writes, never the live render journal. */
static int nx_journal_clone(struct nx_shader_journal *dst, const struct nx_shader_journal *src) {
 unsigned i; struct nx_shader_journal tmp={0};
 for(i=0;i<src->count;i++) if(!nx_journal_add(&tmp,src->pairs[i].vertex,src->pairs[i].fragment)) {
  nx_journal_clear(&tmp); return 0;
 }
 nx_journal_clear(dst); *dst=tmp; dst->dirty=src->dirty; return 1;
}
static int nx_journal_u32(FILE *f,uint32_t *v,int writing) {
 unsigned char b[4]; if(writing){ b[0]=*v;b[1]=*v>>8;b[2]=*v>>16;b[3]=*v>>24;return fwrite(b,1,4,f)==4; }
 if(fread(b,1,4,f)!=4)return 0; *v=(uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;return 1;
}
static int nx_journal_load(struct nx_shader_journal *j,const char *path) {
 FILE *file=fopen(path,"rb"); struct nx_shader_journal tmp={0}; unsigned char magic[8];uint32_t n,i;int ok=0;
 if(!file)return 0;
 if(fread(magic,1,8,file)!=8||memcmp(magic,"NXGL15A\0",8)||!nx_journal_u32(file,&n,0)||n>NX_JOURNAL_PAIRS)goto done;
 for(i=0;i<n;i++) {
  uint32_t vl,fl;char *v=NULL,*f=NULL;
  if(!nx_journal_u32(file,&vl,0)||!nx_journal_u32(file,&fl,0)||!vl||!fl||vl>=NX_JOURNAL_SOURCE||fl>=NX_JOURNAL_SOURCE||(size_t)vl+fl+2>NX_JOURNAL_BYTES-tmp.bytes)goto done;
  v=malloc(vl+1);f=malloc(fl+1);
  if(!v||!f){free(v);free(f);goto done;}
  if(fread(v,1,vl,file)!=vl||fread(f,1,fl,file)!=fl||memchr(v,0,vl)||memchr(f,0,fl)){free(v);free(f);goto done;}
  v[vl]=0;f[fl]=0;
  if(!nx_journal_add(&tmp,v,f)){free(v);free(f);goto done;}free(v);free(f);
 }
 if(fgetc(file)!=EOF||ferror(file))goto done;
 nx_journal_clear(j);*j=tmp;memset(&tmp,0,sizeof(tmp));j->dirty=0;ok=1;
 done: fclose(file);nx_journal_clear(&tmp);return ok;
}
static int nx_journal_save(struct nx_shader_journal *j,const char *path) {
 char tmp[1024];FILE *file;uint32_t n=j->count,i;int ok=1;
 if(!j->dirty)return 1;
 if(snprintf(tmp,sizeof(tmp),"%s.tmp",path)>=(int)sizeof(tmp))return 0;
 file=fopen(tmp,"wb");if(!file)return 0;
 ok=fwrite("NXGL15A\0",1,8,file)==8&&nx_journal_u32(file,&n,1);
 for(i=0;ok&&i<n;i++) {uint32_t vl=strlen(j->pairs[i].vertex),fl=strlen(j->pairs[i].fragment);
 ok=nx_journal_u32(file,&vl,1)&&nx_journal_u32(file,&fl,1)&&fwrite(j->pairs[i].vertex,1,vl,file)==vl&&fwrite(j->pairs[i].fragment,1,fl,file)==fl;}
 if(fclose(file))ok=0;
 if(ok&&rename(tmp,path)==0){j->dirty=0;return 1;}
 /* Horizon fsdev rename does not replace an existing destination. Keep the
  * previous complete journal as a backup until the new journal is installed.
  * The loader recovers that backup if termination interrupts this sequence. */
 if(ok) {
  char backup[1024];FILE *previous=fopen(path,"rb");
  if(previous) {
   fclose(previous);
   if(snprintf(backup,sizeof(backup),"%s.bak",path)>=(int)sizeof(backup)){remove(tmp);return 0;}
   if(remove(backup)!=0 && errno!=ENOENT){remove(tmp);return 0;}
   if(rename(path,backup)==0) {
    if(rename(tmp,path)==0){remove(backup);j->dirty=0;return 1;}
    rename(backup,path); /* If restoration fails, next startup reads backup. */
   }
  }
 }
 remove(tmp);return 0;
}
#endif

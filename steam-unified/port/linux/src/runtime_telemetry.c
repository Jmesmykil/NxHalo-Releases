/* Host libc/system observations; game-source stdio uses Xbox path adapters. */
#include "platform.h"
#include "runtime_telemetry.h"
#include <stdio.h>
#include <time.h>
#ifdef __linux__
#include <elf.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

double halo_telemetry_time_ms(void)
{
    LARGE_INTEGER count, frequency;
    if (!halo_telemetry_enabled()) return 0.0;
    QueryPerformanceCounter(&count);
    QueryPerformanceFrequency(&frequency);
    return frequency.QuadPart > 0 ? (double)count.QuadPart * 1000.0 / (double)frequency.QuadPart : 0.0;
}

static void telemetry_identity(void)
{
#ifdef __linux__
    FILE *f=fopen("/proc/self/exe","rb");
    Elf32_Ehdr eh;
    char build_id[129]="unavailable";
    unsigned int i;
    if(f && fread(&eh,1,sizeof(eh),f)==sizeof(eh) && !memcmp(eh.e_ident,ELFMAG,SELFMAG) &&
       eh.e_ident[EI_CLASS]==ELFCLASS32 && eh.e_phentsize==sizeof(Elf32_Phdr) && eh.e_phnum<128) {
        for(i=0;i<eh.e_phnum;i++) {
            Elf32_Phdr ph;
            unsigned char notes[4096];unsigned int pos=0;
            if(fseek(f,(long)eh.e_phoff+(long)i*sizeof(ph),SEEK_SET) || fread(&ph,1,sizeof(ph),f)!=sizeof(ph))break;
            if(ph.p_type!=PT_NOTE || ph.p_filesz>sizeof(notes))continue;
            if(fseek(f,(long)ph.p_offset,SEEK_SET) || fread(notes,1,ph.p_filesz,f)!=ph.p_filesz)continue;
            while(pos+sizeof(Elf32_Nhdr)<=ph.p_filesz) {
                Elf32_Nhdr nh; unsigned int name_size,data_size,j;unsigned char *name,*data;
                memcpy(&nh,notes+pos,sizeof(nh));pos+=sizeof(nh);
                if(nh.n_namesz>sizeof(notes) || nh.n_descsz>sizeof(notes))break;
                name_size=(nh.n_namesz+3)&~3u;data_size=(nh.n_descsz+3)&~3u;
                if(name_size+data_size>ph.p_filesz-pos)break;
                name=notes+pos;data=name+name_size;
                if(nh.n_type==NT_GNU_BUILD_ID && nh.n_namesz>=3 && !memcmp(name,"GNU",3) && nh.n_descsz>0 && nh.n_descsz<=64) {
                    for(j=0;j<nh.n_descsz;j++)snprintf(build_id+2*j,3,"%02x",data[j]);
                    break;
                }
                pos+=name_size+data_size;
            }
            if(strcmp(build_id,"unavailable"))break;
        }
    }
    if(f)fclose(f);
    platform_log("runtime_identity: unix=%ld pid=%ld elf_build_id=%s",(long)time(NULL),(long)getpid(),build_id);
#endif
}

static int read_integer(char const *path, long *value)
{
    FILE *f=fopen(path,"r"); int ok;
    if (!f) return 0;
    ok=fscanf(f,"%ld",value)==1;
    fclose(f); return ok;
}

void halo_telemetry_report_resources(double seconds)
{
#ifdef __linux__
    static struct rusage last_usage;
    struct rusage usage;
    if (seconds <= 0) { telemetry_identity(); getrusage(RUSAGE_SELF,&last_usage); return; }
    long resident=0, pages=0, gpu=-1, candidate=0;
    int card;
    char path[160];
    FILE *f=fopen("/proc/self/statm","r");
    if (f) { if(fscanf(f,"%ld %ld",&pages,&resident)!=2)resident=0; fclose(f); }
    for(card=0;card<16;card++) {
        unsigned int vendor=0;
        snprintf(path,sizeof(path),"/sys/class/drm/card%d/device/vendor",card);
        f=fopen(path,"r");
        if (f) { if(fscanf(f,"%x",&vendor)!=1)vendor=0; fclose(f); }
        if (vendor!=0x1002) continue;
        snprintf(path,sizeof(path),"/sys/class/drm/card%d/device/gpu_busy_percent",card);
        if(read_integer(path,&candidate) && candidate>=0 && candidate<=100) { gpu=candidate; break; }
    }
    if(getrusage(RUSAGE_SELF,&usage)==0) {
        double cpu=(usage.ru_utime.tv_sec-last_usage.ru_utime.tv_sec)+(usage.ru_stime.tv_sec-last_usage.ru_stime.tv_sec)
            +((usage.ru_utime.tv_usec-last_usage.ru_utime.tv_usec)+(usage.ru_stime.tv_usec-last_usage.ru_stime.tv_usec))/1000000.0;
        platform_log("perf_resources: unix=%ld cpu_process_percent=%.2f rss_kib=%ld maxrss_kib=%ld gpu_device_busy_percent=%ld gpu_available=%d minor_faults=%ld major_faults=%ld voluntary_switches=%ld involuntary_switches=%ld",
            (long)time(NULL),seconds>0?100.0*cpu/seconds:0.0,resident*(sysconf(_SC_PAGESIZE)/1024),usage.ru_maxrss,gpu,gpu>=0,
            usage.ru_minflt-last_usage.ru_minflt,usage.ru_majflt-last_usage.ru_majflt,
            usage.ru_nvcsw-last_usage.ru_nvcsw,usage.ru_nivcsw-last_usage.ru_nivcsw);
        last_usage=usage;
    }
#else
    (void)seconds;
#endif
}


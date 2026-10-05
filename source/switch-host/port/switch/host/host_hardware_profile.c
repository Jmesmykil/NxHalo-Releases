#include "host.h"
#include <switch.h>
#include <stdint.h>

/* Read-only telemetry. Service failures remain visible; never change clocks.
 * Called at report boundaries, with its cost included in diagnostic timing. */
void host_hardware_profile_report(uint32_t frame)
{
    static int initialized;
    static ClkrstSession clocks[3];
    static Result clock_status[3], temperature_status;
    static TsSession temperature;
    static uint64_t used_peak;
    const PcvModuleId modules[3]={PcvModuleId_CpuBus,PcvModuleId_GPU,PcvModuleId_EMC};
    if(!initialized) {
        Result clock_init=clkrstInitialize();
        for(unsigned i=0;i<3;++i)
            clock_status[i]=R_SUCCEEDED(clock_init) ? clkrstOpenSession(&clocks[i],modules[i],3) : clock_init;
        temperature_status=tsInitialize();
        if(R_SUCCEEDED(temperature_status))
            temperature_status=tsOpenSession(&temperature,TsDeviceCode_LocationExternal);
        initialized=1;
    }
    u32 hz[3]={0}; Result rc[3];
    for(unsigned i=0;i<3;++i)
        rc[i]=R_SUCCEEDED(clock_status[i]) ? clkrstGetClockRate(&clocks[i],&hz[i]) : clock_status[i];
    float degrees=0;
    Result trc=R_SUCCEEDED(temperature_status) ? tsSessionGetTemperature(&temperature,&degrees) : temperature_status;
    u64 total=0,used=0,idle=0,thread=0;
    Result total_rc=svcGetInfo(&total,InfoType_TotalMemorySize,CUR_PROCESS_HANDLE,0);
    Result used_rc=svcGetInfo(&used,InfoType_UsedMemorySize,CUR_PROCESS_HANDLE,0);
    Result idle_rc=svcGetInfo(&idle,InfoType_IdleTickCount,INVALID_HANDLE,TickCountInfo_Total);
    Result thread_rc=svcGetInfo(&thread,InfoType_ThreadTickCount,CUR_THREAD_HANDLE,TickCountInfo_Total);
    if(R_SUCCEEDED(used_rc) && used>used_peak) used_peak=used;
    host_logf_buffered(HOST_LOG_INFO,
        "hardware frame=%u tick=%llu core=%u cpu_hz=%u cpu_rc=%x gpu_hz=%u gpu_rc=%x emc_hz=%u emc_rc=%x soc_c=%.2f soc_rc=%x clock_values_valid=%d throttle=unmeasured",
        frame,(unsigned long long)armGetSystemTick(),svcGetCurrentProcessorNumber(),
        hz[0],rc[0],hz[1],rc[1],hz[2],rc[2],degrees,trc,
        R_SUCCEEDED(rc[0]) && R_SUCCEEDED(rc[1]) && R_SUCCEEDED(rc[2]) && hz[0] && hz[1] && hz[2]);
    host_logf_buffered(HOST_LOG_INFO,
        "process_memory frame=%u total=%llu total_rc=%x used=%llu used_rc=%x observed_peak=%llu idle_current_core_ticks=%llu idle_rc=%x presentation_thread_ticks=%llu thread_rc=%x allocator_fragmentation=unmeasured",
        frame,(unsigned long long)total,total_rc,(unsigned long long)used,used_rc,
        (unsigned long long)used_peak,(unsigned long long)idle,idle_rc,(unsigned long long)thread,thread_rc);
    host_log_flush();
}

/* NxHalo adapter for OpenCE Ports Build114 deko3d. AGPL-3.0-or-later. */
#include "host.h"
#include "host_file_io.h"
#include <switch.h>
#include <pthread.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

int host_renderer_deko3d = 1;
static uint64_t helper_mask;
static unsigned helper_next;
void host_sd_lock(void) { host_file_io_lock(); }
void host_sd_unlock(void) { host_file_io_unlock(); }
const char *host_data_root(void) { return "sdmc:/switch/halo"; }
static void seed_shader_keys(void)
{
    const char *dir = "sdmc:/switch/halo/save-community24/z/shader_keys";
    const char *dst = "sdmc:/switch/halo/save-community24/z/shader_keys/keys.dkk";
    const char *tmp = "sdmc:/switch/halo/save-community24/z/shader_keys/keys.dkk.tmp";
    struct stat st;
    if (stat(dst, &st) == 0) return; /* Preserve an existing shared corpus. */
    FILE *input = fopen("romfs:/shader_keys.dkk", "rb");
    if (!input) { host_logf(HOST_LOG_WARN, "deko: no bundled shared keys; learning shaders during play"); return; }
    uint32_t header[3];
    if (fread(header, sizeof(header), 1, input) != 1 || header[0] != 0x314b4b44u || header[1] != 1 || header[2] != 264) {
        fclose(input); host_logf(HOST_LOG_ERROR, "deko: incompatible bundled shader key format"); return;
    }
    rewind(input);
    mkdir("sdmc:/switch/halo/save-community24/z", 0775);
    mkdir(dir, 0775);
    FILE *output = fopen(tmp, "wb");
    if (!output) { fclose(input); host_logf(HOST_LOG_ERROR, "deko: cannot stage shared keys: %d", errno); return; }
    unsigned char bytes[32768]; size_t count, total = 0; int ok = 1;
    while ((count = fread(bytes, 1, sizeof(bytes), input)) != 0) {
        total += count;
        if (total > 8u * 1024u * 1024u || fwrite(bytes, 1, count, output) != count) { ok = 0; break; }
    }
    if (ferror(input)) ok = 0;
    fclose(input);
    if (fclose(output) != 0) ok = 0;
    if (ok && rename(tmp, dst) == 0) host_logf(HOST_LOG_INFO, "deko: seeded %zu bytes of shared shader keys", total);
    else { remove(tmp); host_logf(HOST_LOG_ERROR, "deko: shared keys not installed; shaders will be learned"); }
}

void host_deko_prepare(void)
{
    seed_shader_keys();
    u64 allowed = 0;
    Result result = svcGetInfo(&allowed, InfoType_CoreMask, CUR_PROCESS_HANDLE, 0);
    if (R_FAILED(result) || !allowed) {
        host_logf(HOST_LOG_WARN, "deko: cannot read allowed CPU mask: %08x", result);
        return;
    }
    unsigned core = svcGetCurrentProcessorNumber();
    if (!(allowed & (1ULL << core))) core = __builtin_ctzll(allowed);
    result = svcSetThreadCoreMask(CUR_THREAD_HANDLE, core, 1ULL << core);
    if (R_FAILED(result)) {
        host_logf(HOST_LOG_WARN, "deko: cannot pin game thread: %08x", result);
        return;
    }
    helper_mask = allowed & ~(1ULL << core);
    host_dk_profile = 1;
    host_dk_hud_log = 0;
    /* No direct window registration: NxHalo may unmap/reuse guest pages.
     * host_dk uses fence-protected upload copies until permanent mappings
     * have independent hardware validation. */
    host_logf(HOST_LOG_INFO, "deko: game core %u, helpers %llx; upload-copy memory path", core, (unsigned long long)helper_mask);
}
void host_thread_place_on_helper_core(void)
{
    uint64_t mask = helper_mask;
    if (!mask) return;
    unsigned pick = __atomic_fetch_add(&helper_next, 1, __ATOMIC_RELAXED) % __builtin_popcountll(mask);
    unsigned core = 0;
    for (; core < 64; ++core) if (mask & (1ULL << core)) { if (!pick--) break; }
    Result result = svcSetThreadCoreMask(CUR_THREAD_HANDLE, core, 1ULL << core);
    if (R_FAILED(result)) host_logf(HOST_LOG_WARN, "deko: helper affinity failed: %08x", result);
}
void host_debug_thread_backtrace(uint32_t handle, const char *who)
{
    /* Never fabricate a successful sample or suspend an unknown worker. */
    host_logf(HOST_LOG_ERROR, "%s: thread %08x backtrace unavailable in NxHalo host", who, handle);
}

static u32 saved_cpu_hz;
static int restore_registered;
static void restore_cpu_clock(void)
{
    if (!saved_cpu_hz || R_FAILED(clkrstInitialize())) return;
    ClkrstSession session;
    if (R_SUCCEEDED(clkrstOpenSession(&session, PcvModuleId_CpuBus, 3))) {
        clkrstSetClockRate(&session, saved_cpu_hz);
        clkrstCloseSession(&session);
    }
    clkrstExit();
}
void host_clock_cpu(unsigned int mhz)
{
    if (mhz != 0 && mhz != 1020 && mhz != 1785) {
        host_logf(HOST_LOG_WARN, "deko: unsupported CPU setting %u MHz", mhz); return;
    }
    if (!mhz && !saved_cpu_hz) return; /* Stock changes nothing. */
    Result result = clkrstInitialize();
    if (R_FAILED(result)) { host_logf(HOST_LOG_WARN, "deko: CPU clock service unavailable: %08x", result); return; }
    ClkrstSession session;
    result = clkrstOpenSession(&session, PcvModuleId_CpuBus, 3);
    if (R_SUCCEEDED(result)) {
        if (!saved_cpu_hz) result = clkrstGetClockRate(&session, &saved_cpu_hz);
        if (R_SUCCEEDED(result) && saved_cpu_hz) {
            if (!restore_registered) {
                if (atexit(restore_cpu_clock) == 0) restore_registered = 1;
                else result = 1;
            }
            if (R_SUCCEEDED(result)) result = clkrstSetClockRate(&session, mhz ? mhz * 1000000u : saved_cpu_hz);
        }
        clkrstCloseSession(&session);
    }
    clkrstExit();
    host_logf(R_SUCCEEDED(result) ? HOST_LOG_INFO : HOST_LOG_WARN, "deko: CPU clock request %u MHz result=%08x", mhz, result);
}

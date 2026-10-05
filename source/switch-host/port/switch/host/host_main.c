/*
HOST_MAIN.C — Nintendo Switch

NRO entry point. Initializes libnx services, loads the guest image from
romfs, sets up the environment and runs the game.

Based on the Android port's host_main.c, adapted for Switch/libnx:
- nxlink stdio for logging (replaces Android logcat)
- romfs for the guest ELF (replaces APK assets)
- SD card paths for game data and saves
- Switch display mode (1280x720 docked, 720x? handheld)
*/

#include "host.h"
#include "host_log_queue.h"

#include <switch.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include <SDL2/SDL.h>

/* ---------- libnx service overrides ---------- */

/* Request more memory for the NRO / Application. */
/* Force Application applet type for installed NSP on Switch HOME Menu */
u32 __nx_applet_type = AppletType_Application;

/* Host heap follows the kernel's ASLR layout. Guest memory is mapped separately
 * at its ILP32 addresses; no host heap address is assumed. */
u64 __nx_heap_size = 0x30000000ULL; /* 768 MB */

/* Retained without allocation so the earliest SD log can explain boot heap
 * failures/layout differences on hardware. */
static struct
{
	u64 requested;
	uintptr_t base;
	Result result;
} s_boot_heap_attempts[3];
static unsigned s_boot_heap_attempt_count;

static Result request_boot_heap(void **address, u64 size)
{
	unsigned index = s_boot_heap_attempt_count++;
	Result result = svcSetHeapSize(address, size);
	if (index < 3)
	{
		s_boot_heap_attempts[index].requested = size;
		s_boot_heap_attempts[index].base = (uintptr_t)*address;
		s_boot_heap_attempts[index].result = result;
	}
	return result;
}

void __libnx_initheap(void)
{
	extern void* fake_heap_start;
	extern void* fake_heap_end;

	void* addr = NULL;
	Result rc = request_boot_heap(&addr, __nx_heap_size);
	if (R_FAILED(rc))
	{
		__nx_heap_size = 0x20000000ULL; /* 512 MB fallback */
		rc = request_boot_heap(&addr, __nx_heap_size);
		if (R_FAILED(rc))
		{
			__nx_heap_size = 0x18000000ULL; /* 384 MB fallback */
			rc = request_boot_heap(&addr, __nx_heap_size);
			if (R_FAILED(rc))
			{
				const char msg[] = "HALO: svcSetHeapSize failed!\n";
				svcOutputDebugString(msg, sizeof(msg) - 1);
				diagAbortWithResult(rc);
			}
		}
	}

	fake_heap_start = (char*)addr;
	fake_heap_end = (char*)addr + __nx_heap_size;
}

/* ---------- nxlink debugging & initialization ---------- */

static int s_nxlinkSock = -1;
static bool s_sdmcMounted = false;

void host_log_init(void);

void userAppInit(void)
{
	/* libnx initialized FS before this hook. Start the persistent trace before
	 * optional network setup and bundled-data mounting can fail. */
	bool already_mounted = fsdevGetDeviceFileSystem("sdmc") != NULL;
	Result rc = already_mounted ? 0 : fsdevMountSdmc();
	if (R_SUCCEEDED(rc))
		s_sdmcMounted = true;
	host_log_init();
	host_logf(HOST_LOG_INFO, "build native-profile33-full-pipeline-protocol11: userAppInit");
	host_logf(HOST_LOG_INFO, "startup SD mount result=0x%x existing=%d", rc, already_mounted);
	bool capture_supported=false;
	Result capture_rc=appletIsGamePlayRecordingSupported(&capture_supported);
	host_logf(HOST_LOG_INFO,"capture automatic support=%d result=0x%x",capture_supported,capture_rc);
	for (unsigned i = 0; i < s_boot_heap_attempt_count && i < 3; i++)
		host_logf(HOST_LOG_INFO, "startup heap attempt=%u size=0x%llx base=0x%llx result=0x%x",
			i + 1, (unsigned long long)s_boot_heap_attempts[i].requested,
			(unsigned long long)s_boot_heap_attempts[i].base,
			s_boot_heap_attempts[i].result);

	/* Guest transport requests 1 MiB UDP buffers. Allocate them up front
     * instead of growing beyond the default BSD transfer-memory budget. */
    SocketInitConfig net_config = *socketGetDefaultInitConfig();
    net_config.udp_tx_buf_size = 1024 * 1024;
    net_config.udp_rx_buf_size = 1024 * 1024;
    net_config.tcp_tx_buf_max_size = 1024 * 1024;
    net_config.tcp_rx_buf_max_size = 1024 * 1024;
    host_logf(HOST_LOG_INFO, "socket_budget tcp_max=%u/%u udp=%u/%u efficiency=%u",
        net_config.tcp_tx_buf_max_size, net_config.tcp_rx_buf_max_size,
        net_config.udp_tx_buf_size, net_config.udp_rx_buf_size, net_config.sb_efficiency);
    rc = nifmInitialize(NifmServiceType_User);
    host_logf(HOST_LOG_INFO, "startup LAN configuration service result=0x%x", rc);
    rc = sslInitialize(2);
    host_logf(HOST_LOG_INFO, "startup TLS service result=0x%x", rc);
    rc = socketInitialize(&net_config);
	host_logf(HOST_LOG_INFO, "startup sockets result=0x%x", rc);
	if (R_SUCCEEDED(rc))
		s_nxlinkSock = nxlinkStdio();

	rc = romfsInit();
	host_logf(R_FAILED(rc) ? HOST_LOG_ERROR : HOST_LOG_INFO,
		"startup RomFS result=0x%x", rc);
}

void userAppExit(void)
{
	/* main/guest producer has ended. libnx tears down FS after this hook,
	 * so terminate every thread if a stalled writer cannot stop in time. */
	if (!host_log_async_stop(1000))
		svcExitProcess();
	romfsExit();
    sslExit();
    nifmExit();
	if (s_sdmcMounted)
	{
		fsdevUnmountDevice("sdmc");
		s_sdmcMounted = false;
	}
	if (s_nxlinkSock >= 0)
	{
		close(s_nxlinkSock);
		s_nxlinkSock = -1;
		socketExit();
	}
}

/* ---------- logging ---------- */

FILE *s_logFile = NULL;
static Mutex log_output_lock;
/* Routine reports have exactly one producer: the rendering thread. */
static char summary_batch[HOST_LOG_BATCH_CAPACITY];
static size_t summary_length;
static bool summary_overflow;
static bool log_runtime_async;

void host_log_init(void)
{
	/* main() also calls this; preserve the earlier startup trace. */
	if (s_logFile)
		return;
	mkdir("sdmc:/switch", 0777);
	mkdir("sdmc:/switch/halo", 0777);
	s_logFile = fopen("sdmc:/switch/halo/halo-profile33.log", "w");
	if (s_logFile)
	{
		fprintf(s_logFile, "=== Halo CE Nintendo Switch Log Started: native-profile33-full-pipeline-protocol11 ===\n");
		fflush(s_logFile);
	}
}

static void host_logv(int priority, const char *format, va_list arguments, int flush)
{
    va_list copy;
    mutexLock(&log_output_lock);
    const char *prefix = priority >= HOST_LOG_ERROR ? "ERROR" :
                         priority >= HOST_LOG_WARN ? "WARN" : "INFO";
    printf("[halo/%s] ", prefix);
    va_copy(copy, arguments);
    vprintf(format, copy);
    va_end(copy);
    printf("\n");
    if (flush) fflush(stdout);
    if (s_logFile)
    {
        fprintf(s_logFile, "[%s] ", prefix);
        va_copy(copy, arguments);
        vfprintf(s_logFile, format, copy);
        va_end(copy);
        fprintf(s_logFile, "\n");
        if (flush) fflush(s_logFile);
    }
    mutexUnlock(&log_output_lock);
}

void host_logf(int priority, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    if (log_runtime_async) {
        char bytes[HOST_LOG_BATCH_CAPACITY];
        const char *prefix = priority >= HOST_LOG_ERROR ? "ERROR" :
                             priority >= HOST_LOG_WARN ? "WARN" : "INFO";
        int head = snprintf(bytes,sizeof(bytes),"[%s] ",prefix);
        int length = head > 0 ? vsnprintf(bytes+head,sizeof(bytes)-(size_t)head,format,arguments) : -1;
        if (length >= 0 && (size_t)(head+length+1) <= sizeof(bytes)) {
            bytes[head+length]='\n';
            host_log_async_submit(bytes,(size_t)(head+length+1));
        } else host_log_async_submit(bytes,sizeof(bytes)+1);
    } else host_logv(priority, format, arguments, 1);
    va_end(arguments);
}

/* Only the native writer performs routine report I/O. The producer never
 * acquires the output mutex and never falls back to synchronous output. */
static void host_log_write_summary(const char *bytes, size_t length)
{
    mutexLock(&log_output_lock);
    fwrite(bytes, 1, length, stdout);
    fflush(stdout);
    if (s_logFile) {
        fwrite(bytes, 1, length, s_logFile);
        fflush(s_logFile);
    }
    mutexUnlock(&log_output_lock);
}

/* During gameplay, stdout/stderr never wait for SD IO or the writer mutex. */
void host_log_write_bytes(int fd, const char *bytes, size_t length)
{
    if (!length) return;
    if (log_runtime_async) {
        host_log_async_submit(bytes,length);
        return;
    }
    mutexLock(&log_output_lock);
    FILE *stream = fd == 2 ? stderr : stdout;
    fwrite(bytes, 1, length, stream);
    fflush(stream);
    if (s_logFile) {
        fwrite(bytes, 1, length, s_logFile);
        fflush(s_logFile);
    }
    mutexUnlock(&log_output_lock);
}

void host_logf_buffered(int priority, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    if (priority >= HOST_LOG_WARN) {
        char bytes[HOST_LOG_BATCH_CAPACITY];
        int length=vsnprintf(bytes,sizeof(bytes)-1,format,arguments);
        if(length>=0 && (size_t)length<sizeof(bytes)-1) {
            bytes[length]='\n';host_log_async_submit(bytes,(size_t)length+1);
        } else host_log_async_submit(bytes,sizeof(bytes)+1);
    } else if (!summary_overflow) {
        size_t available = sizeof(summary_batch) - summary_length;
        int prefix = snprintf(summary_batch + summary_length, available, "[INFO] ");
        if (prefix < 0 || (size_t)prefix >= available) summary_overflow = true;
        else {
            size_t offset = summary_length + (size_t)prefix;
            available = sizeof(summary_batch) - offset;
            int count = vsnprintf(summary_batch + offset, available, format, arguments);
            /* Leave room for the newline. Drop the whole report on overflow. */
            if (count < 0 || (size_t)count >= available) summary_overflow = true;
            else {
                summary_length = offset + (size_t)count;
                summary_batch[summary_length++] = '\n';
            }
        }
    }
    va_end(arguments);
}

uint64_t host_log_dropped_batches(void)
{
    return host_log_async_dropped();
}

void host_log_flush(void)
{
    if (summary_overflow)
        host_log_async_submit(summary_batch, sizeof(summary_batch) + 1);
    else if (summary_length)
        host_log_async_submit(summary_batch, summary_length);
    summary_length = 0;
    summary_overflow = false;
}

void host_fatal(const char *format, ...)
{
	char message[1024];
	va_list arguments;

	va_start(arguments, format);
	vsnprintf(message, sizeof(message), format, arguments);
	va_end(arguments);

	mutexLock(&log_output_lock);
	printf("[halo/FATAL] %s\n", message);
	fflush(stdout);

	if (s_logFile)
	{
		fprintf(s_logFile, "[FATAL] %s\n", message);
		fflush(s_logFile);
		fclose(s_logFile);
		s_logFile = NULL;
	}

	mutexUnlock(&log_output_lock);

	/* Show error on console screen before exiting */
	consoleInit(NULL);
	printf("\n\x1b[1;31m========================================\x1b[0m\n");
	printf("\x1b[1;37m           HALO CE FATAL ERROR          \x1b[0m\n");
	printf("\x1b[1;31m========================================\x1b[0m\n\n");
	printf("%s\n\n", message);
	printf("\x1b[1;33mLog saved to: sdmc:/switch/halo/halo-profile33.log\x1b[0m\n\n");
	printf("\x1b[1;33mPress + or B to exit back to menu.\x1b[0m\n");

	PadState pad;
	padInitializeDefault(&pad);
	while (appletMainLoop())
	{
		padUpdate(&pad);
		u64 kDown = padGetButtonsDown(&pad);
		if (kDown & (HidNpadButton_Plus | HidNpadButton_B | HidNpadButton_A))
			break;
		consoleUpdate(NULL);
	}
	consoleExit(NULL);
	_exit(1);
}

void host_exit(int code)
{
	host_logf(HOST_LOG_INFO, "the game exited (%d)", code);
	mutexLock(&log_output_lock);
	if (s_logFile)
	{
		fclose(s_logFile);
		s_logFile = NULL;
	}
	mutexUnlock(&log_output_lock);
	_exit(code);
}

int host_errno(void)
{
	return errno;
}

/* ---------- paths ---------- */

/* Game data on SD card: /switch/halo/
   Save data:            /switch/halo/save/ */
static char data_root[512];
static char save_root[512];

void host_switch_path(int which, char *buffer, uint32_t size)
{
	snprintf(buffer, size, "%s", which ? save_root : data_root);
}

static int directory_has_maps(const char *root)
{
	char path[600];
	struct stat information;

	snprintf(path, sizeof(path), "%s/maps/ui.map", root);
	return stat(path, &information) == 0;
}

/* ---------- the guest's environment ---------- */

#define ENVIRONMENT_MAXIMUM 64

struct environment
{
	char *entries[ENVIRONMENT_MAXIMUM];
	int count;
};

static void environment_set(struct environment *env, const char *name,
                            const char *value)
{
	size_t length = strlen(name);
	char *entry;
	int index;

	entry = malloc(length + strlen(value) + 2);
	sprintf(entry, "%s=%s", name, value);
	for (index = 0; index < env->count; index++)
	{
		if (!strncmp(env->entries[index], name, length) &&
		    env->entries[index][length] == '=')
		{
			free(env->entries[index]);
			env->entries[index] = entry;
			return;
		}
	}
	if (env->count < ENVIRONMENT_MAXIMUM)
		env->entries[env->count++] = entry;
	else
		free(entry);
}

/* Copies argv and the environment into guest memory */
static uint32_t make_boot(const struct environment *environment)
{
	size_t size = 0x10000;
	char *memory = host_low_map(size, 0);
	struct halo_guest_boot *boot = (struct halo_guest_boot *)memory;
	uint32_t *argv = (uint32_t *)(memory + sizeof(*boot));
	uint32_t *environ_list = argv + 2;
	char *strings = (char *)(environ_list + ENVIRONMENT_MAXIMUM + 1);
	int index;

	if (!memory)
		host_fatal("cannot allocate the guest's environment");

	strcpy(strings, "halo");
	argv[0] = (uint32_t)(uintptr_t)strings;
	argv[1] = 0;
	strings += strlen(strings) + 1;

	for (index = 0; index < environment->count; index++)
	{
		size_t length = strlen(environment->entries[index]) + 1;
		if (strings + length > memory + size)
			break;
		memcpy(strings, environment->entries[index], length);
		environ_list[index] = (uint32_t)(uintptr_t)strings;
		strings += length;
	}
	environ_list[index] = 0;

	boot->argc = 1;
	boot->argv = (uint32_t)(uintptr_t)argv;
	boot->environment = (uint32_t)(uintptr_t)environ_list;
	boot->page_size = HALO_SWITCH_PAGE_SIZE;
	return (uint32_t)(uintptr_t)boot;
}

/* ---------- main ---------- */

#define MAIN_STACK_SIZE (4 * 1024 * 1024)

static void *game_main(void *unused)
{
	struct environment environment = { { 0 }, 0 };
	char width_str[16];
	FILE *guest_file;
	size_t image_size;
	void *image;
	uint32_t boot;

	(void)unused;

	/* Check romfs first (all-in-one standalone package), then fallback to SD card */
	if (directory_has_maps("romfs:"))
	{
		snprintf(data_root, sizeof(data_root), "romfs:");
		host_logf(HOST_LOG_INFO, "using bundled RomFS data root");
	}
	else
	{
		snprintf(data_root, sizeof(data_root), "sdmc:/switch/halo");
		host_logf(HOST_LOG_INFO, "using SD card data root: %s", data_root);
	}

	snprintf(save_root, sizeof(save_root), "sdmc:/switch/halo/save-community24");
	mkdir(save_root, 0775);

	if (!directory_has_maps(data_root))
	{
		host_fatal(
			"Copy the Halo game data (build 01.01.14.2342 or 01.10.12.2276),\n"
			"the directory containing maps/, into:\n"
			"sdmc:/switch/halo/\n"
			"on your SD card.");
	}

	environment_set(&environment, "HOME", save_root);
	environment_set(&environment, "HALO_DATA_ROOT", data_root);
	environment_set(&environment, "HALO_SAVE_ROOT", save_root);

	/* Switch display: 1280x720 docked, variable handheld.
	   The game renders 480 lines; width = 480 * aspect. */
	{
		int display_width = 854; /* default 16:9 at 480p */

		/* Check if we're docked (1920x1080 output) or handheld (1280x720) */
		AppletOperationMode mode = appletGetOperationMode();
		if (mode == AppletOperationMode_Console)
			display_width = 854; /* 16:9 */
		else
			display_width = 854; /* handheld is also 16:9 */

		snprintf(width_str, sizeof(width_str), "%d", display_width);
		environment_set(&environment, "HALO_DISPLAY_WIDTH", width_str);
		host_logf(HOST_LOG_INFO, "rendering %sx480", width_str);
	}

	/* Load the guest ELF from romfs */
	guest_file = fopen("romfs:/halo_guest.elf", "rb");
	if (!guest_file)
	{
		/* Try SD card as fallback */
		char elf_path[600];
		snprintf(elf_path, sizeof(elf_path), "%s/halo_guest.elf", data_root);
		guest_file = fopen(elf_path, "rb");
	}
	if (!guest_file)
		host_fatal("cannot find halo_guest.elf (checked romfs and SD card)");

	fseek(guest_file, 0, SEEK_END);
	image_size = ftell(guest_file);
	fseek(guest_file, 0, SEEK_SET);
	image = malloc(image_size);
	if (!image)
		host_fatal("cannot allocate %lu bytes for the guest image",
			(unsigned long)image_size);
	if (fread(image, 1, image_size, guest_file) != image_size)
		host_fatal("short read of the guest image");
	fclose(guest_file);

	if (host_load_image(image, image_size) != 0)
		host_fatal("cannot load the guest image");
	free(image);

	if (!host_log_async_start(host_log_write_summary))
		host_logf(HOST_LOG_WARN, "async diagnostics unavailable; routine reports will be dropped");
	else
		host_logf(HOST_LOG_INFO, "async diagnostics enabled: 128x4096-byte batches, routine host/guest output off render thread");
	log_runtime_async = true; /* Also drop safely if the worker failed to start. */
	host_memory_crc_benchmark();
	boot = make_boot(&environment);
	host_logf(HOST_LOG_INFO, "data %s, saves %s", data_root, save_root);
	host_run_guest_main(boot);
}

int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	host_log_init();
	host_logf(HOST_LOG_INFO, "Halo CE for Nintendo Switch starting");

	u64 alias_addr = 0, alias_size = 0;
	u64 heap_addr = 0, heap_size = 0;
	u64 aslr_addr = 0, aslr_size = 0;
	u64 stack_addr = 0, stack_size = 0;
	u64 sys_res_total = 0, sys_res_used = 0;
	u64 total_mem = 0, used_mem = 0;
	svcGetInfo(&alias_addr, InfoType_AliasRegionAddress, CUR_PROCESS_HANDLE, 0);
	svcGetInfo(&alias_size, InfoType_AliasRegionSize, CUR_PROCESS_HANDLE, 0);
	svcGetInfo(&heap_addr, InfoType_HeapRegionAddress, CUR_PROCESS_HANDLE, 0);
	svcGetInfo(&heap_size, InfoType_HeapRegionSize, CUR_PROCESS_HANDLE, 0);
	svcGetInfo(&aslr_addr, InfoType_AslrRegionAddress, CUR_PROCESS_HANDLE, 0);
	svcGetInfo(&aslr_size, InfoType_AslrRegionSize, CUR_PROCESS_HANDLE, 0);
	svcGetInfo(&stack_addr, InfoType_StackRegionAddress, CUR_PROCESS_HANDLE, 0);
	svcGetInfo(&stack_size, InfoType_StackRegionSize, CUR_PROCESS_HANDLE, 0);
	svcGetInfo(&sys_res_total, InfoType_SystemResourceSizeTotal, CUR_PROCESS_HANDLE, 0);
	svcGetInfo(&sys_res_used, InfoType_SystemResourceSizeUsed, CUR_PROCESS_HANDLE, 0);
	svcGetInfo(&total_mem, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
	svcGetInfo(&used_mem, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
	AppletType a_type = appletGetAppletType();

	host_logf(HOST_LOG_INFO, "=== Horizon Memory Layout Diagnostics ===");
	host_logf(HOST_LOG_INFO, "AppletType:      %d (0=Application, -1=Default, -2=None)", a_type);
	host_logf(HOST_LOG_INFO, "System Resource: Total=0x%lx, Used=0x%lx", (unsigned long)sys_res_total, (unsigned long)sys_res_used);
	host_logf(HOST_LOG_INFO, "Total Memory:    0x%lx (%lu MB), Used=0x%lx", (unsigned long)total_mem, (unsigned long)(total_mem / (1024ULL * 1024ULL)), (unsigned long)used_mem);
	host_logf(HOST_LOG_INFO, "Alias Region:    0x%lx - 0x%lx (size 0x%lx)", (unsigned long)alias_addr, (unsigned long)(alias_addr + alias_size), (unsigned long)alias_size);
	host_logf(HOST_LOG_INFO, "Heap Region:     0x%lx - 0x%lx (size 0x%lx)", (unsigned long)heap_addr, (unsigned long)(heap_addr + heap_size), (unsigned long)heap_size);
	host_logf(HOST_LOG_INFO, "ASLR Region:     0x%lx - 0x%lx (size 0x%lx)", (unsigned long)aslr_addr, (unsigned long)(aslr_addr + aslr_size), (unsigned long)aslr_size);
	host_logf(HOST_LOG_INFO, "Stack Region:    0x%lx - 0x%lx (size 0x%lx)", (unsigned long)stack_addr, (unsigned long)(stack_addr + stack_size), (unsigned long)stack_size);
	host_logf(HOST_LOG_INFO, "=========================================");

	host_install_fault_handler();

	game_main(NULL);

	return 0;
}

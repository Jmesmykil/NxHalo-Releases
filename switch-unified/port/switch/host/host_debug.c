/*
HOST_DEBUG.C — Nintendo Switch

Debug thread tracking. Simplified from Android (no signal-based sampler;
on Switch we just track thread registration for logging).
*/

#include "host.h"
#include <switch.h>
#include <string.h>

#define MAXIMUM_THREADS 64

static uint32_t guest_thread_ids[MAXIMUM_THREADS];
static Mutex threads_lock;
static int lock_init = 0;

static void ensure_lock(void)
{
	if (!lock_init)
	{
		mutexInit(&threads_lock);
		lock_init = 1;
	}
}

void host_debug_thread_started(void)
{
	int index;

	ensure_lock();
	mutexLock(&threads_lock);
	for (index = 0; index < MAXIMUM_THREADS; index++)
	{
		if (!guest_thread_ids[index])
		{
			guest_thread_ids[index] = 1; /* placeholder */
			break;
		}
	}
	mutexUnlock(&threads_lock);
}

void host_debug_thread_exited(void)
{
	/* Just decrement active count; thread IDs are placeholders */
	ensure_lock();
	mutexLock(&threads_lock);
	{
		int index;
		for (index = MAXIMUM_THREADS - 1; index >= 0; index--)
		{
			if (guest_thread_ids[index])
			{
				guest_thread_ids[index] = 0;
				break;
			}
		}
	}
	mutexUnlock(&threads_lock);
}

void host_debug_start_sampler(const char *setting)
{
	/* No signal-based sampler on Switch; just log the setting */
	if (setting)
		host_logf(HOST_LOG_INFO,
			"sampling requested (%s s) but not available on Switch",
			setting);
}

/* Preserve the actual fatal caller before libnx's fatal service discards the
 * user stack. Driver failures then have an ELF-relative site in halo.log. */
extern void _start(void);
extern void __real_fatalThrow(Result result) __attribute__((noreturn));
void __wrap_fatalThrow(Result result)
{
    uintptr_t caller = (uintptr_t)__builtin_return_address(0);
    host_logf(HOST_LOG_ERROR, "native fatal result=0x%x caller_offset=0x%llx staged_IPC=%llu",
        result, (unsigned long long)(caller - (uintptr_t)_start),
        (unsigned long long)host_ipc_staged_requests());
    __real_fatalThrow(result);
}

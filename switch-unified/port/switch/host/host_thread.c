/*
HOST_THREAD.C — Nintendo Switch

Threads that run guest code. Adapted from Android: uses pthread (which
libnx/newlib provides) with stacks allocated in guest memory (below 4 GB)
via host_low_map. The guest's ILP32 code keeps stack pointers in 32-bit
registers, so every thread must have its stack there.

libnx's pthread implementation sits on top of the Switch's native Thread
API. We use pthread directly for portability with the Android code.
*/

#include "host.h"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>

static __thread uint32_t guest_tp;

uint32_t host_get_tp(void)
{
	return guest_tp;
}

void host_set_tp(uint32_t thread)
{
	guest_tp = thread;
}

/* ---------- calling into the guest ---------- */

static int on_guest_stack(void)
{
    return (uintptr_t)__builtin_frame_address(0) < UINT64_C(0x100000000);
}

typedef uint32_t (*guest_function)(uint32_t, uint32_t, uint32_t, uint32_t);
extern uint32_t host_call_on_guest_stack(uintptr_t top, uintptr_t function,
    uint32_t a, uint32_t b, uint32_t c, uint32_t d);

static __thread void *tls_guest_stack;
static __thread size_t tls_guest_stack_size = 1024 * 1024;

uint32_t host_call_guest(uint32_t function, uint32_t a, uint32_t b,
                         uint32_t c, uint32_t d)
{
    if (on_guest_stack()) {
        if (!guest_tp && function != host_image.header->thread_start)
            ((guest_function)(uintptr_t)host_image.header->thread_attach)(0,0,0,0);
        return ((guest_function)(uintptr_t)function)(a,b,c,d);
    }
    if (!tls_guest_stack) {
        tls_guest_stack = host_low_map(tls_guest_stack_size, 0);
        if (!tls_guest_stack) host_fatal("cannot allocate secondary guest stack");
    }
    uintptr_t top = ((uintptr_t)tls_guest_stack + tls_guest_stack_size - 0x20) & ~(uintptr_t)15;
    /* thread_attach executes guest malloc/TLS code too: it must already be on
     * an ILP32-addressable stack, including SDL's native audio worker. */
    if (!guest_tp && function != host_image.header->thread_start)
        host_call_on_guest_stack(top, host_image.header->thread_attach, 0,0,0,0);
    return host_call_on_guest_stack(top, function, a,b,c,d);
}

void host_run_guest_main(uint32_t boot)
{
    const size_t size = 4 * 1024 * 1024;
    void *stack = host_low_map(size, 0);
    if (!stack) host_fatal("cannot allocate guest stack in low memory");
    uintptr_t top = ((uintptr_t)stack + size - 0x20) & ~(uintptr_t)15;
    host_call_on_guest_stack(top, host_image.header->start, boot,0,0,0);
    host_fatal("the guest returned from __guest_start");
}

/* ---------- guest threads ---------- */

struct thread_start
{
	void *(*function)(void *);
	void *argument;
	size_t guest_stack_size;
};

static void *thread_main(void *context)
{
	struct thread_start start = *(struct thread_start *)context;

	free(context);
	if (start.guest_stack_size > tls_guest_stack_size)
		tls_guest_stack_size = start.guest_stack_size;
	host_debug_thread_started();
	start.function(start.argument);
	host_debug_thread_exited();
	guest_tp = 0;
	if (tls_guest_stack) {
		host_low_unmap(tls_guest_stack, tls_guest_stack_size);
		tls_guest_stack = NULL;
	}
	return NULL;
}

int host_native_thread_create(void *(*function)(void *), void *argument,
                              size_t stack_size)
{
	struct thread_start *start = calloc(1, sizeof(*start));
	pthread_attr_t attributes;
	pthread_t thread;
	int error;

	if (!start)
		return ENOMEM;

	start->function = function;
	start->argument = argument;
	if (stack_size > SIZE_MAX - 0xfff) { free(start); return EINVAL; }
	start->guest_stack_size = (stack_size + 0xfff) & ~(size_t)0xfff;

	pthread_attr_init(&attributes);
	pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
	if (stack_size > 0)
		pthread_attr_setstacksize(&attributes, (stack_size + 0xfff) & ~(size_t)0xfff);
	error = pthread_create(&thread, &attributes, thread_main, start);
	pthread_attr_destroy(&attributes);

	if (error)
	{
		host_logf(HOST_LOG_ERROR, "pthread_create failed: %d", error);
		free(start);
	}
	return error;
}

static void *guest_thread_main(void *guest_thread)
{
	host_call_guest(host_image.header->thread_start,
		(uint32_t)(uintptr_t)guest_thread, 0, 0, 0);
	return NULL;
}

int host_thread_create(uint32_t guest_thread, uint32_t stack_size)
{
	return host_native_thread_create(guest_thread_main,
		(void *)(uintptr_t)guest_thread, stack_size);
}

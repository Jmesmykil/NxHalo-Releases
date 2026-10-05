/*
HOST_SDL.C — Nintendo Switch

SDL bridge for the guest. The Android port uses SDL3; the Switch portlibs
ship SDL2. This file provides the same host_sdl_* interface the guest
imports, translating to SDL2 calls.

Key SDL3→SDL2 differences handled here:
- SDL3 audio streams → SDL2 audio device callbacks
- SDL3 gamepad API → SDL2 game controller API
- SDL3 window creation flags
- SDL3 SDL_GetTicks returns Uint64; SDL2 returns Uint32
*/

#include "host.h"
#include "host_file_io.h"
#include "host_stall_profile.h"
#include "host_slow_frame_profile.h"
#include "host_phase_profile.h"

#include <switch.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengles2.h>
#include <pthread.h>
#include <string.h>
#include "host_sdl_input.h"
#include "host_frame_profile.h"
#include "host_audio_profile.h"
#include "host_frame_bridge.h"

static void host_audio_log_stats(uint32_t frame);
void host_gpu_time_begin(uint32_t frame);
void host_gpu_time_end(void);
void host_gpu_time_report(uint32_t frame);
void host_hardware_profile_report(uint32_t frame);
void host_network_profile_report(uint32_t frame);
void host_graphics_validation_init(void);
void host_graphics_validation_report(uint32_t frame);

#define HANDLE_COUNT 256

enum handle_type
{
	_handle_free,
	_handle_window,
	_handle_context,
	_handle_gamepad,
	_handle_audio,
};

struct handle
{
	int type;
	void *object;
};

static struct handle handles[HANDLE_COUNT];
static Mutex handle_lock;
static int handle_lock_init = 0;

static void ensure_lock(void)
{
	if (!handle_lock_init)
	{
		mutexInit(&handle_lock);
		handle_lock_init = 1;
	}
}

static uint32_t handle_new(int type, void *object)
{
	uint32_t index;

	if (!object)
		return 0;
	ensure_lock();
	mutexLock(&handle_lock);
	for (index = 1; index < HANDLE_COUNT; index++)
	{
		if (handles[index].type == type &&
		    handles[index].object == object)
		{
			mutexUnlock(&handle_lock);
			return index;
		}
	}
	for (index = 1; index < HANDLE_COUNT; index++)
	{
		if (handles[index].type == _handle_free)
		{
			handles[index].type = type;
			handles[index].object = object;
			mutexUnlock(&handle_lock);
			return index;
		}
	}
	mutexUnlock(&handle_lock);
	host_logf(HOST_LOG_ERROR, "out of SDL handles");
	return 0;
}

static void *handle_get(uint32_t handle, int type)
{
	void *object = NULL;

	if (handle == 0 || handle >= HANDLE_COUNT)
		return NULL;
	ensure_lock();
	mutexLock(&handle_lock);
	if (handles[handle].type == type)
		object = handles[handle].object;
	mutexUnlock(&handle_lock);
	return object;
}

/* ---------- general ---------- */

int host_sdl_init(uint32_t flags)
{
	/* Map SDL3 init flags to SDL2 equivalents */
	Uint32 sdl2_flags = 0;

	if (flags & 0x00000020) sdl2_flags |= SDL_INIT_VIDEO;      /* SDL_INIT_VIDEO */
	if (flags & 0x00000010) sdl2_flags |= SDL_INIT_AUDIO;      /* SDL_INIT_AUDIO */
	if (flags & 0x00000200) sdl2_flags |= SDL_INIT_JOYSTICK;   /* SDL_INIT_JOYSTICK */
	if (flags & 0x00002000) sdl2_flags |= SDL_INIT_GAMECONTROLLER; /* SDL_INIT_GAMEPAD */
	if (flags & 0x00001000) sdl2_flags |= SDL_INIT_HAPTIC;     /* SDL_INIT_HAPTIC */
	if (flags & 0x00004000) sdl2_flags |= SDL_INIT_EVENTS;     /* SDL_INIT_EVENTS */

	if (!sdl2_flags)
		sdl2_flags = flags; /* pass through if no translation needed */

	return SDL_Init(sdl2_flags) == 0 ? 1 : 0;
}

int host_sdl_set_hint(const char *name, const char *value)
{
	return SDL_SetHint(name, value);
}

void host_sdl_get_error(char *buffer, uint32_t size)
{
	SDL_strlcpy(buffer, SDL_GetError(), size);
}

int64_t host_sdl_ticks(void)
{
	return (int64_t)SDL_GetTicks();
}

int64_t host_sdl_thread_id(void)
{
	return (int64_t)SDL_ThreadID();
}

/* ---------- video ---------- */

uint32_t host_sdl_create_window(const char *title, int width, int height,
                                int64_t flags)
{
	host_logf(HOST_LOG_INFO, "host_sdl_create_window: '%s' %dx%d flags=0x%llx",
		title, width, height, (unsigned long long)flags);

	/* Switch native display resolution: 1280x720 handheld, 1920x1080 docked */
	int win_w = 1280, win_h = 720;
	AppletOperationMode mode = appletGetOperationMode();
	if (mode == AppletOperationMode_Console)
	{
		win_w = 1920;
		win_h = 1080;
	}

	Uint32 sdl2_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
	SDL_Window *window = SDL_CreateWindow(title, 0, 0, win_w, win_h, sdl2_flags);
	if (!window)
	{
		host_logf(HOST_LOG_ERROR, "SDL_CreateWindow failed: %s", SDL_GetError());
		return 0;
	}
	host_logf(HOST_LOG_INFO, "SDL_CreateWindow SUCCESS: %dx%d (staged IPC=%llu)", win_w, win_h,
		(unsigned long long)host_ipc_staged_requests());
	return handle_new(_handle_window, window);
}

void host_sdl_window_size_in_pixels(uint32_t window, int *width, int *height)
{
	SDL_Window *object = handle_get(window, _handle_window);

	*width = 0;
	*height = 0;
	if (object)
		SDL_GL_GetDrawableSize(object, width, height);
}

int host_sdl_set_relative_mouse(uint32_t window, int enabled)
{
	(void)window;
	return SDL_SetRelativeMouseMode(enabled ? SDL_TRUE : SDL_FALSE) == 0;
}

int host_sdl_gl_set_attribute(int attribute, int value)
{
	host_logf(HOST_LOG_INFO, "host_sdl_gl_set_attribute(%d, %d)", attribute, value);
	return SDL_GL_SetAttribute((SDL_GLattr)attribute, value) == 0;
}

uint32_t host_sdl_gl_create_context(uint32_t window)
{
	SDL_Window *object = handle_get(window, _handle_window);
	if (!object)
	{
		host_logf(HOST_LOG_ERROR, "host_sdl_gl_create_context: invalid window %u", window);
		return 0;
	}

	SDL_GLContext context = SDL_GL_CreateContext(object);
	if (!context)
	{
		host_logf(HOST_LOG_ERROR, "SDL_GL_CreateContext initial attempt failed: %s", SDL_GetError());

		/* Fallback 1: GLES 2.0 (Mesa EGL standard on Switch) */
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		context = SDL_GL_CreateContext(object);

		if (!context)
		{
			host_logf(HOST_LOG_ERROR, "SDL_GL_CreateContext fallback GLES 2.0 failed: %s", SDL_GetError());

			/* Fallback 2: Desktop OpenGL Core 4.3 (switch-mesa native) */
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
			context = SDL_GL_CreateContext(object);
		}
	}

	if (context)
	{
		host_logf(HOST_LOG_INFO, "SDL_GL_CreateContext SUCCESS!");
		return handle_new(_handle_context, context);
	}

	host_logf(HOST_LOG_ERROR, "All SDL_GL_CreateContext attempts failed: %s", SDL_GetError());
	return 0;
}

int host_sdl_gl_make_current(uint32_t window, uint32_t context)
{
	static uint32_t reported_context;
	SDL_GLContext object = handle_get(context, _handle_context);
	int current = SDL_GL_MakeCurrent(
		handle_get(window, _handle_window),
		object) == 0;
	if (current && object && context != reported_context)
	{
		host_gl_log_capabilities();
		host_graphics_validation_init();
		reported_context = context;
	}
	return current;
}

/* Slow-frame context is formatted only at the 300-frame boundary. Raw values
 * remain64bit in the ring; wire values explicitly clamp with clipped=1.
 * Metric order is enum gpu_metric/event. CPU completed-call rows include nested
 * helper phases and are not GPU execution measurements or disjoint costs. */
static void host_slow_log_record(const struct host_slow_frame_record *r,
    uint32_t window, unsigned group, const char *role)
{
    _Static_assert(GPU_METRIC_COUNT <= 20, "slow metric row capacity");
    _Static_assert(GPU_EVENT_COUNT <= 20, "slow event row capacity");
    char calls[224] = {0}, times[224] = {0}, events[224] = {0};
    size_t ca = 0, ta = 0, ea = 0;
    unsigned clipped = r->frame_ns / 1000 > UINT32_MAX || r->swap_ns / 1000 > UINT32_MAX;
    for (unsigned i = 0; i < GPU_METRIC_COUNT; ++i) {
        uint64_t us = gpu_ticks_us(r->gpu.timing[i].ticks);
        clipped |= r->gpu.timing[i].calls > UINT32_MAX || us > UINT32_MAX;
        ca += (size_t)snprintf(calls + ca, sizeof(calls) - ca, "%s%u", i ? "," : "", host_slow_u32(r->gpu.timing[i].calls));
        ta += (size_t)snprintf(times + ta, sizeof(times) - ta, "%s%u", i ? "," : "", host_slow_u32(us));
    }
    for (unsigned i = 0; i < GPU_EVENT_COUNT; ++i) {
        clipped |= r->gpu.events[i] > UINT32_MAX;
        ea += (size_t)snprintf(events + ea, sizeof(events) - ea, "%s%u", i ? "," : "", host_slow_u32(r->gpu.events[i]));
    }
    host_logf_buffered(HOST_LOG_INFO,
        "slow_calls window=%u frame=%u guest=%u group=%u role=%s flags=%u clipped=%u frame_us=%u swap_us=%u values=%s",
        window, r->frame, r->guest_frame, group, role, r->flags, clipped,
        host_slow_u32(r->frame_ns / 1000), host_slow_u32(r->swap_ns / 1000), calls);
    host_logf_buffered(HOST_LOG_INFO, "slow_cpu_us window=%u frame=%u group=%u role=%s values=%s",
        window, r->frame, group, role, times);
    host_logf_buffered(HOST_LOG_INFO, "slow_events window=%u frame=%u group=%u role=%s values=%s",
        window, r->frame, group, role, events);
    uint64_t wait_us=host_io_ticks_us(r->io.wait_ticks);
    uint64_t held_us=host_io_ticks_us(r->io.held_ticks);
    uint64_t max_wait_us=host_io_ticks_us(r->io.max_wait_ticks);
    uint64_t max_held_us=host_io_ticks_us(r->io.max_held_ticks);
    unsigned io_clipped=r->io.saturated || r->io.calls>UINT32_MAX ||
        wait_us>UINT32_MAX || held_us>UINT32_MAX || max_wait_us>UINT32_MAX || max_held_us>UINT32_MAX;
    host_logf_buffered(HOST_LOG_INFO,
        "slow_io window=%u frame=%u group=%u role=%s available=%u clipped=%u scope=presentation_completed workers=unmeasured calls=%u wait_us=%u held_us=%u max_wait_us=%u max_held_us=%u",
        window,r->frame,group,role,r->io_available,io_clipped,
        host_slow_u32(r->io.calls),host_slow_u32(wait_us),host_slow_u32(held_us),
        host_slow_u32(max_wait_us),host_slow_u32(max_held_us));
    host_logf_buffered(HOST_LOG_INFO,
        "slow_memory window=%u frame=%u guest=%u role=%s valid=%u queries=%llu crc_bytes=%llu changed_pages=%llu sampled_queries=%llu sampled_us=%llu sample_stride=256",
        window,r->frame,r->guest_frame,role,r->memory.valid,
        (unsigned long long)r->memory.queries,(unsigned long long)r->memory.crc_bytes,
        (unsigned long long)r->memory.changed_pages,(unsigned long long)r->memory.sample_queries,
        (unsigned long long)(armTicksToNs(r->memory.sample_ticks)/1000));
    host_log_flush();
}

static void host_slow_log_stats(struct host_slow_frame_profile *p, uint32_t window)
{
    for (unsigned i = 0; i < HOST_SLOW_FRAME_GROUPS; ++i) {
        const struct host_slow_frame_group *g = &p->groups[i];
        if (!g->valid) continue;
        host_logf_buffered(HOST_LOG_INFO,
            "slow_group window=%u group=%u spike=%u before=%u after=%u crc_frame=slow_memory guest_frame=slow_calls gpu_elapsed=asynchronous_summary",
            window, i, g->spike.frame, g->before_valid, g->after_valid);
        if (g->before_valid) host_slow_log_record(&g->before, window, i, "before");
        host_slow_log_record(&g->spike, window, i, "spike");
        if (g->after_valid) host_slow_log_record(&g->after, window, i, "after");
        /* Each group is independently bounded below4096. */
        host_log_flush();
    }
    host_slow_frame_reset(p);
}

int host_sdl_gl_set_swap_interval(int interval)
{
	int result = SDL_GL_SetSwapInterval(interval);
    int actual = SDL_GL_GetSwapInterval();
    host_logf(HOST_LOG_INFO, "swap_interval requested=%d result=%d actual=%d", interval, result, actual);
    return result == 0;
}

int host_sdl_gl_swap_window(uint32_t window)
{
    SDL_Window *object = handle_get(window, _handle_window);

    if (object)
    {
        static uint32_t frame_count;
        static uint64_t previous_diagnostic_ns;
        static struct host_frame_profile profile;
        static struct host_slow_frame_profile slow;
        static uint64_t previous_end_ns;
        struct host_slow_frame_record record;
        struct host_frame_summary summary;
        uint64_t before_ns = armTicksToNs(armGetSystemTick());
        uint64_t after_ns;
        host_gpu_time_end();
        SDL_GL_SwapWindow(object);
        after_ns = armTicksToNs(armGetSystemTick());
        frame_count++;
        host_gpu_time_begin(frame_count + 1);
        memset(&record, 0, sizeof(record));
        record.frame = frame_count;
        record.guest_frame = host_frame_bridge_next();
        host_memory_frame_snapshot(&record.memory);
        record.io_available=host_file_io_snapshot(&record.io);
        /* The first delta includes startup GL, but its interval begins only
         * at the first SwapWindow. Exclude it from spike/context selection. */
        if (!previous_end_ns) record.flags |= HOST_SLOW_FRAME_STARTUP;
        record.frame_ns = after_ns - (previous_end_ns ? previous_end_ns : before_ns);
        record.swap_ns = after_ns - before_ns;
        previous_end_ns = after_ns;
        if (!host_gl_frame_snapshot(&record.gpu)) record.flags |= HOST_SLOW_GPU_INCOMPLETE;
        host_slow_frame_push(&slow, &record);
        if (host_frame_record(&profile, before_ns, after_ns, &summary))
        {
            uint64_t diagnostic_begin = armGetSystemTick();
            /* Format and enqueue one whole report; the native writer owns I/O.
             * Its producer cost belongs to the following frame interval. */
            host_memory_watch_log_stats(frame_count, summary.elapsed_ns);
            host_logf_buffered(HOST_LOG_INFO,
                "frame_profile frame=%u samples=%u p50_us=%llu p95_us=%llu "
                "max_us=%llu over_33ms=%u over_50ms=%u over_100ms=%u swap_mean_us=%llu "
                "max_frame=%u diagnostic_prev_us=%llu log_batches_dropped=%llu",
                frame_count, (unsigned)HOST_FRAME_PROFILE_WINDOW,
                (unsigned long long)(summary.p50_ns / 1000),
                (unsigned long long)(summary.p95_ns / 1000),
                (unsigned long long)(summary.max_ns / 1000),
                summary.over_33ms, summary.over_50ms, summary.over_100ms,
                (unsigned long long)(summary.swap_ns / HOST_FRAME_PROFILE_WINDOW / 1000),
                frame_count - HOST_FRAME_PROFILE_WINDOW + 1 + summary.max_index,
                (unsigned long long)(previous_diagnostic_ns / 1000),
                (unsigned long long)host_log_dropped_batches());
            host_gl_log_stats(frame_count);
            host_gpu_time_report(frame_count);
            host_hardware_profile_report(frame_count);
            host_network_profile_report(frame_count);
            host_graphics_validation_report(frame_count);
            host_frame_bridge_report(frame_count);
            host_audio_log_stats(frame_count);
            {
                struct host_io_all all;
                host_file_io_all_snapshot(&all);
                host_logf_buffered(HOST_LOG_INFO,
                    "io_all frame=%u cumulative=1 scope=all_guest_descriptor_threads calls=%llu wait_us=%llu held_us=%llu max_wait_us=%llu max_held_us=%llu failures=%llu read_bytes=%llu write_bytes=%llu saturated=%u",
                    frame_count, (unsigned long long)all.timing.calls,
                    (unsigned long long)host_io_ticks_us(all.timing.wait_ticks),
                    (unsigned long long)host_io_ticks_us(all.timing.held_ticks),
                    (unsigned long long)host_io_ticks_us(all.timing.max_wait_ticks),
                    (unsigned long long)host_io_ticks_us(all.timing.max_held_ticks),
                    (unsigned long long)all.failures, (unsigned long long)all.read_bytes,
                    (unsigned long long)all.write_bytes, all.timing.saturated);
                host_log_flush();
                static const char *names[]={"open","close","dup","fcntl","fstat","truncate","sync","seek","read","write","pread","pwrite"};
                for(unsigned i=0;i<HOST_IO_FAILURE_SLOTS;++i)if(all.failure[i].count) {
                    const struct host_io_failure *f=&all.failure[i];
                    host_logf_buffered(HOST_LOG_INFO,"io_failure frame=%u cumulative=1 op=%s errno=%d count=%llu overflow=%llu",
                        frame_count, f->operation<sizeof(names)/sizeof(names[0]) ? names[f->operation] : "unknown",
                        f->error,(unsigned long long)f->count,(unsigned long long)all.failure_overflow);
                    host_log_flush();
                }
            }
            host_phase_log_stats(frame_count);
            host_log_flush();
            host_extra_phase_log_stats(frame_count);
            host_log_flush();
            host_stall_log_stats(frame_count);
            host_log_flush();
            host_slow_log_stats(&slow, frame_count);
            previous_diagnostic_ns = armTicksToNs(armGetSystemTick() - diagnostic_begin);
        }
        return 1;
    }
    return 0;
}

/* ---------- events ---------- */

int host_sdl_poll_event(void *event)
{
	SDL_Event host_event;

	while (SDL_PollEvent(&host_event))
	{
		if (guest_input_event(event, &host_event))
			return 1;
		/* Ignored pointer-bearing events still own SDL2 allocations. */
		if (host_event.type == SDL_DROPFILE || host_event.type == SDL_DROPTEXT)
			SDL_free(host_event.drop.file);
		else if (host_event.type == SDL_TEXTEDITING_EXT)
			SDL_free(host_event.editExt.text);
	}
	return 0;
}

/* ---------- gamepads ---------- */

int host_sdl_get_gamepads(uint32_t *ids, int capacity)
{
	return guest_gamepads(ids, capacity);
}

uint32_t host_sdl_open_gamepad(uint32_t id)
{
	SDL_JoystickID instance = host_gamepad_id(id);
	SDL_GameController *controller;
	int index;

	if (instance < 0)
		return 0;
	/* Repeated ADDED/open calls must not acquire another SDL reference. */
	controller = SDL_GameControllerFromInstanceID(instance);
	if (!controller)
	{
		index = host_gamepad_index(id);
		if (index < 0)
			return 0;
		controller = SDL_GameControllerOpen(index);
	}

	if (controller)
		host_logf(HOST_LOG_INFO, "gamepad %u: %s", (unsigned)id,
			SDL_GameControllerName(controller));
	return handle_new(_handle_gamepad, controller);
}

uint32_t host_sdl_gamepad_from_id(uint32_t id)
{
	SDL_JoystickID instance = host_gamepad_id(id);
	SDL_GameController *controller = instance < 0 ? NULL :
		SDL_GameControllerFromInstanceID(instance);
	return handle_new(_handle_gamepad, controller);
}

int host_sdl_gamepad_axis(uint32_t gamepad, int axis)
{
	SDL_GameController *object = handle_get(gamepad, _handle_gamepad);

	return object && axis >= 0 && axis < SDL_CONTROLLER_AXIS_MAX ?
		SDL_GameControllerGetAxis(object, (SDL_GameControllerAxis)axis) : 0;
}

int host_sdl_gamepad_button(uint32_t gamepad, int button)
{
	SDL_GameController *object = handle_get(gamepad, _handle_gamepad);
	SDL_GameControllerButton mapped = host_gamepad_button(button);

	return object && mapped != SDL_CONTROLLER_BUTTON_INVALID ?
		SDL_GameControllerGetButton(object, mapped) : 0;
}

int host_sdl_gamepad_type(uint32_t gamepad)
{
	SDL_GameController *object = handle_get(gamepad, _handle_gamepad);

	return object ? guest_gamepad_type(SDL_GameControllerGetType(object)) : 0;
}

int host_sdl_rumble_gamepad(uint32_t gamepad, uint32_t low, uint32_t high,
                            uint32_t milliseconds)
{
	SDL_GameController *object = handle_get(gamepad, _handle_gamepad);

	return object ? SDL_GameControllerRumble(object, (Uint16)low,
		(Uint16)high, milliseconds) == 0 : 0;
}

/* ---------- audio ---------- */

/* SDL2 audio uses a callback model, which maps naturally to the
   Android port's audio_callback pattern. We bridge it through a
   guest thread just like the Android port does. */

struct audio_binding
{
	uint32_t handle;
	uint32_t callback;
	uint32_t userdata;
	SDL_AudioDeviceID device;
	Mutex lock;
	CondVar requested;
	CondVar done;
	int ready;
	int pending;
	Uint8 *output;
	int buffer_size;
	int cursor;
	int frame_bytes;
	Uint8 silence;
	uint64_t bytes_per_second;
	struct host_audio_profile profile;
	uint64_t callback_started,worker_started,resume_calls,request_tick;
};

/* Only the worker executing this stream's guest callback may submit PCM.
 * A handle alone must not let another stream/thread overwrite its buffer. */
static __thread struct audio_binding *active_audio_binding;

static void *audio_thread(void *context)
{
	struct audio_binding *binding = context;

	/* Attach guest TLS/stack before the device is unpaused. The pinned guest
	 * callback does no mixing or writes when additional_amount is zero. */
	host_call_guest(binding->callback, binding->userdata, binding->handle, 0, 0);
	mutexLock(&binding->lock);
	binding->ready = 1;
	condvarWakeOne(&binding->done);
	for (;;)
	{
		int requested;
		uint64_t worker_begin, worker_elapsed;
		while (!binding->pending)
			condvarWait(&binding->requested, &binding->lock);

		requested = binding->buffer_size;
		binding->worker_started++;
		mutexUnlock(&binding->lock);
		active_audio_binding = binding;
		worker_begin = armGetSystemTick();
		host_call_guest(binding->callback, binding->userdata,
			binding->handle, (uint32_t)requested, (uint32_t)requested);
		worker_elapsed = armGetSystemTick() - worker_begin;
		active_audio_binding = NULL;
		mutexLock(&binding->lock);
		binding->profile.worker_ticks += worker_elapsed;
		if (worker_elapsed > binding->profile.worker_max_ticks)
			binding->profile.worker_max_ticks = worker_elapsed;

		/* The SDL callback may return as soon as pending clears. Never retain
		 * its output pointer beyond this request, even after a partial fill. */
		binding->output = NULL;
		binding->buffer_size = 0;
		binding->pending = 0;
		condvarWakeOne(&binding->done);
	}
	return NULL;
}

static void SDLCALL sdl2_audio_callback(void *userdata, Uint8 *stream,
                                         int len)
{
	struct audio_binding *binding = userdata;
	uint64_t begin = armGetSystemTick();

	if (!stream || len <= 0)
		return;
	memset(stream, binding->silence, (size_t)len);
	mutexLock(&binding->lock);
	binding->callback_started++;
	binding->request_tick=begin;
	if (!binding->ready || binding->pending || len % binding->frame_bytes)
	{
		binding->profile.rejected_callbacks++;
		mutexUnlock(&binding->lock);
		return;
	}
	binding->output = stream;
	binding->buffer_size = len;
	binding->cursor = 0;
	binding->pending = 1;
	condvarWakeOne(&binding->requested);
	while (binding->pending)
		condvarWait(&binding->done, &binding->lock);
	host_audio_profile_complete(&binding->profile, armGetSystemTick() - begin,
		(uint64_t)len, (uint64_t)binding->cursor, binding->bytes_per_second);
	binding->cursor = 0;
	mutexUnlock(&binding->lock);
}

static void host_audio_log_stats(uint32_t frame)
{
	/* Streams persist after successful open. Failed opens have no running worker. */
	for (uint32_t id = 1; id < HANDLE_COUNT; ++id)
	{
		struct audio_binding *binding = handle_get(id, _handle_audio);
		struct host_audio_profile p;
		uint64_t started,workers,resumes,request_age;
		int ready,pending,cursor,requested;
		if (!binding) continue;
		mutexLock(&binding->lock);
		p = binding->profile;
		started=binding->callback_started;workers=binding->worker_started;resumes=binding->resume_calls;
		ready=binding->ready;pending=binding->pending;cursor=binding->cursor;requested=binding->buffer_size;
		request_age=pending ? armGetSystemTick()-binding->request_tick : 0;
		mutexUnlock(&binding->lock);
		host_logf_buffered(HOST_LOG_INFO,
			"audio_profile frame=%u stream=%u cumulative=1 callbacks=%llu requested_bytes=%llu supplied_bytes=%llu partial=%llu rejected_callbacks=%llu rejected_writes=%llu late_callbacks=%llu callback_us=%llu callback_max_us=%llu worker_us=%llu worker_max_us=%llu hardware_underruns=unmeasured",
			frame, id, (unsigned long long)p.callbacks,
			(unsigned long long)p.requested_bytes, (unsigned long long)p.supplied_bytes,
			(unsigned long long)p.partial_fills, (unsigned long long)p.rejected_callbacks,
			(unsigned long long)p.rejected_writes, (unsigned long long)p.late_callbacks,
			(unsigned long long)(armTicksToNs(p.callback_ticks) / 1000),
			(unsigned long long)(armTicksToNs(p.callback_max_ticks) / 1000),
			(unsigned long long)(armTicksToNs(p.worker_ticks) / 1000),
			(unsigned long long)(armTicksToNs(p.worker_max_ticks) / 1000));
		host_log_flush();
		host_logf_buffered(HOST_LOG_INFO,
			"audio_liveness frame=%u stream=%u resume_calls=%llu started=%llu workers=%llu ready=%d pending=%d pending_age_us=%llu cursor=%d requested=%d",
			frame,id,(unsigned long long)resumes,(unsigned long long)started,(unsigned long long)workers,
			ready,pending,(unsigned long long)(armTicksToNs(request_age)/1000),cursor,requested);
		host_log_flush();
	}
}

/* Open failures occur while the device is paused and before any worker was
 * started. SDL_CloseAudioDevice may wait for SDL's thread: hold no locks. */
static void audio_failed_open(struct audio_binding *binding)
{
	if (binding->handle)
	{
		ensure_lock();
		mutexLock(&handle_lock);
		handles[binding->handle].object = NULL;
		handles[binding->handle].type = _handle_free;
		mutexUnlock(&handle_lock);
	}
	SDL_CloseAudioDevice(binding->device);
	SDL_free(binding);
}

uint32_t host_sdl_open_audio_stream(uint32_t device_id, const void *spec_ptr,
                                    uint32_t callback, uint32_t userdata)
{
	struct audio_binding *binding;
	SDL_AudioSpec desired, obtained;
	int32_t guest_spec[3]; /* SDL3 3.4.16: format, channels, frequency */
	SDL_AudioDeviceID dev;

	(void)device_id;
	if (!spec_ptr || !callback)
		return 0;
	memcpy(guest_spec, spec_ptr, sizeof(guest_spec));
	if (guest_spec[1] <= 0 || guest_spec[1] > 255 || guest_spec[2] <= 0)
		return 0;
	/* These SDL3 PCM format values are shared with SDL2. Reject unsupported
	 * formats rather than truncating the guest's 32-bit enum into Uint16. */
	switch ((uint32_t)guest_spec[0])
	{
	case AUDIO_U8: case AUDIO_S8:
	case AUDIO_S16LSB: case AUDIO_S16MSB:
	case AUDIO_S32LSB: case AUDIO_S32MSB:
	case AUDIO_F32LSB: case AUDIO_F32MSB:
		break;
	default:
		return 0;
	}
	memset(&desired, 0, sizeof(desired));
	desired.format = (SDL_AudioFormat)guest_spec[0];
	desired.channels = (Uint8)guest_spec[1];
	desired.freq = guest_spec[2];
	desired.samples = 1024;

	binding = SDL_calloc(1, sizeof(*binding));
	if (!binding)
		return 0;

	binding->callback = callback;
	binding->userdata = userdata;
	binding->frame_bytes = SDL_AUDIO_BITSIZE(desired.format) / 8 * desired.channels;
	binding->silence = desired.format == AUDIO_U8 ? 0x80 : 0;
	binding->bytes_per_second = (uint64_t)binding->frame_bytes * desired.freq;
	mutexInit(&binding->lock);
	condvarInit(&binding->requested);
	condvarInit(&binding->done);

	desired.callback = sdl2_audio_callback;
	desired.userdata = binding;

	dev = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);
	if (dev == 0)
	{
		SDL_free(binding);
		return 0;
	}

	binding->device = dev;
	binding->handle = handle_new(_handle_audio, binding);

	if (!binding->handle || host_native_thread_create(audio_thread, binding,
		256 * 1024) != 0)
	{
		audio_failed_open(binding);
		return 0;
	}
	mutexLock(&binding->lock);
	while (!binding->ready)
		condvarWait(&binding->done, &binding->lock);
	mutexUnlock(&binding->lock);

	return binding->handle;
}

int host_sdl_put_audio_stream_data(uint32_t stream, const void *data,
                                   int length)
{
	struct audio_binding *binding = handle_get(stream, _handle_audio);

	if (!binding || active_audio_binding != binding || !data || length <= 0)
		return 0;
	mutexLock(&binding->lock);
	if (!binding->pending || !binding->output || length % binding->frame_bytes ||
	    length > binding->buffer_size - binding->cursor)
	{
		binding->profile.rejected_writes++;
		mutexUnlock(&binding->lock);
		return 0;
	}
	memcpy(binding->output + binding->cursor, data, (size_t)length);
	binding->cursor += length;
	mutexUnlock(&binding->lock);
	return 1;
}

int host_sdl_resume_audio_stream_device(uint32_t stream)
{
	struct audio_binding *binding = handle_get(stream, _handle_audio);

	if (!binding)
		return 0;
	mutexLock(&binding->lock);
	binding->resume_calls++;
	mutexUnlock(&binding->lock);
	SDL_PauseAudioDevice(binding->device, 0);
	return 1;
}

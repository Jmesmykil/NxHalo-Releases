/*
GUEST_SDL.C

The SDL3 functions the platform layer calls (sdl_platform.c, xinput_sdl.c,
dsound_sdl.c), for the guest. SDL itself runs in the host; objects it
returns (windows, contexts, gamepads, audio streams) are 64-bit pointers
there, so the guest only ever sees small integer handles that the host maps
back (host_sdl.c). Events are written by SDL straight into the guest's
buffer: SDL_Event has the same 128-byte layout in both ABIs for every event
type that carries no pointer, which covers all the platform layer reads.
*/

/* before SDL: musl's alloca.h must come first */
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <SDL3/SDL.h>

#include "guest_host.h"

/* SDL's Linux host normally supplies these primitives. In the guest, keep
their state in guest memory and back them with the guest musl runtime: its
pthread implementation gives each worker a guest-addressable stack and
thread-local state through the host thread bridge. */
struct guest_sdl_thread
{
	pthread_t native;
	SDL_ThreadFunction function;
	void *userdata;
	int status;
};

struct guest_sdl_mutex
{
	pthread_mutex_t native;
};

static void *guest_sdl_thread_start(void *opaque)
{
	struct guest_sdl_thread *thread = opaque;
	thread->status = thread->function(thread->userdata);
	return NULL;
}

SDL_Thread *SDL_CreateThreadRuntime(SDL_ThreadFunction function, const char *name, void *userdata,
	SDL_FunctionPointer begin_thread, SDL_FunctionPointer end_thread)
{
	struct guest_sdl_thread *thread;
	(void)name;
	/* The guest build's SDL_BeginThreadFunction/SDL_EndThreadFunction are
	NULL; callbacks from another ABI cannot safely be invoked by this shim. */
	if (!function || begin_thread || end_thread)
		return NULL;
	thread = calloc(1, sizeof(*thread));
	if (!thread)
		return NULL;
	thread->function = function;
	thread->userdata = userdata;
	thread->status = -1;
	if (pthread_create(&thread->native, NULL, guest_sdl_thread_start, thread) != 0)
	{
		free(thread);
		return NULL;
	}
	return (SDL_Thread *)thread;
}

void SDL_WaitThread(SDL_Thread *opaque, int *status)
{
	struct guest_sdl_thread *thread = (struct guest_sdl_thread *)opaque;
	if (!thread)
		return;
	if (pthread_join(thread->native, NULL) == 0 && status)
		*status = thread->status;
	free(thread);
}

SDL_Mutex *SDL_CreateMutex(void)
{
	struct guest_sdl_mutex *mutex = calloc(1, sizeof(*mutex));
	pthread_mutexattr_t attributes;
	if (!mutex)
		return NULL;
	if (pthread_mutexattr_init(&attributes) != 0)
	{
		free(mutex);
		return NULL;
	}
	if (pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE) != 0 ||
		pthread_mutex_init(&mutex->native, &attributes) != 0)
	{
		pthread_mutexattr_destroy(&attributes);
		free(mutex);
		return NULL;
	}
	pthread_mutexattr_destroy(&attributes);
	return (SDL_Mutex *)mutex;
}

void SDL_LockMutex(SDL_Mutex *opaque)
{
	if (opaque)
		(void)pthread_mutex_lock(&((struct guest_sdl_mutex *)opaque)->native);
}

bool SDL_TryLockMutex(SDL_Mutex *opaque)
{
	return !opaque || pthread_mutex_trylock(&((struct guest_sdl_mutex *)opaque)->native) == 0;
}

void SDL_UnlockMutex(SDL_Mutex *opaque)
{
	if (opaque)
		(void)pthread_mutex_unlock(&((struct guest_sdl_mutex *)opaque)->native);
}

void SDL_DestroyMutex(SDL_Mutex *opaque)
{
	struct guest_sdl_mutex *mutex = (struct guest_sdl_mutex *)opaque;
	if (mutex)
	{
		(void)pthread_mutex_destroy(&mutex->native);
		free(mutex);
	}
}

bool SDL_TryLockSpinlock(SDL_SpinLock *lock)
{
	return lock && __atomic_exchange_n(lock, 1, __ATOMIC_ACQUIRE) == 0;
}

void SDL_LockSpinlock(SDL_SpinLock *lock)
{
	while (lock && !SDL_TryLockSpinlock(lock))
		(void)sched_yield();
}

void SDL_UnlockSpinlock(SDL_SpinLock *lock)
{
	if (lock)
		__atomic_store_n(lock, 0, __ATOMIC_RELEASE);
}

int SDL_strcasecmp(const char *left, const char *right)
{
	return strcasecmp(left, right);
}

int SDL_strncasecmp(const char *left, const char *right, size_t count)
{
	return strncasecmp(left, right, count);
}

/* guest/runtime/guest_gl.c (generated) */
SDL_FunctionPointer guest_gl_get_proc_address(const char *name);

bool SDL_Init(SDL_InitFlags flags)
{
	return host_sdl_init(flags) != 0;
}

bool SDL_SetHint(const char *name, const char *value)
{
	return host_sdl_set_hint(name, value) != 0;
}

const char *SDL_GetError(void)
{
	static __thread char buffer[256];

	host_sdl_get_error(buffer, sizeof(buffer));
	return buffer;
}

const char *SDL_GetScancodeName(SDL_Scancode scancode)
{
	static __thread char buffer[64];

	host_sdl_scancode_name((int)scancode, buffer, sizeof(buffer));
	return buffer;
}

SDL_Scancode SDL_GetScancodeFromName(const char *name)
{
	return (SDL_Scancode)host_sdl_scancode_from_name(name);
}

Uint64 SDL_GetTicks(void)
{
	return (Uint64)host_sdl_ticks();
}

SDL_ThreadID SDL_GetCurrentThreadID(void)
{
	return (SDL_ThreadID)host_sdl_thread_id();
}

void SDL_free(void *memory)
{
	free(memory);
}

void *SDL_calloc(size_t count, size_t size)
{
	return calloc(count, size);
}

char *SDL_GetPrefPath(const char *org, const char *app)
{
	char root[512];
	size_t used;
	char *result;
	if (!app || !*app)
		return NULL;
	host_android_path(1, root, sizeof(root));
	root[sizeof(root) - 1] = 0;
	used = strlen(root);
	if (used + strlen(app) + (org && *org ? strlen(org) + 2 : 1) >= sizeof(root))
		return NULL;
	if (org && *org)
		used += (size_t)snprintf(root + used, sizeof(root) - used, "/%s", org);
	(void)mkdir(root, 0777);
	used += (size_t)snprintf(root + used, sizeof(root) - used, "/%s", app);
	(void)mkdir(root, 0777);
	used = strlen(root);
	result = malloc(used + 2);
	if (!result)
		return NULL;
	memcpy(result, root, used);
	if (!used || root[used - 1] != '/') result[used++] = '/';
	result[used] = 0;
	return result;
}

bool SDL_CreateDirectory(const char *path)
{
	struct stat info;
	if (!path || !*path)
		return false;
	if (mkdir(path, 0777) == 0 || errno == EEXIST)
		return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
	return false;
}

bool SDL_OpenURL(const char *url)
{
	return host_sdl_open_url(url) != 0;
}

bool SDL_ShowMessageBox(const SDL_MessageBoxData *data, int *buttonid)
{
	Uint32 flags[8], texts[8];
	int ids[8], choice, i;
	if (!data || data->numbuttons < 0 || data->numbuttons > 8 || (data->numbuttons && !data->buttons))
		return false;
	for (i = 0; i < data->numbuttons; i++) {
		flags[i] = data->buttons[i].flags;
		ids[i] = data->buttons[i].buttonID;
		texts[i] = (Uint32)(uintptr_t)data->buttons[i].text;
	}
	choice = host_sdl_show_message_box((unsigned int)data->flags, data->title, data->message,
		data->numbuttons, flags, ids, texts);
	if (choice < 0) return false;
	if (buttonid) *buttonid = choice;
	return true;
}

/* ---------- the clipboard (internet play's invite links, sdl_platform.c) */

bool SDL_SetClipboardText(const char *text)
{
	return host_sdl_set_clipboard_text(text) != 0;
}

char *SDL_GetClipboardText(void)
{
	char buffer[1024];

	host_sdl_get_clipboard_text(buffer, sizeof(buffer));
	return strdup(buffer);
}

bool SDL_ShowAndroidToast(const char *message, int duration, int gravity, int xoffset, int yoffset)
{
	return host_sdl_show_toast(message, duration, gravity, xoffset, yoffset) != 0;
}

/* ---------- a message for the player (sdl_platform.c): the host's own window */

bool SDL_ShowSimpleMessageBox(SDL_MessageBoxFlags flags, const char *title, const char *message, SDL_Window *window)
{
	(void)window;
	return host_sdl_show_simple_message_box((unsigned int)flags, title, message) != 0;
}

void SDL_Delay(Uint32 milliseconds)
{
	struct timespec duration;

	duration.tv_sec = milliseconds / 1000;
	duration.tv_nsec = (long)(milliseconds % 1000) * 1000000L;
	nanosleep(&duration, NULL);
}

/* ---------- video */

SDL_Window *SDL_CreateWindow(const char *title, int width, int height, SDL_WindowFlags flags)
{
	return (SDL_Window *)host_sdl_create_window(title, width, height, (long long)flags);
}

bool SDL_GetWindowSizeInPixels(SDL_Window *window, int *width, int *height)
{
	int w = 0, h = 0;

	host_sdl_window_size_in_pixels((unsigned int)window, &w, &h);
	if (width)
		*width = w;
	if (height)
		*height = h;
	return true;
}

bool SDL_SetWindowRelativeMouseMode(SDL_Window *window, bool enabled)
{
	return host_sdl_set_relative_mouse((unsigned int)window, enabled) != 0;
}

bool SDL_GL_SetAttribute(SDL_GLAttr attribute, int value)
{
	return host_sdl_gl_set_attribute((int)attribute, value) != 0;
}

SDL_GLContext SDL_GL_CreateContext(SDL_Window *window)
{
	return (SDL_GLContext)host_sdl_gl_create_context((unsigned int)window);
}

bool SDL_GL_MakeCurrent(SDL_Window *window, SDL_GLContext context)
{
	return host_sdl_gl_make_current((unsigned int)window, (unsigned int)context) != 0;
}

bool SDL_GL_SetSwapInterval(int interval)
{
	return host_sdl_gl_set_swap_interval(interval) != 0;
}

bool SDL_GL_SwapWindow(SDL_Window *window)
{
	return host_sdl_gl_swap_window((unsigned int)window) != 0;
}

SDL_FunctionPointer SDL_GL_GetProcAddress(const char *name)
{
	return guest_gl_get_proc_address(name);
}

/* ---------- events */

bool SDL_PollEvent(SDL_Event *event)
{
	SDL_Event scratch;

	return host_sdl_poll_event(event ? event : &scratch) != 0;
}

/* ---------- gamepads */

SDL_JoystickID *SDL_GetGamepads(int *count)
{
	unsigned int ids[16];
	int found = host_sdl_get_gamepads(ids, 16);
	SDL_JoystickID *result = malloc((found + 1) * sizeof(SDL_JoystickID));
	int index;

	if (!result)
		return NULL;
	for (index = 0; index < found; index++)
		result[index] = ids[index];
	result[found] = 0;
	if (count)
		*count = found;
	return result;
}

SDL_Gamepad *SDL_OpenGamepad(SDL_JoystickID id)
{
	return (SDL_Gamepad *)host_sdl_open_gamepad(id);
}

SDL_Gamepad *SDL_GetGamepadFromID(SDL_JoystickID id)
{
	return (SDL_Gamepad *)host_sdl_gamepad_from_id(id);
}

Sint16 SDL_GetGamepadAxis(SDL_Gamepad *gamepad, SDL_GamepadAxis axis)
{
	return (Sint16)host_sdl_gamepad_axis((unsigned int)gamepad, (int)axis);
}

bool SDL_GetGamepadButton(SDL_Gamepad *gamepad, SDL_GamepadButton button)
{
	return host_sdl_gamepad_button((unsigned int)gamepad, (int)button) != 0;
}

SDL_GamepadType SDL_GetGamepadType(SDL_Gamepad *gamepad)
{
	return (SDL_GamepadType)host_sdl_gamepad_type((unsigned int)gamepad);
}

bool SDL_RumbleGamepad(SDL_Gamepad *gamepad, Uint16 low, Uint16 high, Uint32 milliseconds)
{
	return host_sdl_rumble_gamepad((unsigned int)gamepad, low, high, milliseconds) != 0;
}

/* ---------- audio */

SDL_AudioStream *SDL_OpenAudioDeviceStream(SDL_AudioDeviceID device, const SDL_AudioSpec *spec,
	SDL_AudioStreamCallback callback, void *userdata)
{
	return (SDL_AudioStream *)host_sdl_open_audio_stream(device, spec, (unsigned int)callback,
		(unsigned int)userdata);
}

bool SDL_PutAudioStreamData(SDL_AudioStream *stream, const void *data, int length)
{
	return host_sdl_put_audio_stream_data((unsigned int)stream, data, length) != 0;
}

bool SDL_ResumeAudioStreamDevice(SDL_AudioStream *stream)
{
	return host_sdl_resume_audio_stream_device((unsigned int)stream) != 0;
}

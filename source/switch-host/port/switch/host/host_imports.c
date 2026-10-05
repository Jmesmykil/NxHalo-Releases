/*
HOST_IMPORTS.C — Nintendo Switch

Resolves guest import names to host functions. The guest calls the host
through a table of function pointers filled at load time; each entry is
looked up by name here.

This table mirrors the Android port's imports — the guest image is the
same, so it imports the same names.
*/

#include "host.h"
#include <string.h>
extern int posix_browser_request(const char *, const char *, const char *, char *, int, char *, int);
extern int posix_browser_private_key(const char *, unsigned char *, int);
extern int posix_browser_replace_key(const char *, const unsigned char *, int);

/* Forward declarations of all host_* functions the guest can import */

/* SDL */
extern int host_sdl_init(uint32_t flags);
extern int host_sdl_set_hint(const char *name, const char *value);
extern void host_sdl_get_error(char *buffer, uint32_t size);
extern int64_t host_sdl_ticks(void);
extern int64_t host_sdl_thread_id(void);
extern uint32_t host_sdl_create_window(const char *title, int width,
                                       int height, int64_t flags);
extern void host_sdl_window_size_in_pixels(uint32_t window, int *width,
                                           int *height);
extern int host_sdl_set_relative_mouse(uint32_t window, int enabled);
extern int host_sdl_gl_set_attribute(int attribute, int value);
extern uint32_t host_sdl_gl_create_context(uint32_t window);
extern int host_sdl_gl_make_current(uint32_t window, uint32_t context);
extern int host_sdl_gl_set_swap_interval(int interval);
extern int host_sdl_gl_swap_window(uint32_t window);
extern int host_sdl_poll_event(void *event);
extern int host_sdl_get_gamepads(uint32_t *ids, int capacity);
extern uint32_t host_sdl_open_gamepad(uint32_t id);
extern uint32_t host_sdl_gamepad_from_id(uint32_t id);
extern int host_sdl_gamepad_axis(uint32_t gamepad, int axis);
extern int host_sdl_gamepad_button(uint32_t gamepad, int button);
extern int host_sdl_gamepad_type(uint32_t gamepad);
extern int host_sdl_rumble_gamepad(uint32_t gamepad, uint32_t low,
                                   uint32_t high, uint32_t ms);
extern uint32_t host_sdl_open_audio_stream(uint32_t device, const void *spec,
                                           uint32_t callback, uint32_t userdata);
extern int host_sdl_put_audio_stream_data(uint32_t stream, const void *data,
                                          int length);
extern int host_sdl_resume_audio_stream_device(uint32_t stream);

/* GL */
extern void host_gl_get_string(uint32_t name, int index, char *buffer,
                               uint32_t size);
extern int host_gl_has_extension(const char *name);
extern uint32_t host_gl_read_buffer_word(uint32_t buffer, uint32_t offset);
extern void host_gl_fence_frame(uint32_t slot);
extern void host_gl_wait_frame(uint32_t slot);
extern void host_gl_buffer_write(uint32_t target, uint32_t offset,
                                 uint32_t size, const void *data);

/* System call dispatch */
extern long long host_syscall(long long number, long long a, long long b,
                              long long c, long long d, long long e,
                              long long f);

/* Memory watch */
extern void host_memory_watch_initialize(void);
extern void host_memory_watch_protect(uint32_t address, uint32_t size);
extern uint32_t host_memory_watch_serial(void);
extern uint32_t host_memory_watch_generation(uint32_t address, uint32_t size);
extern void host_memory_watch_prepare_write(uint32_t address, uint32_t size);
extern void host_memory_watch_forget(uint32_t address, uint32_t size);

/* Threading */
extern int host_thread_create(uint32_t guest_thread, uint32_t stack_size);
extern uint32_t host_get_tp(void);
extern void host_set_tp(uint32_t thread);

/* Paths */
extern void host_switch_path(int which, char *buffer, uint32_t size);

/* Misc */
extern void host_abort(const char *reason);
extern void host_log(int priority, const char *text);

/* POSIX File helpers (posix.h) */
struct posix_file_information;
extern int posix_stat(const char *path, struct posix_file_information *information);
extern int posix_fstat(int descriptor, struct posix_file_information *information);
extern int posix_set_file_times(const char *path, uint32_t a_sec, uint32_t a_nsec, uint32_t m_sec, uint32_t m_nsec);
extern int posix_seek(int descriptor, int32_t off_lo, int32_t off_hi, int whence, uint32_t *pos_lo, uint32_t *pos_hi);
extern int posix_truncate(int descriptor, uint32_t size_lo, uint32_t size_hi);
extern int posix_disk_space(const char *path, uint32_t *free_lo, uint32_t *free_hi, uint32_t *total_lo, uint32_t *total_hi);
extern int posix_set_read_only(const char *path, int read_only);
extern int posix_make_directory(const char *path);
extern void *posix_directory_open(const char *path);
extern int posix_directory_next(void *directory, char *name, uint32_t name_size);
extern void posix_directory_close(void *directory);
extern int posix_find_entry_case_insensitive(const char *directory, const char *name, char *result, uint32_t result_size);

/* POSIX Socket & Crypto helpers (posix.h) */
extern int posix_socket_last_error(void);
extern int posix_socket(int family, int type, int protocol);
extern int posix_socket_close(int socket);
extern int posix_socket_bind(int socket, const void *address, int address_length);
extern int posix_socket_connect(int socket, const void *address, int address_length);
extern int posix_socket_listen(int socket, int backlog);
extern int posix_socket_accept(int socket, void *address, int *address_length);
extern int posix_socket_send(int socket, const void *buffer, int length, int flags);
extern int posix_socket_sendto(int socket, const void *buffer, int length, int flags, const void *address, int address_length);
extern int posix_socket_recv(int socket, void *buffer, int length, int flags);
extern int posix_socket_recvfrom(int socket, void *buffer, int length, int flags, void *address, int *address_length);
extern int posix_socket_shutdown(int socket, int how);
extern int posix_socket_set_nonblocking(int socket, int nonblocking);
extern int posix_socket_bytes_available(int socket, uint32_t *count);
extern int posix_socket_setsockopt(int socket, int level, int name, const void *value, int length);
extern int posix_socket_getsockopt(int socket, int level, int name, void *value, int *length);
extern int posix_socket_getsockname(int socket, void *address, int *address_length);
extern int posix_socket_getpeername(int socket, void *address, int *address_length);
extern int posix_socket_select(int *read, int *read_count, int *write, int *write_count, int *error, int *error_count, int32_t to_sec, int32_t to_usec, int infinite);
extern uint32_t posix_local_ipv4_address(void);
extern void posix_random_bytes(void *buffer, uint32_t size);


extern void host_sdl_scancode_name(int, char *, uint32_t);
extern int host_sdl_scancode_from_name(const char *);
extern int host_sdl_open_url(const char *);
extern int host_sdl_show_message_box(uint32_t, const char *, const char *, int, const uint32_t *, const int *, const uint32_t *);
extern int posix_user_secret(unsigned char *, int);
extern void posix_describe_address(void *, char *, uint32_t);

/* The import table */
struct import_entry
{
	const char *name;
	void *function;
};

extern int host_sdl_set_clipboard_text(const char *);
extern void host_sdl_get_clipboard_text(char *, uint32_t);
extern int host_sdl_show_toast(const char *, int, int, int, int);
extern int host_sdl_show_simple_message_box(uint32_t, const char *, const char *);
#include "posix.h"
static const struct import_entry imports[] =
{
	{ "host_sdl_set_clipboard_text", (void *)host_sdl_set_clipboard_text },
	{ "host_sdl_get_clipboard_text", (void *)host_sdl_get_clipboard_text },
	{ "host_sdl_show_toast", (void *)host_sdl_show_toast },
	{ "host_sdl_show_simple_message_box", (void *)host_sdl_show_simple_message_box },
	{ "hostposix_descriptor_is_stream", (void *)posix_descriptor_is_stream },
	{ "hostposix_descriptors_same_file", (void *)posix_descriptors_same_file },
	{ "hostposix_socket_set_nodelay", (void *)posix_socket_set_nodelay },
	{ "hostposix_resolve_ipv4", (void *)posix_resolve_ipv4 },
	{ "hostposix_upnp_forward_udp", (void *)posix_upnp_forward_udp },
	{ "hostposix_upnp_stop_forwarding_udp", (void *)posix_upnp_stop_forwarding_udp },
	{ "hostposix_command_line_argument", (void *)posix_command_line_argument },
	{ "hostposix_process_id", (void *)posix_process_id },
	{ "hostposix_register_url_scheme", (void *)posix_register_url_scheme },
	{ "hostposix_discord_connect", (void *)posix_discord_connect },
	{ "hostposix_discord_write", (void *)posix_discord_write },
	{ "hostposix_discord_read", (void *)posix_discord_read },
	{ "hostposix_discord_close", (void *)posix_discord_close },

	/* SDL */
	{ "host_sdl_init",                    host_sdl_init },
	{ "host_sdl_set_hint",               host_sdl_set_hint },
	{ "host_sdl_get_error",              host_sdl_get_error },
	{ "host_sdl_ticks",                  host_sdl_ticks },
	{ "host_sdl_thread_id",             host_sdl_thread_id },
	{ "host_sdl_create_window",          host_sdl_create_window },
	{ "host_sdl_window_size_in_pixels",  host_sdl_window_size_in_pixels },
	{ "host_sdl_set_relative_mouse",     host_sdl_set_relative_mouse },
	{ "host_sdl_gl_set_attribute",       host_sdl_gl_set_attribute },
	{ "host_sdl_gl_create_context",      host_sdl_gl_create_context },
	{ "host_sdl_gl_make_current",        host_sdl_gl_make_current },
	{ "host_sdl_gl_set_swap_interval",   host_sdl_gl_set_swap_interval },
	{ "host_sdl_gl_swap_window",         host_sdl_gl_swap_window },
	{ "host_sdl_poll_event",             host_sdl_poll_event },
	{ "host_sdl_get_gamepads",           host_sdl_get_gamepads },
	{ "host_sdl_open_gamepad",           host_sdl_open_gamepad },
	{ "host_sdl_gamepad_from_id",        host_sdl_gamepad_from_id },
	{ "host_sdl_gamepad_axis",           host_sdl_gamepad_axis },
	{ "host_sdl_gamepad_button",         host_sdl_gamepad_button },
	{ "host_sdl_gamepad_type",           host_sdl_gamepad_type },
	{ "host_sdl_rumble_gamepad",         host_sdl_rumble_gamepad },
	{ "host_sdl_open_audio_stream",      host_sdl_open_audio_stream },
	{ "host_sdl_put_audio_stream_data",  host_sdl_put_audio_stream_data },
	{ "host_sdl_resume_audio_stream_device", host_sdl_resume_audio_stream_device },

	/* GL */
	{ "host_gl_get_string",              host_gl_get_string },
	{ "host_gl_has_extension",           host_gl_has_extension },
	{ "host_gl_read_buffer_word",        host_gl_read_buffer_word },
	{ "host_gl_fence_frame",             host_gl_fence_frame },
	{ "host_gl_wait_frame",              host_gl_wait_frame },
	{ "host_gl_buffer_write",            host_gl_buffer_write },

	/* Syscall */
	{ "host_syscall",                    host_syscall },

	/* Memory watch */
	{ "host_memory_watch_initialize",    host_memory_watch_initialize },
	{ "host_memory_watch_protect",       host_memory_watch_protect },
	{ "host_memory_watch_serial",        host_memory_watch_serial },
	{ "host_memory_watch_generation",    host_memory_watch_generation },
	{ "host_memory_watch_prepare_write", host_memory_watch_prepare_write },
	{ "host_memory_watch_forget",        host_memory_watch_forget },

	/* Threading */
	{ "host_thread_create",              host_thread_create },
	{ "host_get_tp",                     host_get_tp },
	{ "host_set_tp",                     host_set_tp },

	/* Paths (the guest calls host_android_path; we alias it) */
	{ "host_android_path",               host_switch_path },

	/* Misc & Logging */
	{ "host_exit",                       host_exit },
	{ "host_errno",                      host_errno },
	{ "host_abort",                      host_exit }, /* abort → exit */
	{ "host_logf",                       host_logf },
	{ "host_log",                        host_log },

	/* POSIX File helpers (posix_files) */
	{ "hostposix_stat",                  posix_stat },
	{ "hostposix_fstat",                 posix_fstat },
	{ "hostposix_set_file_times",        posix_set_file_times },
	{ "hostposix_seek",                  posix_seek },
	{ "hostposix_truncate",              posix_truncate },
	{ "hostposix_disk_space",            posix_disk_space },
	{ "hostposix_set_read_only",         posix_set_read_only },
	{ "hostposix_make_directory",        posix_make_directory },
	{ "hostposix_directory_open",        posix_directory_open },
	{ "hostposix_directory_next",        posix_directory_next },
	{ "hostposix_directory_close",       posix_directory_close },
	{ "hostposix_find_entry_case_insensitive", posix_find_entry_case_insensitive },

	/* POSIX Sockets & Crypto helpers (posix_net) */
	{ "hostposix_socket_last_error",     posix_socket_last_error },
	{ "hostposix_socket",                posix_socket },
	{ "hostposix_socket_close",          posix_socket_close },
	{ "hostposix_socket_bind",           posix_socket_bind },
	{ "hostposix_socket_connect",        posix_socket_connect },
	{ "hostposix_socket_listen",         posix_socket_listen },
	{ "hostposix_socket_accept",         posix_socket_accept },
	{ "hostposix_socket_send",           posix_socket_send },
	{ "hostposix_socket_sendto",         posix_socket_sendto },
	{ "hostposix_socket_recv",           posix_socket_recv },
	{ "hostposix_socket_recvfrom",       posix_socket_recvfrom },
	{ "hostposix_socket_shutdown",       posix_socket_shutdown },
	{ "hostposix_socket_set_nonblocking", posix_socket_set_nonblocking },
	{ "hostposix_socket_bytes_available", posix_socket_bytes_available },
	{ "hostposix_socket_setsockopt",     posix_socket_setsockopt },
	{ "hostposix_socket_getsockopt",     posix_socket_getsockopt },
	{ "hostposix_socket_getsockname",    posix_socket_getsockname },
	{ "hostposix_socket_getpeername",    posix_socket_getpeername },
	{ "hostposix_socket_select",         posix_socket_select },
	{ "hostposix_local_ipv4_address",    posix_local_ipv4_address },
	{ "hostposix_random_bytes",          posix_random_bytes },

	{ "hostposix_browser_request", posix_browser_request },
	{ "hostposix_browser_private_key", posix_browser_private_key },
	{ "hostposix_browser_replace_key", posix_browser_replace_key },
	{ "host_sdl_scancode_name", host_sdl_scancode_name },
	{ "host_sdl_scancode_from_name", host_sdl_scancode_from_name },
	{ "host_sdl_open_url", host_sdl_open_url },
	{ "host_sdl_show_message_box", host_sdl_show_message_box },
	{ "hostposix_user_secret", posix_user_secret },
	{ "hostposix_describe_address", posix_describe_address },
	{ NULL, NULL }
};

void *host_resolve_import(const char *name)
{
	const struct import_entry *entry;

	for (entry = imports; entry->name; entry++)
	{
		if (!strcmp(entry->name, name))
			return entry->function;
	}
	return NULL;
}

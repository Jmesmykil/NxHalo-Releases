/* OpenCE Ports Build114: 201e0f18011a30ea6d098ad249bf17eba005d653. AGPL-3.0-or-later. */
#ifndef NXHALO_HOST_DEKO_H
#define NXHALO_HOST_DEKO_H
#ifndef HALO_GUEST_WINDOW_MAXIMUM
#define HALO_GUEST_WINDOW_MAXIMUM (512u*1024u*1024u)
#endif
/* ---------- the renderer (host_main.c)

The game image, halo_guest.elf, draws with deko3d (port/switch/DEKO3D.md),
and this is always set. The host makes no EGL surface under it: the guest's
SDL window and GL context are stand-ins, so the display is left for deko3d's
swapchain. (The OpenGL-over-Mesa paths it skips are what is left of the
OpenGL image the Switch no longer builds.) */
extern int host_renderer_deko3d;

/* debug.texture_verify_compressed: each compressed texture read back out
of its image and compared with the game's bytes, and the differences logged */
extern int host_dk_verify_compressed;
/* debug.gpu_trace: the deko3d host keeps a record of
the commands it ran and where the GPU got to, and after a GPU fault writes
them and the kernel's account of the fault to the log (host_dk.c) */
extern int host_dk_trace;
/* debug.gpu_fault_test: a GPU fault made on purpose, to see the record */
extern int host_dk_fault_test;
/* debug.gpu_profile: the GPU's time by part of the frame, and what a frame
asks of it, in the log every 300 frames (host_dk.c) */
extern int host_dk_profile;
/* debug.gpu_capture: with the profile, one frame of each 300 timed draw by
draw, and the GPU's time by shader, target and kind of command logged */
extern int host_dk_capture;
/* debug.render_thread (host_dk.c): the guest's commands run on a thread of
their own, not the game's */
extern int host_dk_render_thread;
/* the high-res HUD's sheets, decoded on a thread of the host's own
(host_dk_hud.c, dk_commands.h's DK_COMMAND_HUD_PREPARE) */
struct dk_hud_sheet;
struct host_dk_hud_levels
{
	/* every level, blue first, the largest first, one after another; aligned
	and rounded (bytes) to DK_MEMBLOCK_ALIGNMENT, out of the CPU's cache */
	unsigned char *texels;
	uint32_t bytes;
	uint32_t width, height, levels;
};
extern int host_dk_hud_log;
void host_dk_hud_prepare(const struct dk_hud_sheet *list, uint32_t count);
void host_dk_hud_forget(void);
/* the guest's (host_imports.list): 1 the sheet decoded, 0 not yet (queued if
it was not), -1 never */
int host_dk_hud_ready(unsigned int asset, unsigned int png, unsigned int png_size, unsigned int tag);
/* the render thread's: the decoded levels, now its own; 0 if there are none */
int host_dk_hud_take(uint32_t asset, struct host_dk_hud_levels *levels);
int host_dk_hud_decode(const unsigned char *data, uint32_t size, struct host_dk_hud_levels *levels);
/* debug.frame_pacing: the game thread waits, as it hands a frame over, for
the present of the frame before it (host_dk_pace, from the stand-in
window's swap) */
extern int host_dk_frame_pacing;
void host_dk_pace(void);
/* debug.gpu_experiment: what-ifs that measure what part of the GPU's time a
change could take back, comma-separated: "no_compression" (render targets
without hardware compression), "no_target_barriers" (no wait before a
target change; may draw wrong), "lod_bias" (textures sampled three levels
smaller) */
extern int host_dk_no_compression, host_dk_no_target_barriers, host_dk_lod_bias;

/* host_dk.c: runs a frame's commands from the deko3d renderer's guest half
(port/switch/guest/dk_commands.h); commands is a guest address */
void host_dk_submit(uint32_t commands, uint32_t size);
/* the queue drained (a map is going, or the process is exiting) */
void host_dk_idle(void);
/* set once deko3d has presented a frame, after which its swapchain paces
the frames, not the stand-in window's swap */
extern int host_dk_presenting;
/* the highest submission (one a host_dk_submit) the GPU has finished; the
deko3d guest half's IsBusy and its locks wait against it (DEKO3D.md,
phase 3) */
uint32_t host_dk_retired(void);
/* called as each 16 MB chunk of the window is committed (host_memory.c);
under the deko3d renderer the chunk gets a deko3d memory block (phase 3) */
void host_dk_window_chunk_committed(uint64_t address, uint64_t size);
/* the deko3d device, made on the game thread (the only thread that touches
deko3d), for the shader cache's code memory (host_dk_shaders.c). It starts
the backend if the guest has not yet handed a frame over. (deko3d.h's
DkDevice, an opaque struct tag_DkDevice *; spelled out so this header needs
no deko3d.h.) */
struct tag_DkDevice *host_dk_device(void);

/* host_dk_compiler.cpp (UAM's compiler; DEKO3D.md, phase 5, step 2) or
host_dk_compiler_stub.c, when this build has no UAM: compiles GLSL into a
DKSH in memory (*dksh, *dksh_size: the caller frees it, and writes it to the
card in a batch). Returns 0 on failure. Called on the compile thread only, and locked inside,
so nothing else can ever be compiling at the same time. */
int host_dk_compile_glsl(int fragment, const char *glsl, void **dksh, size_t *dksh_size);

/* host_dk_shaders.c (DEKO3D.md, phase 5, step 3): the shader cache, called
by the guest through imports. stage: 0 vertex, 1 pixel. A shader is known by
a 64-bit hash of its key; the host never sees keys, only hashes and GLSL. */
/* a handle (1 or more) if the shader is in GPU code memory; else 0, and
*state (when state_out is a guest address) says which: 0 unknown, 1 queued
or compiling. Loads a shader whose file is on the card (~2 ms), on the game
thread, the only thread that touches deko3d. */
uint32_t host_dk_shader_find(uint32_t stage, uint64_t hash, uint32_t state_out);
/* queues the shader to be compiled in the background and written to the
card. glsl is a guest address, copied into host memory. Already queued: the
priority is raised if the new one is higher, and the GLSL is ignored (the
old copy compiles the same). Already on the card or compiled: nothing. */
void host_dk_shader_compile(uint32_t stage, uint64_t hash, uint32_t glsl, uint32_t glsl_size,
	uint32_t priority);
/* what the host has of a shader, without loading it: 0 nothing, 1 queued
or being compiled, 2 on the card, 3 loaded (the guest's startup pass) */
uint32_t host_dk_shader_known(uint32_t stage, uint64_t hash);
/* how many shaders are queued or being compiled (the guest tells the player
while there are many: dk_shaders.c) */
uint32_t host_dk_shader_pending(void);
/* how many of them are at priority or more urgent (lower): the map's, while
its load waits for them under the loading screen (dk_shaders.c) */
uint32_t host_dk_shader_pending_urgent(uint32_t priority);
/* every queued shader at priority from goes to priority to: the map played
before's, left uncompiled, when another map's load raises its own */
void host_dk_shader_priority_lower(uint32_t from, uint32_t to);
/* how many shaders on the card are queued for, or being read by, the loader
thread (the guest's preload and its wait under the loading screen) */
uint32_t host_dk_shader_loading(void);
/* a shader on the card a draw is waiting for (the first seconds of a map:
dk_shaders.c's stall_wait), to the front of the loader thread's queue, or
put there if it is not queued: read next, not after a map's thousand. 1 if it
is at the front now */
uint32_t host_dk_shader_load_first(uint32_t stage, uint64_t hash);
/* the guest's preload has asked for every shader it knows: what is left in
the cache's folder (from before the pack) is removed, a little at a time */
void host_dk_shader_preload_done(void);
/* how many loads this run read a shader from its own file in the folder (the
cache before the pack), which then moves into the pack */
uint32_t host_dk_shader_loose_loads(void);
/* the DkShader (as a const void *) a handle of find's names, or NULL: for the
backend's draws, on the game thread */
const void *host_dk_shader(uint32_t handle);
/* a loaded shader's hash, stage, code bytes and registers, for the GPU
profile; 0 if the handle names none */
int host_dk_shader_describe(uint32_t handle, uint64_t *hash, uint32_t *stage, uint32_t *code_size, uint32_t *registers);


struct host_dk_frame_report
{
	uint32_t submit_wait_us, streams;
	uint32_t acquire_max_us, fence_max_us;
	uint32_t presents;
	uint32_t gpu_last_us, gpu_average_us;
};
void host_dk_frame_report(struct host_dk_frame_report *report);
unsigned int host_dk_output_height(void);
unsigned int host_dk_gpu_frame_us(void);
void host_sd_lock(void);
void host_sd_unlock(void);
const char *host_data_root(void);
void host_thread_place_on_helper_core(void);
void host_debug_thread_backtrace(uint32_t handle, const char *who);
uint32_t host_dk_visibility(uint32_t index);
void host_clock_cpu(unsigned int mhz);
void host_deko_prepare(void);
#endif

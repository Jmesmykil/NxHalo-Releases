/*
HOST_GL.C — Nintendo Switch

OpenGL ES for the guest. Nearly identical to the Android version — the
Switch's Tegra X1 GPU supports OpenGL ES 3.2 through the Mesa port in
devkitPro's portlibs, using the same EGL/GLES headers.

The only difference from Android is how we load GL function pointers:
we use EGL's eglGetProcAddress (same as Android) but link statically
against the portlibs mesa libraries instead of dlopen.
*/

#include "host.h"
#include "host_gpu_profile.h"
#include "host_upload_detail.h"
#include <switch.h>

#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* GL timing data is merged once per call. Never hold this mutex during GL,
 * guest callbacks or logging; reports contain the completed calls since the
 * previous snapshot (not an estimate of GPU execution time). */
static Mutex gpu_profile_lock;
static struct gpu_profile gpu_stats;
static struct gpu_profile gpu_frame_previous;
static struct host_upload_top upload_top;
static uint32_t gpu_completed_frames;

static void gpu_commit(const struct gpu_profile *delta)
{
    mutexLock(&gpu_profile_lock);
    gpu_merge_helpers(&gpu_stats, delta);
    mutexUnlock(&gpu_profile_lock);
}

int host_gl_frame_snapshot(struct gpu_profile *delta)
{
    int valid;
    mutexLock(&gpu_profile_lock);
    valid = gpu_delta_totals(&gpu_stats, &gpu_frame_previous, delta);
    gpu_frame_previous = gpu_stats;
    gpu_completed_frames++;
    mutexUnlock(&gpu_profile_lock);
    return valid;
}

void host_gl_log_stats(uint32_t frame)
{
    struct gpu_profile p;
    struct host_upload_top details;
    static const char *names[GPU_METRIC_COUNT] = {
        "read", "read_map", "wait", "write", "write_map", "write_copy",
        "write_unmap", "write_fallback", "compile", "link", "shader_query",
        "program_query", "texture_upload", "buffer_subdata", "blit",
        "draw_arrays", "draw_elements", "draw_base_vertex"
    };
    mutexLock(&gpu_profile_lock);
    gpu_snapshot_reset(&gpu_stats, &p);
    details=upload_top; memset(&upload_top,0,sizeof(upload_top));
    memset(&gpu_frame_previous, 0, sizeof(gpu_frame_previous));
    mutexUnlock(&gpu_profile_lock);
    /* Reporting only. Helper totals include their subphases; do not add them.
     * Direct rows describe resolved guest calls, excluding helper fallback
     * glBufferSubData. Omit inactive direct rows to keep the batch bounded. */
    for (unsigned i = 0; i < GPU_METRIC_COUNT; i++) {
        if (i >= GPU_COMPILE && !p.timing[i].calls) continue;
        host_logf_buffered(HOST_LOG_INFO, "gpu frame=%u op=%s calls=%llu us=%llu max_us=%llu",
            frame, names[i], (unsigned long long)p.timing[i].calls,
            (unsigned long long)gpu_ticks_us(p.timing[i].ticks),
            (unsigned long long)gpu_ticks_us(p.timing[i].max_ticks));
    }
    host_logf_buffered(HOST_LOG_INFO,
        "gpu frame=%u read_map_failed=%llu read_unmap_failed=%llu write_map_failed=%llu "
        "write_unmap_failed=%llu wait_already=%llu wait_satisfied=%llu wait_timeout=%llu "
        "wait_failed=%llu wait_other=%llu wait_skipped=%llu write_bytes=%llu",
        frame, (unsigned long long)p.events[GPU_READ_MAP_FAILED],
        (unsigned long long)p.events[GPU_READ_UNMAP_FAILED],
        (unsigned long long)p.events[GPU_WRITE_MAP_FAILED],
        (unsigned long long)p.events[GPU_WRITE_UNMAP_FAILED],
        (unsigned long long)p.events[GPU_WAIT_ALREADY],
        (unsigned long long)p.events[GPU_WAIT_SATISFIED],
        (unsigned long long)p.events[GPU_WAIT_TIMEOUT],
        (unsigned long long)p.events[GPU_WAIT_FAILED],
        (unsigned long long)p.events[GPU_WAIT_OTHER],
        (unsigned long long)p.events[GPU_WAIT_SKIPPED],
        (unsigned long long)p.events[GPU_WRITE_BYTES]);
    /* Give detail rows their own bounded batch; no driver hot-path I/O. */
    host_log_flush();
    host_logf_buffered(HOST_LOG_INFO,
        "gpu_validation frame=%u compile_checks=%llu compile_failed_checks=%llu link_checks=%llu link_failed_checks=%llu scope=queried_status_only",
        frame, (unsigned long long)p.events[GPU_COMPILE_STATUS_CHECKS],
        (unsigned long long)p.events[GPU_COMPILE_STATUS_FAILED],
        (unsigned long long)p.events[GPU_LINK_STATUS_CHECKS],
        (unsigned long long)p.events[GPU_LINK_STATUS_FAILED]);
    host_log_flush();
    for(unsigned kind=0;kind<2;kind++)for(unsigned i=0;i<3;i++) {
        const struct host_upload_detail *d=kind ? &details.buffer[i] : &details.texture[i];
        if(!d->ticks)continue;
        host_logf_buffered(HOST_LOG_INFO,
            "upload_detail window=%u frame=%u class=%s kind=%u target=%u us=%llu level=%d width=%d height=%d format=%u offset=%lld bytes=%lld pixels=%d",
            frame,d->frame,kind ? "buffer" : "texture",d->kind,d->target,
            (unsigned long long)gpu_ticks_us(d->ticks),d->level,d->width,d->height,d->format,
            (long long)d->offset,(long long)d->bytes,d->pixels);
    }
    host_log_flush();

}

/* Direct GL call timing: preserve the driver ABI, arguments and outputs.
 * Resolution is completed before the guest uses these stable wrappers.
 * No driver call runs under gpu_profile_lock, and no call performs I/O or
 * allocates a profiling record. Query timing includes driver status waits.
 * Aggregation overhead is excluded here but remains in frame intervals. */
static void gpu_commit_call(enum gpu_metric metric, uint64_t begin)
{
    uint64_t end = armGetSystemTick();
    mutexLock(&gpu_profile_lock);
    gpu_record(&gpu_stats, metric, begin, end);
    mutexUnlock(&gpu_profile_lock);
}

static void gpu_commit_upload(enum gpu_metric metric,uint64_t begin,struct host_upload_detail row)
{
    uint64_t end=armGetSystemTick();
    row.ticks=end-begin;
    mutexLock(&gpu_profile_lock);
    row.frame=gpu_completed_frames+1;
    gpu_record(&gpu_stats,metric,begin,end);
    host_upload_remember(metric==GPU_BUFFER_SUBDATA ? upload_top.buffer : upload_top.texture,row);
    mutexUnlock(&gpu_profile_lock);
}

static PFNGLCOMPILESHADERPROC driver_compile_shader;
static PFNGLLINKPROGRAMPROC driver_link_program;
static PFNGLGETSHADERIVPROC driver_get_shaderiv;
static PFNGLGETPROGRAMIVPROC driver_get_programiv;
static PFNGLTEXIMAGE2DPROC driver_tex_image_2d;
static PFNGLTEXSUBIMAGE2DPROC driver_tex_sub_image_2d;
static PFNGLCOMPRESSEDTEXIMAGE2DPROC driver_compressed_tex_image_2d;
static PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC driver_compressed_tex_sub_image_2d;
static PFNGLBUFFERSUBDATAPROC driver_buffer_sub_data;
static PFNGLBLITFRAMEBUFFERPROC driver_blit_framebuffer;
static PFNGLDRAWARRAYSPROC driver_draw_arrays;
static PFNGLDRAWELEMENTSPROC driver_draw_elements;
static PFNGLDRAWELEMENTSBASEVERTEXPROC driver_draw_base_vertex;

static void GL_APIENTRY profile_compile_shader(GLuint shader)
{
    uint64_t begin = armGetSystemTick();
    driver_compile_shader(shader);
    gpu_commit_call(GPU_COMPILE, begin);
}

static void GL_APIENTRY profile_link_program(GLuint program)
{
    uint64_t begin = armGetSystemTick();
    driver_link_program(program);
    gpu_commit_call(GPU_LINK, begin);
}

static void GL_APIENTRY profile_get_shaderiv(GLuint shader, GLenum pname, GLint *params)
{
    uint64_t begin = armGetSystemTick();
    driver_get_shaderiv(shader, pname, params);
    gpu_commit_call(GPU_SHADER_QUERY, begin);
    if (pname == GL_COMPILE_STATUS && params) {
        mutexLock(&gpu_profile_lock);
        gpu_stats.events[GPU_COMPILE_STATUS_CHECKS]++;
        if (*params == GL_FALSE) gpu_stats.events[GPU_COMPILE_STATUS_FAILED]++;
        mutexUnlock(&gpu_profile_lock);
    }
}

static void GL_APIENTRY profile_get_programiv(GLuint program, GLenum pname, GLint *params)
{
    uint64_t begin = armGetSystemTick();
    driver_get_programiv(program, pname, params);
    gpu_commit_call(GPU_PROGRAM_QUERY, begin);
    if (pname == GL_LINK_STATUS && params) {
        mutexLock(&gpu_profile_lock);
        gpu_stats.events[GPU_LINK_STATUS_CHECKS]++;
        if (*params == GL_FALSE) gpu_stats.events[GPU_LINK_STATUS_FAILED]++;
        mutexUnlock(&gpu_profile_lock);
    }
}

static void GL_APIENTRY profile_tex_image_2d(GLenum target, GLint level, GLint internalformat,
    GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels)
{
    uint64_t begin = armGetSystemTick();
    driver_tex_image_2d(target, level, internalformat, width, height, border, format, type, pixels);
    gpu_commit_upload(GPU_TEXTURE_UPLOAD,begin,(struct host_upload_detail){.kind=0,.target=target,.level=level,.width=width,.height=height,.format=format,.pixels=pixels!=NULL,.bytes=-1});
}

static void GL_APIENTRY profile_tex_sub_image_2d(GLenum target, GLint level, GLint xoffset,
    GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels)
{
    uint64_t begin = armGetSystemTick();
    driver_tex_sub_image_2d(target, level, xoffset, yoffset, width, height, format, type, pixels);
    gpu_commit_upload(GPU_TEXTURE_UPLOAD,begin,(struct host_upload_detail){.kind=1,.target=target,.level=level,.width=width,.height=height,.format=format,.pixels=pixels!=NULL,.bytes=-1});
}

static void GL_APIENTRY profile_compressed_tex_image_2d(GLenum target, GLint level,
    GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei image_size,
    const void *data)
{
    uint64_t begin = armGetSystemTick();
    driver_compressed_tex_image_2d(target, level, internalformat, width, height, border, image_size, data);
    gpu_commit_upload(GPU_TEXTURE_UPLOAD,begin,(struct host_upload_detail){.kind=2,.target=target,.level=level,.width=width,.height=height,.format=internalformat,.pixels=data!=NULL,.bytes=image_size});
}

static void GL_APIENTRY profile_compressed_tex_sub_image_2d(GLenum target, GLint level,
    GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format,
    GLsizei image_size, const void *data)
{
    uint64_t begin = armGetSystemTick();
    driver_compressed_tex_sub_image_2d(target, level, xoffset, yoffset, width, height, format, image_size, data);
    gpu_commit_upload(GPU_TEXTURE_UPLOAD,begin,(struct host_upload_detail){.kind=3,.target=target,.level=level,.width=width,.height=height,.format=format,.pixels=data!=NULL,.bytes=image_size});
}

static void GL_APIENTRY profile_buffer_sub_data(GLenum target, GLintptr offset,
    GLsizeiptr size, const void *data)
{
    uint64_t begin = armGetSystemTick();
    driver_buffer_sub_data(target, offset, size, data);
    gpu_commit_upload(GPU_BUFFER_SUBDATA,begin,(struct host_upload_detail){.kind=0,.target=target,.offset=offset,.bytes=size,.pixels=data!=NULL});
}

static void GL_APIENTRY profile_blit_framebuffer(GLint src_x0, GLint src_y0, GLint src_x1,
    GLint src_y1, GLint dst_x0, GLint dst_y0, GLint dst_x1, GLint dst_y1,
    GLbitfield mask, GLenum filter)
{
    uint64_t begin = armGetSystemTick();
    driver_blit_framebuffer(src_x0, src_y0, src_x1, src_y1, dst_x0, dst_y0, dst_x1, dst_y1, mask, filter);
    gpu_commit_call(GPU_BLIT, begin);
}

/* CPU elapsed submission time, not GPU execution time. */
static void GL_APIENTRY profile_draw_arrays(GLenum mode, GLint first, GLsizei count)
{
    uint64_t begin = armGetSystemTick();
    driver_draw_arrays(mode, first, count);
    gpu_commit_call(GPU_DRAW_ARRAYS, begin);
}
static void GL_APIENTRY profile_draw_elements(GLenum mode, GLsizei count, GLenum type, const void *indices)
{
    uint64_t begin = armGetSystemTick();
    driver_draw_elements(mode, count, type, indices);
    gpu_commit_call(GPU_DRAW_ELEMENTS, begin);
}
static void GL_APIENTRY profile_draw_base_vertex(GLenum mode, GLsizei count, GLenum type,
    const void *indices, GLint basevertex)
{
    uint64_t begin = armGetSystemTick();
    driver_draw_base_vertex(mode, count, type, indices, basevertex);
    gpu_commit_call(GPU_DRAW_BASE_VERTEX, begin);
}

void *host_gl_resolve(const char *name)
{
	/* On Switch, GL functions are statically linked through portlibs.
	   eglGetProcAddress still works for extensions. */
	void *function = (void *)eglGetProcAddress(name);
	if (!function) return NULL;
    /* A repeated successful resolution keeps the first driver pointer. A
     * failed resolution still returns NULL, even if an earlier one worked. */
#define PROFILE_GL(symbol, type, driver, wrapper) \
    if (!strcmp(name, #symbol)) { \
        if (!(driver)) (driver) = (type)function; \
        return (void *)(wrapper); \
    }
    PROFILE_GL(glCompileShader, PFNGLCOMPILESHADERPROC, driver_compile_shader, profile_compile_shader)
    PROFILE_GL(glLinkProgram, PFNGLLINKPROGRAMPROC, driver_link_program, profile_link_program)
    PROFILE_GL(glGetShaderiv, PFNGLGETSHADERIVPROC, driver_get_shaderiv, profile_get_shaderiv)
    PROFILE_GL(glGetProgramiv, PFNGLGETPROGRAMIVPROC, driver_get_programiv, profile_get_programiv)
    PROFILE_GL(glTexImage2D, PFNGLTEXIMAGE2DPROC, driver_tex_image_2d, profile_tex_image_2d)
    PROFILE_GL(glTexSubImage2D, PFNGLTEXSUBIMAGE2DPROC, driver_tex_sub_image_2d, profile_tex_sub_image_2d)
    PROFILE_GL(glCompressedTexImage2D, PFNGLCOMPRESSEDTEXIMAGE2DPROC, driver_compressed_tex_image_2d, profile_compressed_tex_image_2d)
    PROFILE_GL(glCompressedTexSubImage2D, PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC, driver_compressed_tex_sub_image_2d, profile_compressed_tex_sub_image_2d)
    PROFILE_GL(glBufferSubData, PFNGLBUFFERSUBDATAPROC, driver_buffer_sub_data, profile_buffer_sub_data)
    PROFILE_GL(glBlitFramebuffer, PFNGLBLITFRAMEBUFFERPROC, driver_blit_framebuffer, profile_blit_framebuffer)
    PROFILE_GL(glDrawArrays, PFNGLDRAWARRAYSPROC, driver_draw_arrays, profile_draw_arrays)
    PROFILE_GL(glDrawElements, PFNGLDRAWELEMENTSPROC, driver_draw_elements, profile_draw_elements)
    PROFILE_GL(glDrawElementsBaseVertex, PFNGLDRAWELEMENTSBASEVERTEXPROC, driver_draw_base_vertex, profile_draw_base_vertex)
#undef PROFILE_GL
	return function;
}

void host_gl_get_string(uint32_t name, int index, char *buffer, uint32_t size)
{
	const GLubyte *text = index >= 0 ? glGetStringi(name, (GLuint)index)
	                                 : glGetString(name);
	if (!size)
		return;
	buffer[0] = 0;
	if (text)
	{
		strncpy(buffer, (const char *)text, size - 1);
		buffer[size - 1] = 0;
	}
}

int host_gl_has_extension(const char *name)
{
	GLint count = 0, index;

	glGetIntegerv(GL_NUM_EXTENSIONS, &count);
	for (index = 0; index < count; index++)
	{
		const char *extension =
			(const char *)glGetStringi(GL_EXTENSIONS, (GLuint)index);
		if (extension && !strcmp(extension, name))
			return 1;
	}
	return 0;
}

/* Query the actual current context once at startup. Linked symbols or driver
 * extension strings alone do not prove usable program-binary formats. */
void host_gl_log_capabilities(void)
{
	const char *version = (const char *)glGetString(GL_VERSION);
	const char *vendor = (const char *)glGetString(GL_VENDOR);
	const char *renderer = (const char *)glGetString(GL_RENDERER);
	const char *language = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
	int major = 0, minor = 0;
	int es = version && strstr(version, "OpenGL ES") == version;
	GLint count = 0;

	host_logf(HOST_LOG_INFO, "GL capabilities: vendor=%s renderer=%s version=%s GLSL=%s",
		vendor ? vendor : "unknown", renderer ? renderer : "unknown",
		version ? version : "unknown", language ? language : "unknown");
	if (version)
	{
		if (es)
			sscanf(version, "OpenGL ES %d.%d", &major, &minor);
		else
			sscanf(version, "%d.%d", &major, &minor);
	}
	/* The renderer needs ES3 or desktop GL3+. Avoid invalid queries if a
	 * fallback ES2 context was created and will subsequently be rejected. */
	if (major < 3)
	{
		host_logf(HOST_LOG_INFO, "GL capabilities: binary format query unavailable for this context");
		return;
	}
	if (es || major > 4 || (major == 4 && minor >= 1) ||
		host_gl_has_extension("GL_ARB_get_program_binary"))
	{
		glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &count);
		host_logf(HOST_LOG_INFO, "GL capabilities: program_binary_formats=%d", (int)count);
		if (count > 0 && count <= 128)
		{
			GLint *formats = malloc((size_t)count * sizeof(*formats));
			if (formats)
			{
				GLint index;
				glGetIntegerv(GL_PROGRAM_BINARY_FORMATS, formats);
				for (index = 0; index < count; index++)
					host_logf(HOST_LOG_INFO, "GL capabilities: program_binary_format[%d]=0x%x",
						(int)index, (unsigned int)formats[index]);
				free(formats);
			}
		}
	}
	else
		host_logf(HOST_LOG_INFO, "GL capabilities: program binaries unsupported by this context");
	host_logf(HOST_LOG_INFO, "GL capabilities: parallel_compile KHR=%d ARB=%d program_binary OES=%d ARB=%d",
		host_gl_has_extension("GL_KHR_parallel_shader_compile"),
		host_gl_has_extension("GL_ARB_parallel_shader_compile"),
		host_gl_has_extension("GL_OES_get_program_binary"),
		host_gl_has_extension("GL_ARB_get_program_binary"));
}

/* One 32-bit word of a buffer object (visibility test counters) */
uint32_t host_gl_read_buffer_word(uint32_t buffer, uint32_t offset)
{
    struct gpu_profile p = {0};
    uint64_t begin = armGetSystemTick(), map_begin, map_end;
    uint32_t value = 0;
    GLint previous = 0;
    const void *mapping;
    glGetIntegerv(GL_ATOMIC_COUNTER_BUFFER_BINDING, &previous);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, buffer);
    map_begin = armGetSystemTick();
    mapping = glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, offset, sizeof(value), GL_MAP_READ_BIT);
    map_end = armGetSystemTick();
    gpu_record(&p, GPU_READ_MAP, map_begin, map_end);
    if (mapping) {
        memcpy(&value, mapping, sizeof(value));
        if (glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER) == GL_FALSE)
            p.events[GPU_READ_UNMAP_FAILED]++;
    } else p.events[GPU_READ_MAP_FAILED]++;
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, (GLuint)previous);
    gpu_record(&p, GPU_READ_TOTAL, begin, armGetSystemTick());
    gpu_commit(&p);
    return value;
}

/* Frame fences for streaming vertex/index buffers */
#define FRAME_FENCE_SLOTS 8

static GLsync frame_fences[FRAME_FENCE_SLOTS];

void host_gl_fence_frame(uint32_t slot)
{
	if (slot >= FRAME_FENCE_SLOTS)
		return;
	if (frame_fences[slot])
		glDeleteSync(frame_fences[slot]);
	frame_fences[slot] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

void host_gl_wait_frame(uint32_t slot)
{
    struct gpu_profile p = {0};
    if (slot >= FRAME_FENCE_SLOTS || !frame_fences[slot]) {
        p.events[GPU_WAIT_SKIPPED]++;
        gpu_commit(&p);
        return;
    }
    uint64_t begin = armGetSystemTick();
    GLenum result = glClientWaitSync(frame_fences[slot], GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000ULL);
    gpu_record(&p, GPU_WAIT, begin, armGetSystemTick());
    switch (result) {
        case GL_ALREADY_SIGNALED: p.events[GPU_WAIT_ALREADY]++; break;
        case GL_CONDITION_SATISFIED: p.events[GPU_WAIT_SATISFIED]++; break;
        case GL_TIMEOUT_EXPIRED: p.events[GPU_WAIT_TIMEOUT]++; break;
        case GL_WAIT_FAILED: p.events[GPU_WAIT_FAILED]++; break;
        default: p.events[GPU_WAIT_OTHER]++; break;
    }
    glDeleteSync(frame_fences[slot]);
    frame_fences[slot] = NULL;
    gpu_commit(&p);
}

/* Unsynchronized buffer write (same optimization as Android for Mali;
   Tegra's GL driver benefits too) */
void host_gl_buffer_write(uint32_t target, uint32_t offset, uint32_t size,
                          const void *data)
{
    struct gpu_profile p = {0};
    uint64_t begin = armGetSystemTick(), map_end, copy_end, unmap_end;
    void *mapping = glMapBufferRange(target, offset, size,
        GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_INVALIDATE_RANGE_BIT);
    map_end = armGetSystemTick();
    gpu_record(&p, GPU_WRITE_MAP, begin, map_end);
    p.events[GPU_WRITE_BYTES] = size;
    if (!mapping) {
        p.events[GPU_WRITE_MAP_FAILED]++;
        uint64_t fallback_begin = armGetSystemTick();
        glBufferSubData(target, offset, size, data);
        unmap_end = armGetSystemTick();
        gpu_record(&p, GPU_WRITE_FALLBACK, fallback_begin, unmap_end);
    } else {
        uint64_t copy_begin = armGetSystemTick();
        memcpy(mapping, data, size);
        copy_end = armGetSystemTick();
        gpu_record(&p, GPU_WRITE_COPY, copy_begin, copy_end);
        uint64_t unmap_begin = armGetSystemTick();
        GLboolean result = glUnmapBuffer(target);
        unmap_end = armGetSystemTick();
        gpu_record(&p, GPU_WRITE_UNMAP, unmap_begin, unmap_end);
        if (result == GL_FALSE) p.events[GPU_WRITE_UNMAP_FAILED]++;
    }
    gpu_record(&p, GPU_WRITE_TOTAL, begin, unmap_end);
    gpu_commit(&p);
}

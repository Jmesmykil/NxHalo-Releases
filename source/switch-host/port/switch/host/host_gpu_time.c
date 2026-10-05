#ifndef HOST_GPU_TIME_TEST
#include "host.h"
#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <SDL2/SDL.h>
#endif
#include <stdint.h>
#include <string.h>

/* Asynchronous GPU elapsed intervals, excluding presentation. Never wait for a
 * query result. Ring exhaustion is reported as missing coverage. */
#define TIMER_ELAPSED 0x88BF
#define TIMER_DISJOINT 0x8FBB
#define TIMER_SLOTS 16
static void (*timer_gen)(GLsizei, GLuint *);
static void (*timer_begin)(GLenum, GLuint);
static void (*timer_end)(GLenum);
static void (*timer_available)(GLuint, GLenum, GLuint *);
static void (*timer_result)(GLuint, GLenum, GLuint64 *);
static struct { GLuint id; uint32_t frame; int pending; } timer_slots[TIMER_SLOTS];
static int timer_initialized, timer_supported, timer_disjoint, timer_active = -1;
static uint64_t timer_samples, timer_ns, timer_max_ns, timer_missing, timer_invalid;
static uint32_t timer_max_frame;
static void timer_init(void)
{
    timer_initialized = 1;
    timer_disjoint = SDL_GL_ExtensionSupported("GL_EXT_disjoint_timer_query");
    timer_supported = timer_disjoint || SDL_GL_ExtensionSupported("GL_ARB_timer_query");
    if (!timer_supported) return;
#define TIMER_PROC(var, core, ext) do { \
    *(void **)(&var) = (void *)eglGetProcAddress(core); \
    if (!var) *(void **)(&var) = (void *)eglGetProcAddress(ext); \
} while (0)
    TIMER_PROC(timer_gen,"glGenQueries","glGenQueriesEXT");
    TIMER_PROC(timer_begin,"glBeginQuery","glBeginQueryEXT");
    TIMER_PROC(timer_end,"glEndQuery","glEndQueryEXT");
    TIMER_PROC(timer_available,"glGetQueryObjectuiv","glGetQueryObjectuivEXT");
    TIMER_PROC(timer_result,"glGetQueryObjectui64v","glGetQueryObjectui64vEXT");
#undef TIMER_PROC
    if (!timer_gen || !timer_begin || !timer_end || !timer_available || !timer_result) {
        timer_supported = 0; return;
    }
    GLuint ids[TIMER_SLOTS] = {0}; timer_gen(TIMER_SLOTS,ids);
    for (unsigned i=0;i<TIMER_SLOTS;++i) {
        timer_slots[i].id=ids[i];
        if (!ids[i]) timer_supported=0;
    }
}
void host_gpu_time_end(void)
{
    if (timer_active >= 0) {
        timer_end(TIMER_ELAPSED);
        timer_slots[timer_active].pending=1;
        timer_active=-1;
    }
}
void host_gpu_time_begin(uint32_t frame)
{
    if (!timer_initialized) timer_init();
    if (!timer_supported) return;
    GLint disjoint=0;
    if (timer_disjoint) glGetIntegerv(TIMER_DISJOINT,&disjoint);
    int free_slot=-1;
    for (unsigned i=0;i<TIMER_SLOTS;++i) {
        if (timer_slots[i].pending) {
            GLuint ready=0;
            if (disjoint) { timer_slots[i].pending=0; timer_invalid++; }
            else {
                timer_available(timer_slots[i].id,GL_QUERY_RESULT_AVAILABLE,&ready);
                if (ready) {
                    GLuint64 ns=0; timer_result(timer_slots[i].id,GL_QUERY_RESULT,&ns);
                    timer_slots[i].pending=0; timer_samples++; timer_ns+=ns;
                    if (ns>timer_max_ns) { timer_max_ns=ns; timer_max_frame=timer_slots[i].frame; }
                }
            }
        }
        if (!timer_slots[i].pending && free_slot<0) free_slot=(int)i;
    }
    if (disjoint || free_slot<0) { timer_missing++; return; }
    timer_slots[free_slot].frame=frame;
    timer_begin(TIMER_ELAPSED,timer_slots[free_slot].id);
    timer_active=free_slot;
}
void host_gpu_time_report(uint32_t frame)
{
    unsigned pending=0;
    for(unsigned i=0;i<TIMER_SLOTS;++i) pending+=timer_slots[i].pending;
    host_logf_buffered(HOST_LOG_INFO,
        "gpu_elapsed frame=%u supported=%d cumulative=1 scope=between_presentations samples=%llu total_us=%llu max_us=%llu max_frame=%u missing=%llu invalid_disjoint=%llu pending=%u",
        frame,timer_supported,(unsigned long long)timer_samples,
        (unsigned long long)(timer_ns/1000),(unsigned long long)(timer_max_ns/1000),
        timer_max_frame,(unsigned long long)timer_missing,(unsigned long long)timer_invalid,pending);
    host_log_flush();
}

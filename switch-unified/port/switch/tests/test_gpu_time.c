#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef unsigned GLenum, GLuint;
typedef int GLint, GLsizei;
typedef uint64_t GLuint64;
#define GL_QUERY_RESULT_AVAILABLE 1
#define GL_QUERY_RESULT 2
#define HOST_LOG_INFO 4
#define HOST_GPU_TIME_TEST
static int ready, disjoint, active, results, supported=1;
static int SDL_GL_ExtensionSupported(const char *s) { (void)s; return supported; }
static void glGetIntegerv(GLenum e, GLint *v) { (void)e; *v=disjoint; }
static void gen(GLsizei n, GLuint *p) { for(int i=0;i<n;i++)p[i]=(GLuint)i+1; }
static void begin(GLenum e, GLuint id) { (void)e; assert(!active && id); active=1; }
static void end(GLenum e) { (void)e; assert(active); active=0; }
static void available(GLuint id, GLenum e, GLuint *v) { (void)id; assert(e==1); *v=ready; }
static void result(GLuint id, GLenum e, GLuint64 *v) { (void)id; assert(ready && e==2); results++; *v=2000000; }
static void *eglGetProcAddress(const char *s) {
    if(!strcmp(s,"glGenQueries"))return (void *)gen;
    if(!strcmp(s,"glBeginQuery"))return (void *)begin;
    if(!strcmp(s,"glEndQuery"))return (void *)end;
    if(!strcmp(s,"glGetQueryObjectuiv"))return (void *)available;
    if(!strcmp(s,"glGetQueryObjectui64v"))return (void *)result;
    return NULL;
}
static void host_logf_buffered(int p,const char *f,...) { (void)p;(void)f; }
static void host_log_flush(void) {}
#include "../host/host_gpu_time.c"
int main(void)
{
    for(unsigned i=1;i<=16;i++) {host_gpu_time_begin(i);assert(active);host_gpu_time_end();}
    host_gpu_time_begin(17);assert(!active && !results && timer_missing==1);
    ready=1;host_gpu_time_begin(18);assert(active && results==16 && timer_samples==16);
    assert(timer_ns==32000000 && timer_max_ns==2000000);
    host_gpu_time_end();ready=0;disjoint=1;
    host_gpu_time_begin(19);assert(!active && timer_invalid==1 && timer_missing==2);
    timer_initialized=0;supported=0;disjoint=0;
    host_gpu_time_begin(20);assert(!timer_supported && !active);
    puts("GPU timer: unavailable results never read; ring exhaustion, recovery, disjoint and unsupported cases pass");
}

#include "host.h"
#include <switch.h>
#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <SDL2/SDL.h>
#include <string.h>

/* Driver validation is asynchronous. The callback only records bounded data;
 * formatting and logging happen at presentation report boundaries. */
static Mutex validation_lock;
static uint64_t messages,errors,undefined_behavior,performance;
static GLuint last_id;
static GLenum last_type,last_severity;
static char last_message[192];
static int initialized,supported;
static void GL_APIENTRY validation_message(GLenum source,GLenum type,GLuint id,
    GLenum severity,GLsizei length,const GLchar *message,const void *user)
{
    (void)source;(void)user;
    mutexLock(&validation_lock);
    messages++;
    errors+=type==GL_DEBUG_TYPE_ERROR;
    undefined_behavior+=type==GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR;
    performance+=type==GL_DEBUG_TYPE_PERFORMANCE;
    last_id=id;last_type=type;last_severity=severity;
    size_t n=0;
    if(message && length>0) {
        n=(size_t)length;
        if(n>=sizeof(last_message))n=sizeof(last_message)-1;
        for(size_t i=0;i<n;++i)last_message[i]=message[i]<' ' ? ' ' : message[i];
    }
    last_message[n]=0;
    mutexUnlock(&validation_lock);
}
void host_graphics_validation_init(void)
{
    if(initialized)return;
    initialized=1;
    supported=SDL_GL_ExtensionSupported("GL_KHR_debug");
    if(supported) {
        PFNGLDEBUGMESSAGECALLBACKPROC callback=(PFNGLDEBUGMESSAGECALLBACKPROC)eglGetProcAddress("glDebugMessageCallback");
        if(!callback)callback=(PFNGLDEBUGMESSAGECALLBACKPROC)eglGetProcAddress("glDebugMessageCallbackKHR");
        if(!callback) {supported=0;return;}
        callback(validation_message,NULL);
        glEnable(GL_DEBUG_OUTPUT);
    }
}
void host_graphics_validation_report(uint32_t frame)
{
    uint64_t m,e,u,p;GLuint id;GLenum type,severity;char text[sizeof(last_message)];
    mutexLock(&validation_lock);
    m=messages;e=errors;u=undefined_behavior;p=performance;
    id=last_id;type=last_type;severity=last_severity;memcpy(text,last_message,sizeof(text));
    mutexUnlock(&validation_lock);
    host_logf_buffered(HOST_LOG_INFO,
        "graphics_validation frame=%u supported=%d cumulative=1 messages=%llu errors=%llu undefined=%llu performance=%llu last_id=%u type=%x severity=%x",
        frame,supported,(unsigned long long)m,(unsigned long long)e,(unsigned long long)u,(unsigned long long)p,id,type,severity);
    if(text[0])host_logf_buffered(HOST_LOG_INFO,"graphics_validation_detail frame=%u message=%s",frame,text);
    host_log_flush();
}

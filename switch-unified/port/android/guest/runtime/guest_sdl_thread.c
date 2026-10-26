/* SDL synchronization remains in the guest so callbacks and pthread TLS use
 * the ILP32 ABI. The existing pthread bridge creates the host worker thread. */
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sched.h>
#include <SDL3/SDL.h>

struct SDL_Mutex { pthread_mutex_t value; };
struct SDL_Thread { pthread_t value; SDL_ThreadFunction function; void *data; };

void SDL_LockSpinlock(SDL_SpinLock *lock)
{
    while (__atomic_exchange_n(lock, 1, __ATOMIC_ACQUIRE))
        sched_yield();
}

void SDL_UnlockSpinlock(SDL_SpinLock *lock)
{
    __atomic_store_n(lock, 0, __ATOMIC_RELEASE);
}

SDL_Mutex *SDL_CreateMutex(void)
{
    SDL_Mutex *mutex = malloc(sizeof(*mutex));
    pthread_mutexattr_t attributes;
    if (!mutex) return NULL;
    if (pthread_mutexattr_init(&attributes)) { free(mutex); return NULL; }
    int result = pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE);
    if (!result) result = pthread_mutex_init(&mutex->value, &attributes);
    pthread_mutexattr_destroy(&attributes);
    if (result) { free(mutex); return NULL; }
    return mutex;
}

void SDL_LockMutex(SDL_Mutex *mutex) { if (mutex) pthread_mutex_lock(&mutex->value); }
void SDL_UnlockMutex(SDL_Mutex *mutex) { if (mutex) pthread_mutex_unlock(&mutex->value); }
void SDL_DestroyMutex(SDL_Mutex *mutex)
{
    if (mutex) { pthread_mutex_destroy(&mutex->value); free(mutex); }
}

static void *sdl_thread_run(void *argument)
{
    SDL_Thread *thread = argument;
    return (void *)(intptr_t)thread->function(thread->data);
}

SDL_Thread *SDL_CreateThreadRuntime(SDL_ThreadFunction function, const char *name,
    void *data, SDL_FunctionPointer begin, SDL_FunctionPointer end)
{
    (void)name; (void)begin; (void)end;
    if (!function) return NULL;
    SDL_Thread *thread = malloc(sizeof(*thread));
    if (!thread) return NULL;
    thread->function = function;
    thread->data = data;
    if (pthread_create(&thread->value, NULL, sdl_thread_run, thread)) {
        free(thread);
        return NULL;
    }
    return thread;
}

void SDL_WaitThread(SDL_Thread *thread, int *status)
{
    void *result = NULL;
    if (!thread) return;
    if (pthread_join(thread->value, &result)) return;
    if (status) *status = (int)(intptr_t)result;
    free(thread);
}

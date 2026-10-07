/* Routine guest and host output: bounded producers, one native IO worker. */
#include "host_log_queue.h"
#include <pthread.h>
#include <switch.h>

static struct host_log_queue log_queue;
static atomic_flag started = ATOMIC_FLAG_INIT;
static atomic_bool ready, stopping, exited;
static _Atomic uint64_t rejected;
static void (*write_callback)(const char *, size_t);

static void *log_worker(void *unused)
{
    (void)unused;
    char batch[HOST_LOG_BATCH_CAPACITY * 4];
    size_t length;
    for (;;) {
        if (host_log_queue_pop(&log_queue, batch, &length)) {
            size_t total=length;
            /* Amortize SD flushes without delaying or locking producers. */
            while (total <= sizeof(batch)-HOST_LOG_BATCH_CAPACITY &&
                   host_log_queue_pop(&log_queue,batch+total,&length))
                total+=length;
            write_callback(batch, total);
            continue;
        }
        if (atomic_load_explicit(&stopping, memory_order_acquire)) break;
        /* Polling deliberately keeps wakeup syscalls off the render producer. */
        svcSleepThread(20000000);
    }
    atomic_store_explicit(&exited, true, memory_order_release);
    return NULL;
}

bool host_log_async_start(void (*write_batch)(const char *, size_t))
{
    if (!write_batch || atomic_flag_test_and_set_explicit(&started, memory_order_relaxed))
        return false;
    pthread_attr_t attr;
    if (pthread_attr_init(&attr)) return false;
    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) {
        pthread_attr_destroy(&attr);
        return false;
    }
    host_log_queue_init(&log_queue);
    if (pthread_attr_setstacksize(&attr,128 * 1024)) {
        pthread_attr_destroy(&attr);
        return false;
    }
    write_callback = write_batch;
    pthread_t thread;
    int result = pthread_create(&thread, &attr, log_worker, NULL);
    pthread_attr_destroy(&attr);
    if (result) return false;
    atomic_store_explicit(&ready, true, memory_order_release);
    return true;
}

bool host_log_async_submit(const char *bytes, size_t length)
{
    if (!atomic_load_explicit(&ready, memory_order_acquire)) {
        atomic_fetch_add_explicit(&rejected, 1, memory_order_relaxed);
        return false;
    }
    return host_log_queue_push(&log_queue, bytes, length);
}

uint64_t host_log_async_dropped(void)
{
    return atomic_load_explicit(&rejected, memory_order_relaxed) +
           atomic_load_explicit(&log_queue.dropped, memory_order_relaxed);
}

bool host_log_async_stop(uint32_t timeout_ms)
{
    /* Producer quiescence is an explicit caller obligation. */
    if (!atomic_exchange_explicit(&ready, false, memory_order_acq_rel) &&
        !atomic_load_explicit(&stopping, memory_order_acquire)) return true;
    atomic_store_explicit(&stopping, true, memory_order_release);
    if (timeout_ms > 1000) timeout_ms = 1000;
    uint64_t begin = armGetSystemTick();
    uint64_t budget = (uint64_t)timeout_ms * 19200;
    while (!atomic_load_explicit(&exited, memory_order_acquire)) {
        if (armGetSystemTick() - begin >= budget) return false;
        svcSleepThread(1000000);
    }
    return true;
}

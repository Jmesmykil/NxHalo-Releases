#ifndef HOST_LOG_QUEUE_H
#define HOST_LOG_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
#include <string.h>

#define HOST_LOG_BATCH_CAPACITY 4096u
#define HOST_LOG_QUEUE_CAPACITY 128u
/* Enqueue must never fall back to an out-of-line atomic lock. */
_Static_assert(ATOMIC_LLONG_LOCK_FREE == 2 && ATOMIC_LONG_LOCK_FREE == 2,
               "logger requires lock-free 64-bit atomics");
_Static_assert((HOST_LOG_QUEUE_CAPACITY & (HOST_LOG_QUEUE_CAPACITY - 1)) == 0,
               "ring capacity must be a power of two");

struct host_log_slot { size_t length; char bytes[HOST_LOG_BATCH_CAPACITY]; };
struct host_log_queue {
    _Atomic uint64_t head, tail, dropped;
    atomic_flag producer;
    struct host_log_slot slots[HOST_LOG_QUEUE_CAPACITY];
};
/* One consumer; producers serialize with a single try, never a spin/wait. */
static inline void host_log_queue_init(struct host_log_queue *q)
{
    atomic_init(&q->head, 0);
    atomic_init(&q->tail, 0);
    atomic_init(&q->dropped, 0);
    atomic_flag_clear_explicit(&q->producer, memory_order_relaxed);
}
static inline bool host_log_queue_push_owned(struct host_log_queue *q,
                                       const char *bytes, size_t length)
{
    if (!bytes || !length || length > HOST_LOG_BATCH_CAPACITY) goto dropped;
    uint64_t head = atomic_load_explicit(&q->head, memory_order_relaxed);
    uint64_t tail = atomic_load_explicit(&q->tail, memory_order_acquire);
    if (head - tail >= HOST_LOG_QUEUE_CAPACITY) goto dropped;
    struct host_log_slot *slot = &q->slots[head & (HOST_LOG_QUEUE_CAPACITY - 1)];
    memcpy(slot->bytes, bytes, length);
    slot->length = length;
    atomic_store_explicit(&q->head, head + 1, memory_order_release);
    return true;
dropped:
    atomic_fetch_add_explicit(&q->dropped, 1, memory_order_relaxed);
    return false;
}
static inline bool host_log_queue_push(struct host_log_queue *q,
                                       const char *bytes, size_t length)
{
    if (atomic_flag_test_and_set_explicit(&q->producer, memory_order_acquire)) {
        atomic_fetch_add_explicit(&q->dropped, 1, memory_order_relaxed);
        return false;
    }
    bool result = host_log_queue_push_owned(q, bytes, length);
    atomic_flag_clear_explicit(&q->producer, memory_order_release);
    return result;
}
/* out must have HOST_LOG_BATCH_CAPACITY bytes; copy before releasing the slot.
 * The writer may then stall indefinitely without retaining a queue slot. */
static inline bool host_log_queue_pop(struct host_log_queue *q, char *out,
                                      size_t *length)
{
    uint64_t tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
    uint64_t head = atomic_load_explicit(&q->head, memory_order_acquire);
    if (tail == head) return false;
    struct host_log_slot *slot = &q->slots[tail & (HOST_LOG_QUEUE_CAPACITY - 1)];
    *length = slot->length;
    memcpy(out, slot->bytes, *length);
    atomic_store_explicit(&q->tail, tail + 1, memory_order_release);
    return true;
}

/* Start once, before guest producers. Callback runs on native pthread
 * TLS and must consume the supplied bytes before returning. Start failure never
 * enables a synchronous fallback. Submit copies an entire nonempty batch. */
bool host_log_async_start(void (*write_batch)(const char *, size_t));
bool host_log_async_submit(const char *bytes, size_t length);
uint64_t host_log_async_dropped(void);
/* Shutdown only, AFTER all producers have stopped. Waits at most the
 * requested polling budget (capped at 1000 ms), plus scheduler latency. False
 * leaves the detached worker and static queue alive to finish safely. */
bool host_log_async_stop(uint32_t timeout_ms);

#endif

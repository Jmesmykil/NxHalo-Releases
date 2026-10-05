#ifndef HOST_IO_PROFILE_H
#define HOST_IO_PROFILE_H
#include <stdint.h>
#include <string.h>
struct host_io_profile {
    uint64_t calls, wait_ticks, held_ticks, max_wait_ticks, max_held_ticks;
    uint32_t saturated;
};
static inline uint64_t host_io_sum(uint64_t a, uint64_t b, uint32_t *flag)
{ if (UINT64_MAX-a < b) { *flag=1; return UINT64_MAX; } return a+b; }
static inline void host_io_record(struct host_io_profile *p,uint64_t wait,uint64_t held)
{
    p->calls=host_io_sum(p->calls,1,&p->saturated);
    p->wait_ticks=host_io_sum(p->wait_ticks,wait,&p->saturated);
    p->held_ticks=host_io_sum(p->held_ticks,held,&p->saturated);
    if(wait>p->max_wait_ticks)p->max_wait_ticks=wait;
    if(held>p->max_held_ticks)p->max_held_ticks=held;
}
/* Switch counter frequency is 19.2 MHz: microseconds = ticks*5/96.
 * Divide first so even saturated tick totals cannot overflow conversion. */
static inline uint64_t host_io_ticks_us(uint64_t ticks)
{ return (ticks/96)*5 + (ticks%96)*5/96; }
#endif

#ifndef HOST_LAN_DISCOVERY_H
#define HOST_LAN_DISCOVERY_H
#include <stdint.h>
/* Input/output IPv4 words are host byte order. Limit fallback discovery to
 * the local private /24 or smaller, never all of a large routed network. */
static int host_lan_targets(uint32_t local, uint32_t mask, unsigned *cursor,
    uint32_t *targets, unsigned maximum)
{
    uint32_t hosts = ~mask;
    if (!local || !mask || (hosts & (hosts + 1)) || hosts < 3) return 0;
    if (!((local >> 24) == 10 || (local & 0xfff00000U) == 0xac100000U ||
        (local & 0xffff0000U) == 0xc0a80000U)) return 0;
    if (hosts > 255) { mask = 0xffffff00U; hosts = 255; }
    uint32_t network = local & mask;
    unsigned count = 0, visited = 0;
    while (count < maximum && visited < hosts - 1) {
        unsigned offset = 1 + (*cursor % (hosts - 1));
        *cursor = (*cursor + 1) % (hosts - 1); visited++;
        uint32_t ip = network + offset;
        if (ip != local) targets[count++] = ip;
    }
    return (int)count;
}
#endif

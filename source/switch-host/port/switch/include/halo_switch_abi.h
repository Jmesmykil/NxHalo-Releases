/*
HALO_SWITCH_ABI.H

The contract between the guest (ILP32 AArch64 game image, shared with the
Android port) and the Switch host (64-bit NRO homebrew app).

This is the Switch-specific version of halo_android_abi.h. The guest image
format and ABI are identical — the same halo_guest.elf runs on both — but
the host reserves memory and provides services through libnx instead of
Android's bionic/ART.
*/

#ifndef __HALO_SWITCH_ABI_H
#define __HALO_SWITCH_ABI_H

#include <stdint.h>

/* Re-export the guest contract (shared with Android) */
#include "../../android/include/halo_android_abi.h"

/* Switch-specific constants */

/* The Switch's page size (Horizon OS uses 4 KB pages) */
#define HALO_SWITCH_PAGE_SIZE 0x1000

/* SDL2 vs SDL3: the Switch portlibs ship SDL2 */
#define HALO_SWITCH_SDL2 1

/* NRO homebrew memory: the Switch gives homebrew ~300 MB of usable RAM.
   The guest needs:
   - 128 MB Xbox window (0x80000000-0x88000000)
   - ~16 MB guest image  (0x88000000-0x89xxxxxx)
   - ~32 MB for guest malloc arenas, thread stacks
   That leaves ~120 MB for the host (SDL2, GL driver, textures). Tight
   but feasible — SoH runs in the same envelope. */
#define HALO_SWITCH_MEMORY_BUDGET (300 * 1024 * 1024)

#endif

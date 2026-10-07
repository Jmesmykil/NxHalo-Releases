/*
HALO_NETWORK_PROFILE.H

Runtime selection of the native OpenCE-compatible network wire profile.
*/
#ifndef __HALO_NETWORK_PROFILE_H
#define __HALO_NETWORK_PROFILE_H
#pragma once

/* Version 11 introduced the last breaking change before OpenCE 21. Versions
 * 12 through 20 are additive: older machines drop the newer message types.
 * Version 21 has profile-specific damage and co-op BSP semantics.
 */
#define HALO_NETWORK_PROFILE_VERSIONS(X) \
 X(11, 11, 11, 0, 0) \
 X(12, 11, 12, 0, 1) \
 X(13, 11, 13, 0, 1) \
 X(14, 11, 14, 0, 1) \
 X(15, 11, 15, 0, 1) \
 X(16, 11, 16, 0, 1) \
 X(17, 11, 17, 0, 1) \
 X(18, 11, 18, 0, 1) \
 X(19, 11, 19, 0, 1) \
 X(20, 11, 20, 0, 1) \
 X(21, 21, 21, 1, 1)

int network_profile_host_version(void);
int network_profile_active_version(void);
int network_profile_select_room(unsigned int advertised_version);
void network_profile_clear_room(void);
int network_profile_v21_enabled(void);
int network_profile_supports_coop(void);
int network_profile_known_version(unsigned int version);
int network_profile_client_can_join(unsigned int client_profile, unsigned int host_version);

#endif

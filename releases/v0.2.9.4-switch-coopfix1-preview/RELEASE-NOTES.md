# Unified Switch Co-op Lifecycle Preview 2.9.4

This prerelease updates the standalone Nintendo Switch NRO and matching filtered source archive. The co-op lifecycle changes make pause-menu **Revert to Checkpoint** and **Restart Level** host-authoritative, return the host to the correct next-map lobby, and preserve the configured co-op player cap and friendly-fire setting during the transition. The client does not initiate the host-only restart path.

The Android-ABI guest and native Switch host both cross-compiled and linked. The NRO contains the exact guest ELF bytes. The SD file at `switch/NxHalo-Content29/halo.nro` was staged and read back with a matching SHA-256; the previous file was retained for rollback. macOS declined to unmount the SD volume, so it remains mounted. **The NRO has not been launched on Switch hardware; Switch gameplay and physical acceptance are unverified.** Linux runtime evidence for the shared code is separate and does not establish Switch runtime behavior.

This is a Homebrew Menu NRO, not an NSP. No HOME-menu forwarder, DBI installation, physical boot, or gameplay is claimed. The source archive has 2,305 filtered entries and omits generated shader instruction includes, game maps, resource companions, keys, saves, and generated build binaries. A local shader instruction include was needed to complete this build and is not part of the release source archive.

The previous [Unified Switch Content Preview 2.9.4](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9.4-switch-content-preview) and all earlier releases remain available.

# Unified Steam Deck Content Preview 2.9.1

This additive hotfix corrects menu activation for existing `mods_pack_N` rows: selecting a texture-pack row now enters the native texture-pack selection path instead of falling through to clipboard download. It does not change the Preview 2.9 feature set. Preview 2.9 remains published and available for rollback.

Linux executable SHA-256: `84336f0f5394a53ea9d8ff6fc8b1e350c6709feb7aa967d378915b9ff3003918`. The final source compiled successfully with the production Ninja build. The existing native Deck UI review for catalog controls, texture override OFF, upscaling OFF, and compact footer remains applicable; this hotfix does not claim new keyboard search or texture-pixel acceptance.

The hotfix is atomically installed and running on the Deck; PID 566527 and destination executable both match the release hash. Production selector regression verified rows 0 and 11 select successfully, row 12 is rejected, and clipboard fallback remains intact. The previous 2.9 executable remains available for rollback. No game maps, owner resource companions, shader instruction tokens, console keys, or saves are included. See `NATIVE-CONTENT.md` and `SUPPORTED-MODS.md` for scope and limits.

# NxHalo source snapshot (Steam Deck / Linux client and Switch guest)

Base: ChupathingyCE/chupathingyce commit 39cb857f (network version 11), merged with the NxHalo changes: the Switch GL path (`port/linux/src/d3d8_gl.c`), loading normalization, Profile33 diagnostics, campaign characters (`port/linux/game/nxhalo_custom_content.c`, adapted from fucktrevor/HCE-Mobile, attribution retained in source), and the external shader loader.

Layout: the repository root is the game tree. `switch-host/port/` is the Switch host tree as built for Profile33 (pinned to the earlier upstream commit 690e964; it has not been rebuilt against this game tree).

Shader data: on Linux and on the Switch guest the game reads `maps/shaders.bin`, produced locally by the importer from the owner's game, and verifies its length and CRC (`source/rasterizer/xbox/rasterizer_xbox_vertex_shaders.c`). Shader instruction tokens are absent from the distributed binaries and from this archive. `rasterizer_xbox_vertex_shaders_data.inc` is excluded; targets that reference it (Windows, macOS, the original Xbox) need the owner's local extraction and are not claimed as public ready builds.

Linux / Steam Deck client: `python3 configure.py --release --pgo=off --game-browser --portable`, then `ninja linux`. It is a 32-bit build (clang, `-m32`) against a 32-bit SDL3. To run on glibc older than the build machine's, compile `port/linux/glibc_compat.c` with `clang --target=i686-linux-gnu -m32 -O2 -c` and add the object plus `-Wl,--wrap=fmod -Wl,--wrap=sqrtf` to the link line of `build/linux/halo` in the generated `build.ninja`. `build.ninja` is generated and not shipped.

Switch guest: `configure.py --release --pgo=off --game-browser --android-guest-cc=/opt/homebrew/opt/llvm/bin/clang`, then `ninja build/android/halo_guest.elf`. Copy the guest ELF to `switch-host/port/switch/romfs/halo_guest.elf`, then `make` in `switch-host/port/switch` with devkitPro/devkitA64 and Switch portlibs.

HUD, menu and font assets under `port/assets` and `port/linux/ui` come from the upstream repository. Original component licenses remain in their source directories and are collected in the client package's `NOTICES`. No blanket replacement license is asserted. A bit-identical rebuild is not promised.

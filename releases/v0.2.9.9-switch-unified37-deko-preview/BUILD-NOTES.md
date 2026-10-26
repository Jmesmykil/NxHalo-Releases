# Unified37 source build notes

This filtered source snapshot preserves the tested Unified37 version through export-only restoration of four guest files and two host files from pre-U38 backups. The portable game/guest is under source/; the Horizon host is under host/port/switch/. UAM source is pinned to devkitPro/uam 5a5afc2bae8b55409ab36ba45be63fcb73f68993 plus the halo-ce-universal Build114 patch. Donor recipe commit: 201e0f18011a30ea6d098ad249bf17eba005d653. Original licenses are retained.

This is NOT a standalone reproducible build. Generated shader instruction .inc files, maps/resource companions, console keys, saves and generated binaries are excluded following the earlier public-source policy. The shader instruction include used locally must be supplied/extracted from the user's own compatible data. The snapshot has not been independently rebuilt and compared byte-for-byte with the tested guest.

Build requirements: LLVM clang/lld and Ninja for the ARM64_32 guest; devkitPro devkitA64, libnx, deko3d 0.5 and Switch portlibs for the host. UAM uses Meson, Ninja, Mako and Bison 3.8.2 or compatible. The build helper assumes /opt/devkitpro. Python is used only by host build tools, never as the Switch runtime.

Guest: HALO_SWITCH_DEKO3D=1 configure.py --release --pgo=off --android-guest-cc=<clang>, then ninja build/android/halo_guest.elf. UAM: toolchain/uam/build_uam.sh followed by combine_uam.py; provision its Meson/Mako/Bison prerequisites locally. Place the guest ELF in host/port/switch/romfs-deko/halo_guest.elf and supply runtime broker/certificate files, then make DEKO3D=1 UAM_OBJECT=<absolute uam_compiler.o path>. Installed-title packaging requires locally held console keys, not supplied.

The tested NSP embeds guest SHA256 10a9be9c131a5586243996310718d687d40d1c0b1435f796d01d62f2a864ec4d. Source hashes identify exported bytes; they do not prove a reproducible binary.

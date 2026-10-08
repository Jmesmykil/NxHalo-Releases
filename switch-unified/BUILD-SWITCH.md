# Nintendo Switch community preview build recipe

This tree contains the source for the Switch 2.9.4 co-op lifecycle preview NRO. The NRO embeds its matching Android-ABI guest ELF, a Mozilla CA bundle, and its MPL-2.0 license and source notice. It contains no Halo map assets, console keys, or `instructions.inc`.

## Prerequisites

- macOS with Python 3, Ninja, LLVM clang plus `llvm-ar`, and Rust's `rust-lld` (the guest uses the Android AArch64 ILP32 ABI).
- A Switch devkitPro installation with devkitA64, libnx, SDL2, Mesa, curl, zlib, and their portlibs.
- The matching Halo CE source dependency headers under `dependencies/`, including `dependencies/include/GLES2`, `GLES3`, and `KHR`. These external headers are intentionally not included in this private source bundle.
- A writable object directory with enough space. Keep `build` and `port/switch/build` pointed at that directory; the build tree is large.

## Guest ELF

Create the minimal NDK-shaped sysroot expected by `tools/android_build.py`: `ndk-layout/toolchains/llvm/prebuilt/darwin-arm64/bin/clang`, `aarch64-linux-android21-clang`, `llvm-ar`, and `ld.lld` must resolve to the local LLVM/Rust tools; `sysroot/usr/include/GLES2`, `GLES3`, and `KHR` must resolve to the matching dependency headers above. This is a Switch guest compatibility layout, not a full Android SDK or install.

Generate the guest build and compile only the guest ELF:

```sh
python3 configure.py \
  --android-ndk "$PWD/ndk-layout" \
  --android-guest-cc /path/to/llvm/bin/clang \
  --android-switch-guest
ninja -f build.ninja build/android/halo_guest.elf
```

The 2.9.4 preview was built with the available Clang 21/llvm-ar and Rust LLD on macOS. Profile-guided optimization requiring Clang 22 was therefore omitted. Keep all object output and temporary files on a volume with sufficient free space.

## Host NRO

Copy the guest ELF to `port/switch/romfs/halo_guest.elf`, then build the host package:

```sh
cp build/android/halo_guest.elf port/switch/romfs/halo_guest.elf
(cd port/switch && DEVKITPRO=/path/to/devkitpro make host)
```

`make host` builds the native Switch host and packages the RomFS files, including the guest ELF and CA bundle, into `halo.nro`. It does not install or deploy the package. Do not add private map assets or console keys to this source bundle.

## Preview status

The co-op lifecycle NRO is published as `v0.2.9.4-switch-coopfix1-preview`. It was staged and read back at `switch/NxHalo-Content29/halo.nro`; the prior file was retained for rollback. macOS declined to unmount the SD volume, which was left mounted. The NRO has not been launched on Switch hardware, and Switch gameplay has not been verified. No HOME-menu NSP forwarder was inspected or verified; this source package includes no NSP.

The shared co-op lifecycle changes make host pause-menu Revert and Restart authoritative, return the host to the next-map lobby, and preserve the co-op player cap and friendly-fire setting across the transition. These changes cross-compiled for the Switch guest and host. Linux runtime evidence is documented in the separate Steam Deck release; it is not Switch runtime evidence.

## Trust roots

`port/switch/romfs/cacert.pem` is the Mozilla CA extract distributed by curl.se. Its exact SHA256 is recorded in `port/switch/romfs/cacert-source.txt`; the accompanying `MPL-2.0.txt` contains the license. Both curl request paths set this bundle explicitly and keep TLS peer and hostname verification enabled.

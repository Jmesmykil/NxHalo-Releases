# Switch candidate build evidence

The latest Linux source archive `outputs/NxHalo-Unified-source.zip` has SHA256 `7abe065e42995b186264753cdaec656d35ea8f6839d6e8b160797f41c94e8011`. Its `menu_tags.c` native stock-error redraw and `port_config.c` composed-screenshot registration match the isolated Switch candidate exactly. The candidate retains its Switch SDL, guest ABI, GLES, TLS, and host code.

Builds completed on 2026-10-07:

- Guest: `ninja -f build.ninja build/android/halo_guest.elf` succeeded after compiling 31 affected steps, including cache, HS, networking, menu/tag, config, and UI sources.
- Host/package: `(cd port/switch && DEVKITPRO=/opt/devkitpro make host)` succeeded and packaged the rebuilt guest ELF plus CA roots into `halo.nro`.
- The packaged NRO byte-matches the exact CA PEM file and contains the guest ELF. TLS peer and hostname verification remain enabled in both curl paths.
- Source archive contents were checked for map assets, console keys, `instructions.inc`, generated build.ninja, and external dependency symlinks; none are included. The source build recipe identifies external prerequisites.
- Host source selects `romfs:` only when `romfs:/maps/ui.map` exists; otherwise it selects `sdmc:/switch/halo` as data root and `sdmc:/switch/halo/save-community24` as save root. This candidate NRO has no bundled map data, so the expected data root is the writable SD location.

These are compile and packaging results. This candidate has not been installed or exercised on physical Switch hardware; hardware acceptance remains unverified.

Artifact hashes are in `SHA256SUMS.txt`.

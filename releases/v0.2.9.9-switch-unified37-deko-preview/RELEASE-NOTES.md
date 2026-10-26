# NxHalo Unified37 — Switch deko3d update

Unified37 adds native deko3d rendering, asynchronous shader compilation/preloading, GPU buffer retirement/reuse, HUD/loading integration and bounded UDP receive batching. It adapts halo-ce-universal Build114 while retaining NxHalo gameplay and compatibility profiles. Defaults remain stock CPU clocks and 480p internal rendering.

The owner tested this exact installed-title candidate on physical Switch 1 and reported that it works great. This is user-reported gameplay acceptance, not a measured FPS comparison or proof of every campaign mission, large lobby or Switch 2 compatibility. No numeric performance claim is made.

## Install

Supply your own compatible maps under sdmc:/switch/halo/maps/. Preserve config and sdmc:/switch/halo/save-community24/ saves. Install Halo_CE_Unified37.nsp using your existing homebrew title installer and launch Halo from HOME. It updates NxHalo title010048414c210000 (content version65560), not a separate app. Keep a previous installer for rollback.

## Source and limits

Filtered matching-version source and hashes accompany the package. Generated shader instruction includes are omitted; a clean source checkout alone cannot reproduce the binary without locally supplied shader extraction and toolchain prerequisites. Generated shader material used in the built game remains a provenance limitation. No game maps, resource companions, console keys or personal saves are supplied.

Direct GPU mapping of guest memory is disabled: upload copies preserve our allocation lifetimes. The new co-op/inventory/shield fixes being developed after Unified37 are NOT in this package. Steam Deck/Linux2.9.9 remains separate. Citron/Eden failures are separate emulator evidence, not hardware acceptance.

Credits: original Halo decompilation projects, OpenCE, thelinkin3000/halo-ce-universal, devkitPro/deko3d/UAM and bundled library authors. Source/dependency licenses are retained. This unofficial fan project is not endorsed by Microsoft, Bungie or Halo Studios.

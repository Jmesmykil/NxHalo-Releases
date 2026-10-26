# NxHalo — Halo for Nintendo Switch and Steam Deck

| Platform | Current download | Status |
| --- | --- | --- |
| Nintendo Switch 1 | [Unified37 installer, source and checksums](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9.9-switch-unified37-deko-preview) | Tested by the owner on Switch 1 |
| Steam Deck / Linux | [2.9.9 runtime and source](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9.9-steamdeck-lobby-stability) | Separate native Linux build |

## Current Nintendo Switch — Unified37 deko3d

[Download Unified37 and matching source](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9.9-switch-unified37-deko-preview).

This Switch 1 build adds native deko3d rendering, asynchronous shader compilation and preloading, GPU resource retirement, HUD/loading integration, and bounded UDP receive batching. The owner tested this exact installed-title package on an original Switch and reported it works great on October 25, 2026. Full-campaign coverage, large-lobby capacity and measured FPS gains remain unverified. Switch 2 is not validated by this test.

Install `Halo_CE_Unified37.nsp` using your existing homebrew title installer. It updates the existing NxHalo title. Keep your current installer for rollback. Supply your own compatible maps under `sdmc:/switch/halo/maps/`; saves remain under `sdmc:/switch/halo/save-community24/`. No game maps, console keys or saves are supplied. This release is the tested NSP; the older Homebrew Menu NRO remains in its separate release.

Package SHA-256: `0c7d3abf235d98149e42e634f518b8d89e1290ae89ae3f6615cfbb89f6aac98d`.
Display version: `0.2.9.9-dk1`. Logs: `sdmc:/switch/halo/halo-unified37-deko.log`.

Renderer integration derives from [halo-ce-universal Build114](https://github.com/thelinkin3000/halo-ce-universal/tree/201e0f18011a30ea6d098ad249bf17eba005d653), with upstream attribution and licenses retained. The filtered source and build notes document the required shader-generation inputs. Private Unified38 gameplay fixes are being tested separately and are not part of this download.


## Current Steam Deck/Linux 2.9.9 — lobby stability

[Download runtime and matching source](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9.9-steamdeck-lobby-stability).
Executable SHA-256: 13661136b9dc51b454d6d665df0c4306415bcd3c01f2c862180166591fc4229b. This exact build is installed through the existing Deck shortcut, with the previous 2.9.8 and stable 2.9.6 rollbacks retained.

## Steam Deck / Linux changes

- A multiplayer round returns to its current map, gametype and custom rules instead of stale playlist defaults. Co-op keeps its separate mission progression.
- Gun Game weapon replacement retains the prior weapon on failure, selects the new stage on success and removes spare weapons.
- The name editor rebinds tag-backed keyboard data and clears it before cache teardown.
- Online Play includes a separate Native V24 view, preserves verified newer-version entries, displays directory/cache state and keeps endpoint selection stable across refresh.
- V24 is an explicit compatibility profile; V21 remains the host default and existing older native profiles remain selectable. V24 handles the official flat custom-map namespace and authored CE609 vehicle selection.
- Installed Maps has searchable, paged family/type/details navigation with capacity4096; invalid/resource files are distinguished. Native folder import/cancel preserves selected and enabled packs.

## Steam Deck / Linux evidence

The acceptance receipt identifies exact checks and limits for the real Gun Game restart, keyboard cache transition and independent official v24 client admission. Private Deck AMD GPU gameplay against the unchanged2.9.6 native executable passed; real remote input advanced the weapon ladder. Existing saves, maps, controller settings, launcher and normal audio volume were preserved. Tests use silent audio and do not capture a hardware microphone.

The independent v24 runtime check covers stock Beavercreek and two players; CE609 authored-vehicle behavior and reverse-direction admission require separate checks.

The owner's completed online Gun Game win is retained separately from automated evidence. No synthetic128-row test is described as real128-player gameplay.

## Compatibility and remaining work

Supply your own compatible maps and resource files. Shared-map sword, faction models and authored tracks/towers require the same compatible map on participants. Voice requires compatible native receive/playback support. A classic PC/CE or browser listing does not prove executable/protocol compatibility; private or unannounced games cannot be guaranteed by a public directory.

Complete race-route packs, specialized bots, Prop Hunt, persistent friends, full campaign lifecycle, human microphone use and real capacity remain separate backlog items. Unified37 Switch gameplay was confirmed separately by the owner; exhaustive Switch coverage remains open. This Linux release does not replace the separate Switch executable.

[Guide](steam-unified/docs/LOBBY-STABILITY-2.9.9.md) ·
[Acceptance](releases/v0.2.9.9-steamdeck-lobby-stability/ACCEPTANCE.json) ·
[Deck hardware evidence](releases/v0.2.9.9-steamdeck-lobby-stability/DECK-GPU.json).
## Steam Deck / Linux installation

Extract the Linux runtime, supply your own compatible game maps in assets/maps, and run launch_halo.sh natively with Proton OFF. Keep your existing settings and saves. No game maps, resource companions, console keys or personal saves are provided.

Online campaign supports remote players without requiring a second local controller. Native multiplayer/campaign and classic PC/CE directories remain separately filtered by source and version; classic snapshots require a compatible classic client.

CE609 maps needing external resources require owner-supplied bitmaps.map, sounds.map and loc.map under assets/maps/ce. DLL/Lua/OpenSauce/shader engine extensions need individual native ports. The texture catalogue is an initial compatible content source, not a claim that all CE mods run natively.

## Source and credits

Matching Linux source is in [steam-unified/](steam-unified/) and the release source ZIP. Original decompilation, OpenCE, port and library attribution/licenses remain intact. Optional authored texture packages retain their own licenses. Earlier releases remain available for rollback.

This unofficial fan project is not endorsed by Microsoft, Bungie or Halo Studios.

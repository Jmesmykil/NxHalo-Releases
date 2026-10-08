# NxHalo — Halo: Combat Evolved for Switch and Steam Deck

NxHalo is a native Halo: Combat Evolved port with separate Nintendo Switch and Steam Deck/Linux builds. Releases include offline setup tools and player support. Supply your own legally obtained game data; no game content or console keys are included.

## Current builds

**Newest Switch candidate: Unified Content Preview 2.9.4 / v0.2.9.4-switch-content-preview (pre-release).** The [candidate release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9.4-switch-content-preview) contains a standalone NRO, sanitized source and build records. NRO SHA-256: `95146cf80828d3c7da2b450a7c677b2ce79caab03acfe72b456d35945aa5404b`. The NRO was staged at `sdmc:/switch/NxHalo-Content29/halo.nro` on an identified SD card, read back by hash and safely ejected. It has **not** been launched on Switch hardware. This is a Homebrew Menu NRO: it is not an NSP, has no DBI installer or HOME-menu forwarder, and does not create a HOME icon. For game data, provide your own compatible Halo CE data under `sdmc:/switch/halo/`, including its `maps/` directory; the preview recognizes data builds `01.01.14.2342` and `01.10.12.2276`. Saves are written under `sdmc:/switch/halo/save-community24/`. No game maps, resources, keys or saves are included in the release.

**Previous unified Switch candidate: Preview 2.8 / v0.2.8-switch-unified8 (pre-release).** The [candidate release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.8-switch-unified8) provides a standalone NRO and matching source/build records. NRO SHA-256: `5b18c2b201ee6ab62785911f307197bd7ba13543f6be64161f82b86c2013e376`. This candidate has not been staged through UMS or installed/run on physical Switch hardware; hardware acceptance is unverified. It uses the same title ID (`010048414C210000`) as Profile33, so retain the Profile33 NRO separately for rollback. It is not an NSP and includes no full-memory title override. Game data is read from `sdmc:/switch/halo` when the candidate has no usable `romfs:/maps/ui.map`; saves use `sdmc:/switch/halo/save-community24`. No game maps, resources, keys or saves are included. See [`switch-unified/BUILD-SWITCH.md`](switch-unified/BUILD-SWITCH.md).

**Previous Switch network-profile candidate: Profile33 / v0.1.11-p33 (pre-release, network protocol 11).** The [Profile33 release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.1.11-p33) remains available with its setup apps, checksums and validation records. It includes cross-console multiplayer, public internet lobby browsing and custom campaign characters. The creator's current practical match-size estimate is about 40 players. That is not a measured simultaneous-player acceptance result: public runs discovered lobbies listing 32 and 36 players. Profile33 was staged/read back through UMS; that alone does not establish physical Switch install or runtime acceptance.

**Stable Switch build: Build14 / v0.1.1.** [Build14](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.1.1-build14) remains the stable campaign baseline and latest non-prerelease. It is a separate title from Profile33 and remains available for rollback.

**Newest Steam Deck/Linux preview: Unified Runtime Preview 2.9.4 (pre-release).** [Download v0.2.9.4](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9.4-steamdeck-runtime). Runtime SHA-256: `8335b4f246b592854a91916bb36141c884883eb197eff1f5e59e4fe78aae64fc`. The single native executable combines campaign, online co-op and multiplayer profiles 11/20/21; profile 11 is multiplayer-only. It includes the live CE catalog, native texture packs, Deck Off/Quality/Performance rendering, and cached match-rule presets. A private V21 campaign movement check matched host/client position on `a50`; this does not establish campaign completion. The exact build is installed and running on Steam Deck. One 65-second two-player Bloodgulch Quality session measured 59.70 fps across four post-load intervals, p95 16.825–16.864 ms, p99 16.909–16.949 ms, RSS 210136 KiB (peak 238484 KiB). These measurements cover that session only. Real 128-player matches, Friends/voice and broad legacy-client interoperability remain unverified. Full source is [`steam-unified/`](steam-unified); release-specific evidence is in [`releases/v0.2.9.4-steamdeck-runtime/`](releases/v0.2.9.4-steamdeck-runtime/).
**Custom Maps & Mods** imports supported map/ZIP files and required owner resource companions, downloads maps through HaloNet, and selects campaign/host characters. OpenSauce/Chimera engine extensions need individual native ports. [Content scope and verification limits](releases/v0.2.9-steamdeck-runtime/SUPPORTED-MODS.md); see [native content details](releases/v0.2.9-steamdeck-runtime/NATIVE-CONTENT.md).

Menu routes: Multiplayer → Join/Create/Local Split/Edit Game Types; Create Game → Game Modes (Combat/Factions/Race/Standard) or Network Options; Custom Maps & Mods → Custom Maps → Browse; Characters remains separate. The editor saves game types, weapons, players, vehicles and scoring. Zombies uses a sword only when the selected map contains the required loaded tag/assets; missing content refuses infection and preserves inventory.

The Deck build is native Linux, not Proton. It needs your own prepared compatible Halo CE maps and support files. No game maps, executable, ROM, shader instruction data, console keys or saves are supplied. See the release notes for precise installation and verification status.

## Server discovery

Campaign, multiplayer, all-community, current/legacy native versions, broker and community-announced sources remain separately filtered. Native joins use the same executable. Classic PC/CE snapshots remain separate and are not native-joinable. Online co-op uses remote players without a second controller.

## Play with your own game

1. Download the setup app from [Releases](https://github.com/Jmesmykil/NxHalo-Releases/releases), then use it with your own compatible original Xbox Halo image or extracted game folder.
2. For the Switch preview, put the compatible game data on the SD card under `switch/halo/` with its `maps/` directory. Copy the standalone NRO to `switch/NxHalo-Content29/halo.nro` and launch it from Homebrew Menu. DBI and HOME-menu installation do not apply to this NRO.
3. The Switch preview writes saves under `switch/halo/save-community24/`. Keep backups of existing game data and previous NROs. Build14 and Profile33 remain separate releases.

The Switch must already be configured to run this homebrew title. Setup apps are available for **Windows x64** and **Apple Silicon macOS**. These preview apps are unsigned. Other computers can use the importer source with Python 3.10 or newer with Tk. Halo PC, Custom Edition, Anniversary, MCC and Switch 2 are outside the Switch release.

## Validation status

Unified Runtime Preview 2.9.4 binary SHA-256 is `8335b4f246b592854a91916bb36141c884883eb197eff1f5e59e4fe78aae64fc`. Exact Deck destination/readback and running-process hashes match. The native V21 campaign browser loaded a private two-player `a50` session; a three-second client movement input produced an agreeing host position. This verifies movement/state agreement in the loaded mission, not a completed campaign.

Two CE catalog maps and a two-client Zombies end-round case were tested on predecessor 0f6; the release evidence labels those hashes separately. Visible texture override and 128-slot roster checks used separate earlier builds; the roster used synthetic peers, not 128 real users. Full campaign completion, Friends, voice and broad legacy interoperability remain unverified. See [2.9.4 verification evidence](releases/v0.2.9.4-steamdeck-runtime/VERIFICATION-EVIDENCE.md) and [native content guide](releases/v0.2.9.4-steamdeck-runtime/NATIVE-CONTENT.md).
## Source

The current Switch Content Preview 2.9.4 NRO, source archive and build evidence are attached to its [prerelease](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9.4-switch-content-preview); the source tree is [`switch-unified/`](switch-unified). The earlier Switch 2.8 source and [v0.2.8-switch-unified8 release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.8-switch-unified8) remain available. The complete Linux source snapshot for Steam Deck Runtime Preview 2.9.4 is in [`steam-unified/`](steam-unified), with its matching filtered source ZIP attached to the [Steam v0.2.9.4 prerelease](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9.4-steamdeck-runtime). Earlier Steam 2.9.3, 2.9.2, 2.9.1 and 2.9 releases remain available. The earlier Switch/Profile33 source remains under [`source/`](source). These snapshots omit game data, resource companions, shader instruction tokens, console keys and saves.
## Updates and support

Keep your imported game data, saves and settings. Back up the SD game folder before installing another build. Review the selected release notes for title ID and compatibility details.

For a bug report, include the release version, map or mission, player count, wired or Wi-Fi connection and reproducible steps. Remove personal information from logs before posting. Do not upload game images, maps, executables, keys or saves.

This is an independent community project and is not endorsed by the original game or console publishers.

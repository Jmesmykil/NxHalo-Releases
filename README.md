# NxHalo — Halo: Combat Evolved for Switch and Steam Deck

NxHalo is a native Halo: Combat Evolved port with separate Nintendo Switch and Steam Deck/Linux builds. Releases include offline setup tools and player support. Supply your own legally obtained game data; no game content or console keys are included.

## Current builds

**Newest Switch candidate: Unified Content Preview 2.9.4 / v0.2.9.4-switch-content-preview (pre-release).** The [candidate release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9.4-switch-content-preview) contains a standalone NRO, sanitized source and build records. NRO SHA-256: `95146cf80828d3c7da2b450a7c677b2ce79caab03acfe72b456d35945aa5404b`. The NRO was staged at `sdmc:/switch/NxHalo-Content29/halo.nro` on an identified SD card, read back by hash and safely ejected. It has **not** been launched on Switch hardware. This is a Homebrew Menu NRO: it is not an NSP, has no DBI installer or HOME-menu forwarder, and does not create a HOME icon. For game data, provide your own compatible Halo CE data under `sdmc:/switch/halo/`, including its `maps/` directory; the preview recognizes data builds `01.01.14.2342` and `01.10.12.2276`. Saves are written under `sdmc:/switch/halo/save-community24/`. No game maps, resources, keys or saves are included in the release.

**Previous unified Switch candidate: Preview 2.8 / v0.2.8-switch-unified8 (pre-release).** The [candidate release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.8-switch-unified8) provides a standalone NRO and matching source/build records. NRO SHA-256: `5b18c2b201ee6ab62785911f307197bd7ba13543f6be64161f82b86c2013e376`. This candidate has not been staged through UMS or installed/run on physical Switch hardware; hardware acceptance is unverified. It uses the same title ID (`010048414C210000`) as Profile33, so retain the Profile33 NRO separately for rollback. It is not an NSP and includes no full-memory title override. Game data is read from `sdmc:/switch/halo` when the candidate has no usable `romfs:/maps/ui.map`; saves use `sdmc:/switch/halo/save-community24`. No game maps, resources, keys or saves are included. See [`switch-unified/BUILD-SWITCH.md`](switch-unified/BUILD-SWITCH.md).

**Previous Switch network-profile candidate: Profile33 / v0.1.11-p33 (pre-release, network protocol 11).** The [Profile33 release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.1.11-p33) remains available with its setup apps, checksums and validation records. It includes cross-console multiplayer, public internet lobby browsing and custom campaign characters. The creator's current practical match-size estimate is about 40 players. That is not a measured simultaneous-player acceptance result: public runs discovered lobbies listing 32 and 36 players. Profile33 was staged/read back through UMS; that alone does not establish physical Switch install or runtime acceptance.

**Stable Switch build: Build14 / v0.1.1.** [Build14](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.1.1-build14) remains the stable campaign baseline and latest non-prerelease. It is a separate title from Profile33 and remains available for rollback.

**Newest Steam Deck/Linux candidate: Unified Rules Preview 2.9.3 (pre-release).** [Download the NxHalo release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9.3-steamdeck-rules). It applies cached presets, guards host-selected modes, reports replicated rules in the joined lobby/public listing, and enforces host-side infection melee/loadout behavior. Harness/UI checks pass; old-client skull behavior and broad legacy interoperability remain unverified. Previous 2.9.x releases remain available. The release adds the live CE map catalog, native texture-pack overrides and Deck-gated Off/Quality/Performance rendering. Campaign, online co-op, native multiplayer, separate server filters, CE map setup and host character presets use one executable with V11/V20/V21 profiles. V11 is multiplayer-only. Host bipeds must exist in the map. Full unified Linux source is [`steam-unified/`](steam-unified); the Switch source remains separate.

**Custom Maps & Mods** imports supported map/ZIP files and required owner resource companions, downloads maps through HaloNet, and selects campaign/host characters. OpenSauce/Chimera engine extensions need individual native ports. [Content scope and verification limits](releases/v0.2.9-steamdeck-content/SUPPORTED-MODS.md); see [native content details](releases/v0.2.9-steamdeck-content/NATIVE-CONTENT.md)..

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

Unified Rules Preview 2.9.3 binary SHA-256 is `2f9929de4467a4e908ec9c6cc3ac25f3ece3f9b10f9ae29f6445496918f6c7c4`. The 2.9.3 executable is installed and running on the Deck; both destination and process hashes match `e439a36aa08183cf4b27d4e869a5f6b0900c0f8286e9efc9a9aba7d9584f428c`. Previous 2.9.2 and 2.9.1 builds remain available for rollback.

The reviewed native Deck catalog showed a dynamic SEARCH MAPS field, 5,020 matches over 558 pages, and populated results. Parser query behavior was unit-tested; physical search-entry acceptance and optional XTest search are unverified. Texture identity/decode/GL upload was traced for a synthetic TGA but the captured frame did not visibly show the override. Off/Quality/Performance world probes passed on software GL at 1280x800; physical Deck performance is unmeasured. Full 128-player acceptance, live preset matches, Friends and voice remain unverified. [Release notes](releases/v0.2.9.3-steamdeck-rules/release-notes.md) and [native content guide](releases/v0.2.9-steamdeck-content/NATIVE-CONTENT.md).

## Source

The Switch Content Preview 2.9.2 candidate is in its [prerelease](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9.2-switch-menus); it is not staged or runtime-tested. Preview 2.9.1 remains available. The Switch Content Preview 2.9 source archive and build evidence are attached to its [prerelease](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9-switch-content). The earlier Preview 2.8 Switch source remains under [`switch-unified/`](switch-unified), with its build guide and [v0.2.8-switch-unified8 release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.8-switch-unified8). The complete unified Linux source for Steam Deck Rules Preview 2.9.3 is in [`steam-unified/`](steam-unified); the native build guidance is [`steam-unified/port/linux/README.md`](steam-unified/port/linux/README.md), with the matching source archive attached to the [Steam v0.2.9 prerelease](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9-steamdeck-content). Earlier [Steam Preview 2.8](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.8-steamdeck-unified8) and [2.8.1](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.8.1-steamdeck-unifiedhotfix) releases remain available. The earlier Switch/Profile33 source remains under [`source/`](source) with its own [Profile33 release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.1.11-p33). These sources omit game data, resource companions, shader instruction tokens, console keys and saves.

## Updates and support

Keep your imported game data, saves and settings. Back up the SD game folder before installing another build. Review the selected release notes for title ID and compatibility details.

For a bug report, include the release version, map or mission, player count, wired or Wi-Fi connection and reproducible steps. Remove personal information from logs before posting. Do not upload game images, maps, executables, keys or saves.

This is an independent community project and is not endorsed by the original game or console publishers.

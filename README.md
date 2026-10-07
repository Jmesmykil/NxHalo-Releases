# NxHalo — Halo: Combat Evolved for Switch and Steam Deck

NxHalo is a native Halo: Combat Evolved port with separate Nintendo Switch and Steam Deck/Linux builds. Releases include offline setup tools and player support. Supply your own legally obtained game data; no game content or console keys are included.

## Current builds

**Newest Switch candidate: Unified Content Preview 2.9 / v0.2.9-switch-content (pre-release).** The [candidate release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9-switch-content) contains a standalone NRO and final sanitized source/build records. NRO SHA-256: `2a60465f0c928ed8007bfbe4e5b6fce7c1bad2c6713261515fb5e1158523d646`. The NRO and source builds completed, but the NRO was staged to the identified SD card and hash-verified, but no HOME-menu install or Switch runtime test was performed. It uses the existing title ID (`010048414C210000`), so keep a prior NRO for rollback. SD message-box browsing for map/ZIP and texture-pack folders is compiled in; Deck-only upscaling is disabled in the Switch guest. No game maps, resources, keys or saves are included.

**Previous unified Switch candidate: Preview 2.8 / v0.2.8-switch-unified8 (pre-release).** The [candidate release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.8-switch-unified8) provides a standalone NRO and matching source/build records. NRO SHA-256: `5b18c2b201ee6ab62785911f307197bd7ba13543f6be64161f82b86c2013e376`. This candidate has not been staged through UMS or installed/run on physical Switch hardware; hardware acceptance is unverified. It uses the same title ID (`010048414C210000`) as Profile33, so retain the Profile33 NRO separately for rollback. It is not an NSP and includes no full-memory title override. Game data is read from `sdmc:/switch/halo` when the candidate has no usable `romfs:/maps/ui.map`; saves use `sdmc:/switch/halo/save-community24`. No game maps, resources, keys or saves are included. See [`switch-unified/BUILD-SWITCH.md`](switch-unified/BUILD-SWITCH.md).

**Previous Switch network-profile candidate: Profile33 / v0.1.11-p33 (pre-release, network protocol 11).** The [Profile33 release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.1.11-p33) remains available with its setup apps, checksums and validation records. It includes cross-console multiplayer, public internet lobby browsing and custom campaign characters. The creator's current practical match-size estimate is about 40 players. That is not a measured simultaneous-player acceptance result: public runs discovered lobbies listing 32 and 36 players. Profile33 was staged/read back through UMS; that alone does not establish physical Switch install or runtime acceptance.

**Stable Switch build: Build14 / v0.1.1.** [Build14](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.1.1-build14) remains the stable campaign baseline and latest non-prerelease. It is a separate title from Profile33 and remains available for rollback.

**Newest Steam Deck/Linux candidate: Unified Content Preview 2.9.2 (pre-release).** [Download the NxHalo release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9.2-steamdeck-menus). It adds organized multiplayer/game-mode menus, a saved game-type editor, and map-aware Zombies sword handling. The 2.9 and 2.9.1 releases remain available. The release adds the live CE map catalog, native texture-pack overrides and Deck-gated Off/Quality/Performance rendering. Campaign, online co-op, native multiplayer, separate server filters, CE map setup and host character presets use one executable with V11/V20/V21 profiles. V11 is multiplayer-only. Host bipeds must exist in the map. Full unified Linux source is [`steam-unified/`](steam-unified); the Switch source remains separate.

**Custom Maps & Mods** imports supported map/ZIP files and required owner resource companions, downloads maps through HaloNet, and selects campaign/host characters. OpenSauce/Chimera engine extensions need individual native ports. [Content scope and verification limits](releases/v0.2.9-steamdeck-content/SUPPORTED-MODS.md); see [native content details](releases/v0.2.9-steamdeck-content/NATIVE-CONTENT.md)..

Menu routes: Multiplayer → Join/Create/Local Split/Edit Game Types; Create Game → Game Modes (Combat/Factions/Race/Standard) or Network Options; Custom Maps & Mods → Custom Maps → Browse; Characters remains separate. The editor saves game types, weapons, players, vehicles and scoring. Zombies uses a sword only when the selected map contains the required loaded tag/assets; missing content refuses infection and preserves inventory.

The Deck build is native Linux, not Proton. It needs your own prepared compatible Halo CE maps and support files. No game maps, executable, ROM, shader instruction data, console keys or saves are supplied. See the release notes for precise installation and verification status.

## Server discovery

Campaign, multiplayer, all-community, current/legacy native versions, broker and community-announced sources remain separately filtered. Native joins use the same executable. Classic PC/CE snapshots remain separate and are not native-joinable. Online co-op uses remote players without a second controller.

## Play with your own game

1. Download the setup app from [Releases](https://github.com/Jmesmykil/NxHalo-Releases/releases).
2. Run the importer with your own compatible original Xbox Halo image or extracted game folder and select your mounted Switch SD root. It prepares the required game data locally.
3. Install the Switch runtime through DBI and launch Halo CE from HOME. Profile33 is a separate title from Build14; keep Build14 installed if you want the stable rollback available.

The Switch must already be configured to run this homebrew title. Setup apps are available for **Windows x64** and **Apple Silicon macOS**. These preview apps are unsigned. Other computers can use the importer source with Python 3.10 or newer with Tk. Halo PC, Custom Edition, Anniversary, MCC and Switch 2 are outside the Switch release.

## Validation status

Unified Content Preview 2.9.2 binary SHA-256 is `03a94e8d49c3533807c478ba2a3390080db8f74b8b1a207b31510efa2882c84b`. The 2.9.2 executable is atomically installed on the Deck with exact hash verified. The user’s public match remains live on prior process PID 579445 (hash `84336f0f5394a53ea9d8ff6fc8b1e350c6709feb7aa967d378915b9ff3003918`) until relaunch; rollback is preserved as `halo.before-menus292-20261007`. Rollback is preserved as `halo.before-content29-20261007`.

The reviewed native Deck catalog showed a dynamic SEARCH MAPS field, 5,020 matches over 558 pages, and populated results. Parser query behavior was unit-tested; physical search-entry acceptance and optional XTest search are unverified. Texture identity/decode/GL upload was traced for a synthetic TGA but the captured frame did not visibly show the override. Off/Quality/Performance world probes passed on software GL at 1280x800; physical Deck performance is unmeasured. Full 128-player acceptance, live preset matches, Friends and voice remain unverified. [Release notes](releases/v0.2.9.2-steamdeck-menus/release-notes.md) and [native content guide](releases/v0.2.9-steamdeck-content/NATIVE-CONTENT.md).

## Source

The Switch Content Preview 2.9.2 candidate is in its [prerelease](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9.2-switch-menus); it is not staged or runtime-tested. Preview 2.9.1 remains available. The Switch Content Preview 2.9 source archive and build evidence are attached to its [prerelease](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9-switch-content). The earlier Preview 2.8 Switch source remains under [`switch-unified/`](switch-unified), with its build guide and [v0.2.8-switch-unified8 release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.8-switch-unified8). The complete unified Linux source for Steam Deck Content Preview 2.9 is in [`steam-unified/`](steam-unified); the native build guidance is [`steam-unified/port/linux/README.md`](steam-unified/port/linux/README.md), with the matching source archive attached to the [Steam v0.2.9 prerelease](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9-steamdeck-content). Earlier [Steam Preview 2.8](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.8-steamdeck-unified8) and [2.8.1](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.8.1-steamdeck-unifiedhotfix) releases remain available. The earlier Switch/Profile33 source remains under [`source/`](source) with its own [Profile33 release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.1.11-p33). These sources omit game data, resource companions, shader instruction tokens, console keys and saves.

## Updates and support

Keep your imported game data, saves and settings. Back up the SD game folder before installing another build. Review the selected release notes for title ID and compatibility details.

For a bug report, include the release version, map or mission, player count, wired or Wi-Fi connection and reproducible steps. Remove personal information from logs before posting. Do not upload game images, maps, executables, keys or saves.

This is an independent community project and is not endorsed by the original game or console publishers.

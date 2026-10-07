# NxHalo — Halo: Combat Evolved for Switch and Steam Deck

NxHalo is a native Halo: Combat Evolved port with separate Nintendo Switch and Steam Deck/Linux builds. Releases include offline setup tools and player support. Supply your own legally obtained game data; no game content or console keys are included.

## Current builds

**Newest Switch candidate: Unified Preview 2.8 / v0.2.8-switch-unified8 (pre-release).** The [candidate release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.8-switch-unified8) provides a standalone NRO and matching source/build records. NRO SHA-256: `5b18c2b201ee6ab62785911f307197bd7ba13543f6be64161f82b86c2013e376`. This candidate has not been staged through UMS or installed/run on physical Switch hardware; hardware acceptance is unverified. It uses the same title ID (`010048414C210000`) as Profile33, so retain the Profile33 NRO separately for rollback. It is not an NSP and includes no full-memory title override. Game data is read from `sdmc:/switch/halo` when the candidate has no usable `romfs:/maps/ui.map`; saves use `sdmc:/switch/halo/save-community24`. No game maps, resources, keys or saves are included. See [`switch-unified/BUILD-SWITCH.md`](switch-unified/BUILD-SWITCH.md).

**Previous Switch network-profile candidate: Profile33 / v0.1.11-p33 (pre-release, network protocol 11).** The [Profile33 release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.1.11-p33) remains available with its setup apps, checksums and validation records. It includes cross-console multiplayer, public internet lobby browsing and custom campaign characters. The creator's current practical match-size estimate is about 40 players. That is not a measured simultaneous-player acceptance result: public runs discovered lobbies listing 32 and 36 players. Profile33 was staged/read back through UMS; that alone does not establish physical Switch install or runtime acceptance.

**Stable Switch build: Build14 / v0.1.1.** [Build14](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.1.1-build14) remains the stable campaign baseline and latest non-prerelease. It is a separate title from Profile33 and remains available for rollback.

**Newest Steam Deck/Linux release: Unified Preview 2.8 / v0.2.8 (pre-release).** [Download the NxHalo release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.8-steamdeck-unified8). Campaign, online co-op, native multiplayer, separate server filters, CE map setup and host character presets use one executable with V11/V20/V21 profiles. V11 is multiplayer-only. Host bipeds must exist in the map. Full unified Linux source is [`steam-unified/`](steam-unified); the Switch source remains separate.

**Custom Maps & Mods** imports supported map/ZIP files and required owner resource companions, downloads maps through HaloNet, and selects campaign/host characters. OpenSauce/Chimera engine extensions need individual native ports. [Content scope and verification limits](releases/v0.2.8-steamdeck-unified8/SUPPORTED-MODS.md).

The Deck build is native Linux, not Proton. It needs your own prepared compatible Halo CE maps and support files. No game maps, executable, ROM, shader instruction data, console keys or saves are supplied. See the release notes for precise installation and verification status.

## Server discovery

Campaign, multiplayer, all-community, current/legacy native versions, broker and community-announced sources remain separately filtered. Native joins use the same executable. Classic PC/CE snapshots remain separate and are not native-joinable. Online co-op uses remote players without a second controller.

## Play with your own game

1. Download the setup app from [Releases](https://github.com/Jmesmykil/NxHalo-Releases/releases).
2. Run the importer with your own compatible original Xbox Halo image or extracted game folder and select your mounted Switch SD root. It prepares the required game data locally.
3. Install the Switch runtime through DBI and launch Halo CE from HOME. Profile33 is a separate title from Build14; keep Build14 installed if you want the stable rollback available.

The Switch must already be configured to run this homebrew title. Setup apps are available for **Windows x64** and **Apple Silicon macOS**. These preview apps are unsigned. Other computers can use the importer source with Python 3.10 or newer with Tk. Halo PC, Custom Edition, Anniversary, MCC and Switch 2 are outside the Switch release.

## Validation status

Unified Preview 2.8 has a 22-second native Linux Death Island smoke (actor loaded, ticks31-394, world/weapon/HUD rendered); the final executable is installed on the Deck and its file hash is verified, but the already-running process remains on 5baaae until next launch. Human acceptance of stock modal styling remains open. Content/network menus were captured; archive fixtures and native linking passed. Private host initialization followed a reproduced and corrected keepalive memory fault. Death Island reached native gameplay on Omarchy with owner-supplied CE resources; sustained two-client interoperability, alternate-character matches and full download/join remain unverified. The prior synthetic roster capture does not prove a 128-player battle.

## Source

The unified Switch candidate source is [`switch-unified/`](switch-unified), with build guidance in [`switch-unified/BUILD-SWITCH.md`](switch-unified/BUILD-SWITCH.md) and a matching source archive in its [v0.2.8-switch-unified8 prerelease](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.8-switch-unified8). The complete unified Linux source for Steam Deck Preview 2.8 remains separate in [`steam-unified/`](steam-unified); the native build guidance is [`steam-unified/port/linux/README.md`](steam-unified/port/linux/README.md), with its source ZIP attached to the [Steam v0.2.8 prerelease](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.8-steamdeck-unified8). The earlier Switch/Profile33 source remains under [`source/`](source) with its own [Profile33 release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.1.11-p33). These sources omit game data, resource companions, shader instruction tokens, console keys and saves.

## Updates and support

Keep your imported game data, saves and settings. Back up the SD game folder before installing another build. Review the selected release notes for title ID and compatibility details.

For a bug report, include the release version, map or mission, player count, wired or Wi-Fi connection and reproducible steps. Remove personal information from logs before posting. Do not upload game images, maps, executables, keys or saves.

This is an independent community project and is not endorsed by the original game or console publishers.

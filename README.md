# NxHalo — Halo: Combat Evolved for Switch and Steam Deck

NxHalo is a native Halo: Combat Evolved port with separate Nintendo Switch and Steam Deck/Linux builds. Releases include offline setup tools and player support. Supply your own legally obtained game data; no game content or console keys are included.

## Current builds

**Newest published Switch build: Profile33 / v0.1.11-p33 (pre-release, network protocol 11).** The [Profile33 release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.1.11-p33) contains the Switch runtime, matching source, setup apps, checksums and validation records. It includes cross-console multiplayer, public internet lobby browsing and custom campaign characters. The creator's current practical match-size estimate is about 40 players. That is not a measured simultaneous-player acceptance result: public runs discovered lobbies listing 32 and 36 players. Profile33 was staged/read back through UMS; that alone does not establish physical Switch install or runtime acceptance.

**Stable Switch build: Build14 / v0.1.1.** [Build14](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.1.1-build14) remains the stable campaign baseline and latest non-prerelease. It is a separate title from Profile33 and remains available for rollback.

**Newest Steam Deck/Linux build: Unified Preview 2.7 / v0.2.7 (pre-release).** [Download the unified release](https://github.com/Jmesmykil/HolaDeck/releases/tag/v0.2.7-steamdeck-unified7). Campaign, online co-op, native multiplayer, separate server filters, CE map setup and host character presets now use one executable with V11/V20/V21 profiles. V11 is multiplayer-only. Host bipeds must exist in the map. Full unified Linux source is [`steam-unified/`](steam-unified); the Switch source remains separate.

**Custom Maps & Mods** imports supported map/ZIP files and required owner resource companions, downloads maps through HaloNet, and selects campaign/host characters. OpenSauce/Chimera engine extensions need individual native ports. [Content scope and verification limits](releases/v0.2.7-steamdeck-unified7/SUPPORTED-MODS.md).

The Deck build is native Linux, not Proton. It needs your own prepared compatible Halo CE maps and support files. No game maps, executable, ROM, shader instruction data, console keys or saves are supplied. See the release notes for precise installation and verification status.

## Server discovery

Campaign, multiplayer, all-community, current/legacy native versions, broker and community-announced sources remain separately filtered. Native joins use the same executable. Classic PC/CE snapshots remain separate and are not native-joinable. Online co-op uses remote players without a second controller.

## Play with your own game

1. Download the setup app from [Releases](https://github.com/Jmesmykil/NxHalo-Releases/releases).
2. Run the importer with your own compatible original Xbox Halo image or extracted game folder and select your mounted Switch SD root. It prepares the required game data locally.
3. Install the Switch runtime through DBI and launch Halo CE from HOME. Profile33 is a separate title from Build14; keep Build14 installed if you want the stable rollback available.

The Switch must already be configured to run this homebrew title. Setup apps are available for **Windows x64** and **Apple Silicon macOS**. These preview apps are unsigned. Other computers can use the importer source with Python 3.10 or newer with Tk. Halo PC, Custom Edition, Anniversary, MCC and Switch 2 are outside the Switch release.

## Validation status

Unified Preview 2.7 is installed on the Deck with a verified executable hash. Content/network menus were captured; archive fixtures and native linking passed. Private host initialization followed a reproduced and corrected keepalive memory fault. Real CE gameplay, two-client interoperability, alternate-character matches and full download/join remain unverified. The prior synthetic roster capture does not prove a 128-player battle.

## Source

The [`source/`](source) tree contains the shared game source with the Steam Deck Preview 2.6 responsive lobby, selected-player inspection and console moderation actions, public lobby browser and map download-and-join flow; [build guidance](source/RELEASE-SOURCE.md) describes the native Linux target. The exact Profile33 package source is `NxHalo-Profile33-runtime-source.zip` in the [Profile33 release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.1.11-p33). These sources omit game data, shader instruction tokens and console keys.

## Updates and support

Keep your imported game data, saves and settings. Back up the SD game folder before installing another build. Review the selected release notes for title ID and compatibility details.

For a bug report, include the release version, map or mission, player count, wired or Wi-Fi connection and reproducible steps. Remove personal information from logs before posting. Do not upload game images, maps, executables, keys or saves.

This is an independent community project and is not endorsed by the original game or console publishers.

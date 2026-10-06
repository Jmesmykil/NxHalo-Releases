# NxHalo — Halo: Combat Evolved for Switch and Steam Deck

NxHalo is a native Halo: Combat Evolved port with separate Nintendo Switch and Steam Deck/Linux builds. Releases include offline setup tools and player support. Supply your own legally obtained game data; no game content or console keys are included.

## Current builds

**Newest published Switch build: Profile33 / v0.1.11-p33 (pre-release, network protocol 11).** The [Profile33 release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.1.11-p33) contains the Switch runtime, matching source, setup apps, checksums and validation records. It includes cross-console multiplayer, public internet lobby browsing and custom campaign characters. The creator's current practical match-size estimate is about 40 players. That is not a measured simultaneous-player acceptance result: public runs discovered lobbies listing 32 and 36 players. Profile33 was staged/read back through UMS; that alone does not establish physical Switch install or runtime acceptance.

**Stable Switch build: Build14 / v0.1.1.** [Build14](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.1.1-build14) remains the stable campaign baseline and latest non-prerelease. It is a separate title from Profile33 and remains available for rollback.

**Newest Steam Deck/Linux feature build: Preview 2.3 / v0.2.3 (pre-release, network protocol 11).** The [Preview 2.3 release](https://github.com/Jmesmykil/HolaDeck/releases/tag/v0.2.3-steamdeck-preview3) adds selected-player inspection to the responsive multiplayer waiting lobby. It retains public cross-console browsing, Co-op Campaign labeling, the scrollable 128-entry roster, and opt-in Custom Edition map download-and-join. About 40 players remains the creator's practical match-size estimate; 128 is display capacity, not a tested match size. Preview 2.3 was installed on SteamOS 3.8.28 and process-smoked for 12 seconds; logs reached main-menu music and frame 626, but visible output was not verified.

The Deck build is native Linux, not Proton. It needs your own prepared compatible Halo CE maps and support files. No game maps, executable, ROM, shader instruction data, console keys or saves are supplied. See the release notes for precise installation and verification status.

## Play with your own game

1. Download the setup app from [Releases](https://github.com/Jmesmykil/NxHalo-Releases/releases).
2. Run the importer with your own compatible original Xbox Halo image or extracted game folder and select your mounted Switch SD root. It prepares the required game data locally.
3. Install the Switch runtime through DBI and launch Halo CE from HOME. Profile33 is a separate title from Build14; keep Build14 installed if you want the stable rollback available.

The Switch must already be configured to run this homebrew title. Setup apps are available for **Windows x64** and **Apple Silicon macOS**. These preview apps are unsigned. Other computers can use the importer source with Python 3.10 or newer with Tk. Halo PC, Custom Edition, Anniversary, MCC and Switch 2 are outside the Switch release.

## Validation status

Preview 2.3 completed a physical Deck install and 12-second process smoke. Visible output, lobby interaction, multiplayer, resolution coverage and map-download joining remain unverified.

## Source

The [`source/`](source) tree contains the shared game source with the Steam Deck Preview 2.3 responsive lobby, selected-player inspection and map-browser changes; [build guidance](source/RELEASE-SOURCE.md) describes the native Linux target. The exact Profile33 package source is `NxHalo-Profile33-runtime-source.zip` in the [Profile33 release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.1.11-p33). These sources omit game data, shader instruction tokens and console keys.

## Updates and support

Keep your imported game data, saves and settings. Back up the SD game folder before installing another build. Review the selected release notes for title ID and compatibility details.

For a bug report, include the release version, map or mission, player count, wired or Wi-Fi connection and reproducible steps. Remove personal information from logs before posting. Do not upload game images, maps, executables, keys or saves.

This is an independent community project and is not endorsed by the original game or console publishers.

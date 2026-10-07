# NxHalo — Halo: Combat Evolved for Switch and Steam Deck

NxHalo is a native Halo: Combat Evolved port with separate Nintendo Switch and Steam Deck/Linux builds. Releases include offline setup tools and player support. Supply your own legally obtained game data; no game content or console keys are included.

## Current builds

**Newest published Switch build: Profile33 / v0.1.11-p33 (pre-release, network protocol 11).** The [Profile33 release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.1.11-p33) contains the Switch runtime, matching source, setup apps, checksums and validation records. It includes cross-console multiplayer, public internet lobby browsing and custom campaign characters. The creator's current practical match-size estimate is about 40 players. That is not a measured simultaneous-player acceptance result: public runs discovered lobbies listing 32 and 36 players. Profile33 was staged/read back through UMS; that alone does not establish physical Switch install or runtime acceptance.

**Stable Switch build: Build14 / v0.1.1.** [Build14](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.1.1-build14) remains the stable campaign baseline and latest non-prerelease. It is a separate title from Profile33 and remains available for rollback.

**Newest Steam Deck/Linux build: Preview 2.6 / v0.2.6 (pre-release, protocol 11).** The [release](https://github.com/Jmesmykil/HolaDeck/releases/tag/v0.2.6-steamdeck-preview6) routes the Xbox menu into the responsive waiting lobby, follows the actual window shape, fixes the game-mode label and hardens nested Custom Edition map ZIP import. It retains cross-console/browser discovery, player inspection and confirmed remote-console Kick/Ban actions. Omarchy built the runtime, and its installed Deck hash was verified. Physical captures show the wide lobby and 128/128 synthetic lobby players; a 128-player battle, controller moderation and internet map-download/join remain unverified.

**Community online campaign: OpenCE, protocol 21.** The same release provides a separate community campaign runtime and source, with upstream online campaign synchronization and PUBLIC/PRIVATE lobbies. It is installed alongside NxHalo with an isolated save root and has reached its main menu on the Deck. Two-player campaign gameplay remains unverified. All peers need compatible protocol-21 builds; retain NxHalo for protocol-11 Switch/native/browser rooms.

The Deck build is native Linux, not Proton. It needs your own prepared compatible Halo CE maps and support files. No game maps, executable, ROM, shader instruction data, console keys or saves are supplied. See the release notes for precise installation and verification status.

## Server discovery and online campaign

Preview 2.6 separates **JOIN CAMPAIGN LOBBIES**, **HOST ONLINE CAMPAIGN**, **MULTIPLAYER SERVERS**, and **ALL COMMUNITY SERVERS**. Online campaign does not require a second local controller; local split-screen remains a separate option. NxHalo's Multiplayer > ONLINE CAMPAIGN opens the installed OpenCE component.

Source/version filters keep current V21, legacy V11-20, broker-published, community-announced, classic Halo CE, and classic Halo PC listings separate. The live directory refreshes every ten seconds alongside all four upstream brokers. Native legacy invites launch the sibling `Chupathingy-Legacy` client included in the release. Extract it beside `OpenCE-Campaign` and the NxHalo runtime.

Classic master snapshots contain 253 CE and 97 PC addresses from October 6, 2026. They are listed separately with unknown player counts and activity; joining them requires a classic client. These are all discovered public sources, not a claim to enumerate private or unpublished servers.

The installed Deck build displayed active native rooms and reached live multiplayer gameplay. Final campaign routing and classic-client feedback are installed for the next launch; a two-player campaign run and a legacy real-match handoff remain unverified. Friends/follow and proximity voice remain unimplemented.

## Play with your own game

1. Download the setup app from [Releases](https://github.com/Jmesmykil/NxHalo-Releases/releases).
2. Run the importer with your own compatible original Xbox Halo image or extracted game folder and select your mounted Switch SD root. It prepares the required game data locally.
3. Install the Switch runtime through DBI and launch Halo CE from HOME. Profile33 is a separate title from Build14; keep Build14 installed if you want the stable rollback available.

The Switch must already be configured to run this homebrew title. Setup apps are available for **Windows x64** and **Apple Silicon macOS**. These preview apps are unsigned. Other computers can use the importer source with Python 3.10 or newer with Tk. Halo PC, Custom Edition, Anniversary, MCC and Switch 2 are outside the Switch release.

## Validation status

Preview 2.6 has physical Deck installation, binary hash verification, visible wide-lobby output and a 128-player synthetic lobby capture. Seven map archive fixtures passed and source received independent review. Native multiplayer gameplay was captured on the Deck. Final filter changes, controller moderation, internet map download/join and two-player campaign gameplay still need acceptance.

## Source

The [`source/`](source) tree contains the shared game source with the Steam Deck Preview 2.6 responsive lobby, selected-player inspection and console moderation actions, public lobby browser and map download-and-join flow; [build guidance](source/RELEASE-SOURCE.md) describes the native Linux target. The exact Profile33 package source is `NxHalo-Profile33-runtime-source.zip` in the [Profile33 release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.1.11-p33). These sources omit game data, shader instruction tokens and console keys.

## Updates and support

Keep your imported game data, saves and settings. Back up the SD game folder before installing another build. Review the selected release notes for title ID and compatibility details.

For a bug report, include the release version, map or mission, player count, wired or Wi-Fi connection and reproducible steps. Remove personal information from logs before posting. Do not upload game images, maps, executables, keys or saves.

This is an independent community project and is not endorsed by the original game or console publishers.

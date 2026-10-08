## Current Steam Deck/Linux update: 2.9.4 native voice foundation

[Download runtime and matching source](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9.4-steamdeck-runtime-nativevoice1). Executable SHA-256: `4a083770b6920d2a1f7372ba701967bcf02b840e0d2c3e9bfe7d55bf646964c0`. This exact build is installed on the Steam Deck.

Adds default-OFF compatible-native proximity voice with authenticated joined-player routing, spatial mixing, push-to-talk V/X/C, and expiry after release. Audio Settings spinner arrow hitboxes now match their drawn controls; click OK to save. Compatible native clients are required for voice; unchanged clients and the Switch runtime do not gain voice from this release.

Exact candidate passed a private Deck GPU multiplayer session with a native Linux peer. Two actual native clients passed generated-PCM capture-to-encrypted-transport-to-spatial-mixer testing with dummy audio and waveform analysis: 605 frames received/mapped, zero mapping drops, no sender loopback, and expiry after PTT release. Actual menu writes and fresh-process reload passed. No physical microphone or external audio was used.

Retains confirmed controller fixes, map download/autojoin, match customization and co-op lifecycle fixes. Voice is still a foundation: physical microphone/human PTT, third-client fanout, persistent Friends, natural campaign completion, physical Switch acceptance and real 128-player capacity remain open. The mixer has 16 active stream slots; the tunnel rate budget is roughly five continuously transmitting speakers, not 128-person voice. [Release evidence](releases/v0.2.9.4-steamdeck-runtime-nativevoice1/release-notes.md).

## Current additive releases

The latest Nintendo Switch build is **Unified Switch Co-op Lifecycle Preview 2.9.4 / v0.2.9.4-switch-coopfix1-preview**. The [standalone NRO and matching filtered source archive](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9.4-switch-coopfix1-preview) include the shared host-authoritative co-op pause actions, next-map lobby selection, and preservation of player cap and friendly-fire settings. NRO SHA-256: `e98eea315b6733ff1b3d2f6d3b19d4539332540b32157181ec5d8ac328e2c2d0`; source ZIP SHA-256: `13743eeeacd5bd4007fe18d279b5c5e01f4c9fef9a8c62f274f026d9f99d3dd6`. Guest and NRO cross-builds succeeded. The NRO was staged and read back at `switch/NxHalo-Content29/halo.nro`; macOS declined the unmount, so the volume remains mounted. It has not been launched on Switch hardware. See [release notes](releases/v0.2.9.4-switch-coopfix1-preview/RELEASE-NOTES.md) and [build evidence](releases/v0.2.9.4-switch-coopfix1-preview/BUILD-EVIDENCE.txt).

The latest Linux/Steam Deck build is **v0.2.9.4 co-op lifecycle update**. Download the [runtime and matching filtered source snapshot](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9.4-steamdeck-runtime-coopfix1). Executable SHA-256: `05fb520d15722a1954dcd884702ebff49dddc59b1aada4b39f6770ac8fdefed0`; runtime archive SHA-256: `1b114487e283567a77acd5e3824b7835537b46a9cb42aab425027a9c43c3df2e`; source ZIP SHA-256: `84b4828dce4f7b1ec9f9ee82ba7ee9a64390e93f9d2e0410f3a7f1f49601b941`. The source is mirrored in [`steam-unified/`](steam-unified). The update makes co-op pause-menu Revert and Restart authoritative on the host, returns the host to the correct next-map lobby, and preserves the co-op cap and friendly-fire setting. It retains the controller, custom-map download/autojoin, and preset paths. This is a Linux runtime release and does not update the Switch NRO. See [release notes](releases/v0.2.9.4-steamdeck-runtime-coopfix1/release-notes.md) and [verification limits](releases/v0.2.9.4-steamdeck-runtime-coopfix1/VERIFICATION-EVIDENCE.md).

# NxHalo — Halo: Combat Evolved for Switch and Steam Deck

NxHalo is a native Halo: Combat Evolved port with separate Nintendo Switch and Steam Deck/Linux builds. Releases include offline setup tools and player support. Supply your own legally obtained game data; no game content or console keys are included.

## Current builds

**Newest Switch candidate: Unified Switch Co-op Lifecycle Preview 2.9.4 / v0.2.9.4-switch-coopfix1-preview (pre-release).** The [release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9.4-switch-coopfix1-preview) contains the standalone NRO, matching filtered source archive and cross-build evidence. NRO SHA-256: `e98eea315b6733ff1b3d2f6d3b19d4539332540b32157181ec5d8ac328e2c2d0`; source ZIP SHA-256: `13743eeeacd5bd4007fe18d279b5c5e01f4c9fef9a8c62f274f026d9f99d3dd6`. Guest and host cross-builds succeeded. The NRO was staged at `sdmc:/switch/NxHalo-Content29/halo.nro` and read back by hash; macOS declined to unmount the SD volume, which remains mounted. It has **not** been launched on Switch hardware. This is a Homebrew Menu NRO, not an NSP or HOME-menu forwarder. For game data, provide your own compatible Halo CE data under `sdmc:/switch/halo/`, including its `maps/` directory; the preview recognizes data builds `01.01.14.2342` and `01.10.12.2276`. Saves are written under `sdmc:/switch/halo/save-community24/`. No game maps, resources, keys or saves are included. The previous [Switch Content Preview 2.9.4](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9.4-switch-content-preview) remains available.

**Previous unified Switch candidate: Preview 2.8 / v0.2.8-switch-unified8 (pre-release).** The [candidate release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.8-switch-unified8) provides a standalone NRO and matching source/build records. NRO SHA-256: `5b18c2b201ee6ab62785911f307197bd7ba13543f6be64161f82b86c2013e376`. This candidate has not been staged through UMS or installed/run on physical Switch hardware; hardware acceptance is unverified. It uses the same title ID (`010048414C210000`) as Profile33, so retain the Profile33 NRO separately for rollback. It is not an NSP and includes no full-memory title override. Game data is read from `sdmc:/switch/halo` when the candidate has no usable `romfs:/maps/ui.map`; saves use `sdmc:/switch/halo/save-community24`. No game maps, resources, keys or saves are included. See [`switch-unified/BUILD-SWITCH.md`](switch-unified/BUILD-SWITCH.md).

**Previous Switch network-profile candidate: Profile33 / v0.1.11-p33 (pre-release, network protocol 11).** The [Profile33 release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.1.11-p33) remains available with its setup apps, checksums and validation records. It includes cross-console multiplayer, public internet lobby browsing and custom campaign characters. The creator's current practical match-size estimate is about 40 players. That is not a measured simultaneous-player acceptance result: public runs discovered lobbies listing 32 and 36 players. Profile33 was staged/read back through UMS; that alone does not establish physical Switch install or runtime acceptance.

**Stable Switch build: Build14 / v0.1.1.** [Build14](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.1.1-build14) remains the stable campaign baseline and latest non-prerelease. It is a separate title from Profile33 and remains available for rollback.

See the current Steam Deck/Linux update at the top of this page.

Menu routes: Multiplayer → Join/Create/Local Split/Edit Game Types; Create Game → Game Modes (Combat/Factions/Race/Standard) or Network Options; Custom Maps & Mods → Custom Maps → Browse; Characters remains separate. The editor saves game types, weapons, players, vehicles and scoring. Zombies melee uses a sword on maps with the required loaded tag/assets and falls back to stock Oddball melee otherwise. Map-dependent visuals and loadouts remain bounded; failed loadout grants preserve inventory. A two-client Zombies end-round was verified on predecessor 0f6, but other presets have not been broadly tested in live matches.

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

Two CE catalog maps and a two-client Zombies end-round case were tested on predecessor 0f6; the release evidence labels those hashes separately. Visible texture override and 128-slot roster checks used separate earlier builds; the roster used synthetic peers, not 128 real users. Full campaign completion, Persistent Friends, physical microphone voice acceptance and broad legacy interoperability remain unverified. See [2.9.4 verification evidence](releases/v0.2.9.4-steamdeck-runtime/VERIFICATION-EVIDENCE.md) and [native content guide](releases/v0.2.9.4-steamdeck-runtime/NATIVE-CONTENT.md).
## Source

The current Switch Co-op Lifecycle Preview 2.9.4 NRO, source archive and build evidence are attached to its [prerelease](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9.4-switch-coopfix1-preview); the source tree is [`switch-unified/`](switch-unified). The earlier Switch Content Preview 2.9.4 remains available [here](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9.4-switch-content-preview). The earlier Switch 2.8 source and [v0.2.8-switch-unified8 release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.8-switch-unified8) remain available. The complete Linux source snapshot for Steam Deck Runtime Preview 2.9.4 is in [`steam-unified/`](steam-unified), with its matching filtered source ZIP attached to the [Steam v0.2.9.4 prerelease](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9.4-steamdeck-runtime). Earlier Steam 2.9.3, 2.9.2, 2.9.1 and 2.9 releases remain available. The earlier Switch/Profile33 source remains under [`source/`](source). These snapshots omit game data, resource companions, shader instruction tokens, console keys and saves.
## Updates and support

Keep your imported game data, saves and settings. Back up the SD game folder before installing another build. Review the selected release notes for title ID and compatibility details.

For a bug report, include the release version, map or mission, player count, wired or Wi-Fi connection and reproducible steps. Remove personal information from logs before posting. Do not upload game images, maps, executables, keys or saves.

This is an independent community project and is not endorsed by the original game or console publishers.

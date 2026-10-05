# NxHalo — Halo: Combat Evolved for Nintendo Switch

NxHalo is a native Halo: Combat Evolved port for the original Nintendo Switch. This repository publishes runtime packages, offline game-data setup tools, source snapshots and player support.

## Current builds

**Newest Switch build: Profile33 / v0.1.11-p33 (pre-release, network version 11).** Download the [Profile33 release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.1.11-p33) for the Switch runtime, matching source snapshot, setup apps, manifest, validation records and checksums. Profile33 includes cross-console multiplayer, in-game public-lobby browsing and custom campaign characters. The creator's current approximate match limit is roughly 40 players. That estimate is not a measured acceptance result: recent public discovery runs saw lobby populations of 32 and 36, which do not establish simultaneous players or sustained match performance. A 128-player workload has not been validated.

**Stable Switch build: Build14 / v0.1.1.** The [Build14 release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.1.1-build14) remains the stable campaign baseline and GitHub's latest non-prerelease. It retains the camera/vehicle corrections and RAM-backed transient checkpoints. Multiplayer is unsupported in Build14; use Profile33 for the newer network code. Build14 remains available as a rollback. Profile33 uses a different title ID (`010048414C210000`) from Build14 (`010048414C4F0000`), so it is a separate title, not an in-place update.

**Steam Deck / Linux: v0.2.0 preview 1 (pre-release, network version 11).** The [current Deck release](https://github.com/Jmesmykil/HolaDeck/releases/tag/v0.2.0-steamdeck-preview1) is the newest published Deck package. It has local and internet multiplayer and experimental campaign characters. Its package has not been replaced; see its release notes for the measured tests and remaining gaps. The Deck and Profile33 Switch builds share network version 11. A physical Switch-to-Deck match on Profile33 has not yet been verified.

## Play with your own game

1. Download the Profile33 runtime and offline importer from [Releases](https://github.com/Jmesmykil/NxHalo-Releases/releases).
2. Run the importer with your own compatible original Xbox Halo image or extracted game folder and select your mounted Switch SD root. It prepares the required game data locally.
3. Install the runtime NSP through DBI and launch Halo CE from HOME. Profile33 is a separate title from Build14; keep Build14 installed if you want the stable rollback available.

The Switch must already be configured to run this homebrew title. No ROM, game maps, executable, console keys or personal saves are included. The recommended player flow needs no keys, compiler or packaging tool.

Start with [the player guide](docs/START_HERE.md). Setup apps are available for **Windows x64** and **Apple Silicon macOS**. These preview apps are unsigned. Other computers can use the importer source with **Python 3.10 or newer with Tk**. Halo PC, Custom Edition, Anniversary, MCC and Switch 2 are outside this release.

## Validation status

Profile33's matching NSP is SHA-256 verified and its release carries the validation receipt. The receipt records that the installer was staged and read back from SD; it does **not** establish installation or physical runtime acceptance. Physical performance, visual-effects correctness, campaign-route coverage and real 100–128-player workload remain open. See the Profile33 release notes and validation files for the exact evidence and limits.

Build14 hardware records cover selected scenes and checkpoint writes, not a full-campaign certification or fixed frame rate. Shader compilation can cause first-use stutter; battle performance, loading display, audio, shields and animation remain under test.

## Source

The [`source/`](source) folder contains the public Build14 source snapshot. The exact Profile33 source is included as `NxHalo-Profile33-runtime-source.zip` in the [Profile33 release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.1.11-p33). Both omit game data, shader instruction tokens and console keys. Supply those from your own copy of the game.

## Updates and support

Keep your imported game data, saves and settings. Back up your SD game folder before installing another build. Review the selected release notes for title-ID and compatibility details before installing.

For a bug report, include the release version, mission, checkpoint or encounter, handheld/docked mode and reproducible steps. Remove personal information from logs before posting. Do not upload game images, maps, executables, keys or saves.

This is an independent community project and is not endorsed by the original game or console publishers.

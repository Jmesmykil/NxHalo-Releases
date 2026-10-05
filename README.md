# NxHalo — Nintendo Switch campaign preview

A native Halo: Combat Evolved port for the original Nintendo Switch. This repository contains public releases, offline setup tools, and support information. Development stays separate.

**Latest Switch release: Build14 / v0.1.1 campaign preview.** Download the runtime and offline setup apps from the [Build14 release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.1.1-build14). The release package contains `Halo_CE_Runtime.nsp`, Windows x64 and Apple Silicon macOS setup ZIPs, importer and runtime source archives, receipts, manifest, and SHA-256 checksums. Build14 retains the camera/vehicle corrections and adds RAM-backed transient checkpoints. Multiplayer is unsupported; campaign testing and optimization continue. The earlier [Build13 baseline](releases/build13-baseline.json) remains available for comparison.

**Steam Deck preview 1 (pre-release).** [NxHalo for the Steam Deck](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.0-steamdeck-preview1) is a native Linux build on network version 11 with local and internet multiplayer and experimental campaign characters. It needs your own prepared game data; see its [release notes](releases/steamdeck-preview1/release-notes.md) for what has and has not been tested. Build14 remains the Switch campaign build.

## Play with your own game

1. Download the runtime installer and offline importer from [Releases](https://github.com/Jmesmykil/NxHalo-Releases/releases).
2. Run the importer and choose your own compatible original Xbox Halo image or extracted game folder, then choose your mounted Switch SD root. The importer prepares the maps and required shader data locally.
3. Install the small runtime NSP through DBI and launch Halo CE from HOME.

The Switch must already be configured to run this homebrew title. No ROM, game maps, game executable, console keys, or personal saves are supplied. The recommended player flow needs no keys, compiler, or packaging tool.

Start with [the player guide](docs/START_HERE.md). Standalone setup apps are available for **Windows x64** and **Apple Silicon macOS**. Extract the ZIP and open `NxHalo-Setup.exe` or `NxHalo-Setup.app`; keep the Windows app folder intact. These preview apps are unsigned. Other computers can use the importer source with **Python 3.10 or newer with Tk**. Use the included setup guide for exact supported image formats and installation steps. Halo PC, Custom Edition, Anniversary, MCC, and Switch 2 are outside this preview.

## What has been tested

Hardware testing on Build13 scenes demonstrates smoother Pelican flight and stable Warthog handling. Small lag spikes remain during large ground battles. This reflects tested scenes, not a full-campaign certification or a promise of a locked frame rate.

Build14 hardware logs recorded four successful 16 MiB RAM checkpoint saves in 6.46–7.04 ms; no restore event appeared in that captured run. Full campaign completion, all checkpoint/resume/update cases, audio, shields, animations, and loading display remain under testing. The public package moves game shader data into local import; its separate package/startup acceptance is recorded in release notes.

## Source

The [`source/`](source) folder holds the game and Switch host source that the latest release was built from, with build notes in [`source/RELEASE-SOURCE.md`](source/RELEASE-SOURCE.md). It contains no game data: no disc image, maps, executable, shader instruction data or keys. You supply those from your own copy of the game.

## Updates and support

Keep your imported game data, saves, and settings. Back up your SD game folder before updates. Install the new runtime over the same title; do not erase your existing game folder.

For a bug report, include the release version, mission, checkpoint or encounter, handheld/docked mode, and reproducible steps. Review logs for personal information before posting an excerpt. Do not upload game images, maps, executables, keys, or saves to this repository.

Exact checksums, source snapshots, dependency notices, known issues, and test coverage accompany the release. This is an independent community project and is not endorsed by the original game or console publishers.

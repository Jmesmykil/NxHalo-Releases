# NxHalo — Nintendo Switch campaign preview

A native Halo: Combat Evolved port for the original Nintendo Switch. This repository contains public releases, offline setup tools, and support information. Development stays separate.

**First release: Build13 / v0.1.0 experimental preview.** Download the files attached to the [Build13 preview release](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.1.0-build13). Campaign testing and optimization continue. Multiplayer is unsupported.

## Play with your own game

1. Download the runtime installer and offline importer from [Releases](https://github.com/Jmesmykil/NxHalo-Releases/releases).
2. Run the importer and choose your own compatible original Xbox Halo image or extracted game folder, then choose your mounted Switch SD root. The importer prepares the maps and required shader data locally.
3. Install the small runtime NSP through DBI and launch Halo CE from HOME.

The Switch must already be configured to run this homebrew title. No ROM, game maps, game executable, console keys, or personal saves are supplied. The recommended player flow needs no keys, compiler, or packaging tool.

Start with [the player guide](docs/START_HERE.md). Standalone setup apps are available for **Windows x64** and **Apple Silicon macOS**. Extract the ZIP and open `NxHalo-Setup.exe` or `NxHalo-Setup.app`; keep the Windows app folder intact. These preview apps are unsigned. Other computers can use the importer source with **Python 3.10 or newer with Tk**. Use the included setup guide for exact supported image formats and installation steps. Halo PC, Custom Edition, Anniversary, MCC, and Switch 2 are outside this preview.

## What has been tested

The creator reports substantially smoother Pelican flight and no Warthog trouble in the Build13 scenes tested. Small lag spikes remain during large ground battles. This is a report from tested scenes, not a full-campaign certification or a promise of a locked frame rate.

Full campaign completion, all checkpoint/resume/update cases, audio, shields, animations, and loading display remain under testing. The public package moves game shader data into local import; its separate package/startup acceptance is recorded in release notes.

## Updates and support

Keep your imported game data, saves, and settings. Back up your SD game folder before updates. Install the new runtime over the same title; do not erase your existing game folder.

For a bug report, include the release version, mission, checkpoint or encounter, handheld/docked mode, and reproducible steps. Review logs for personal information before posting an excerpt. Do not upload game images, maps, executables, keys, or saves to this repository.

Exact checksums, source snapshots, dependency notices, known issues, and test coverage accompany the release. This is an independent community project and is not endorsed by the original game or console publishers.

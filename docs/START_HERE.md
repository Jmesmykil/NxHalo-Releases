# Start here

**Status: v0.1.0 experimental campaign preview.** Native Build13 gameplay is tested; this download supplies the importer source. Standalone desktop apps and clean public setup tests remain in progress. Read the
[known issues](COMPATIBILITY.md) before expecting a finished campaign port.

The recommended flow is **your Xbox image → automatic SD import → small app
installed through DBI → HOME → play**. Users need no console keys, compiler,
or NSP packaging tools. Game data is supplied by you and stays on your computer
or SD; this project supplies no ROM downloads.

## What you need

- An original Nintendo Switch already configured for this homebrew title and DBI.
  Stock firmware and Switch 2 are outside this preview.
- Your own original **Xbox Halo: Combat Evolved** XDVDFS/XISO image, extracted
  game folder containing `maps/` and `default.xbe`. Halo PC, Custom Edition,
  Anniversary, and MCC use different data formats.
- A computer and enough local/SD space for the roughly 1.8 GB map dataset.
- The release assets from [NxHalo Releases](https://github.com/Jmesmykil/NxHalo-Releases/releases).
  This importer source requires Python 3.10+ with Tk. Check the release notes for runtime availability; standalone desktop apps are not included.

Xbox cache build `01.10.12.2276` (NTSC) was tested on Switch. The engine also
accepts `01.01.14.2342` (PAL), with device testing pending. A filename ending in
`.iso` is not a format guarantee; compressed archives and unsupported disc
layouts require local extraction/conversion first. The tool explains what it can
recognize and checks the full required map set. Header checks are not complete
validation of every compressed payload.

## Import once

1. Open the NxHalo setup tool and choose your Xbox image or complete game folder.
2. Choose the root of your mounted Switch SD explicitly. The tool does not guess
   drives or eject them. Alternatively, export into a new local data folder first.
3. Choose **Prepare game data**. The importer extracts only the required maps,
   checks the copied bytes, and adds an original loading image.
4. Wait for success. Existing game data that differs is preserved and reported;
   saves, settings, and other Switch files are outside the import target.

This flow is offline and key-free. The only game-data destination is
`switch/halo/maps/`. Keep the local data receipt for support. Do not share the
exported data folder, ROM, or maps publicly.

If you choose local export, the finished folder contains `maps/` and a receipt.
Copy **only that maps folder** into `switch/halo/` on your SD. Keep existing saves
and other applications. Use the automatic SD import path to avoid manual placement.

## Install and play

Copy the reviewed **small, game-free** `Halo_CE_Runtime.nsp` to `switch/install/`, safely
eject the SD after all transfers finish, open DBI, choose **Browse SD Card**, and
install it to SD. Exit DBI and launch **Halo CE** from HOME.

DBI's **Run MTP responder** also exposes **SD install** on the computer: copying
the app NSP there installs it directly. Names may vary by language/version.
Both paths are described in the [DBI guide](https://github.com/rashevskyv/dbi/blob/main/README.md).

Normal play needs no connected computer, terminal, or Homebrew Menu shortcut.
The runtime reads imported SD maps and creates settings/saves automatically.
This source path has been checked; the new small-package/importer combination
still needs a physical first-install test.

## Update later

Keep the imported data. Install only the new small app version through DBI over
the same title. Back up `sdmc:/switch/halo/` first. Reimport only if changing game
data or a release explicitly requires it. Resume across relaunch/updates is still
a device acceptance test, not a verified promise.

An [advanced full-NSP builder](../installer/README.md) remains for
local developer/private use. It needs the user's runtime kit, packer, and keys;
those are not requirements for the recommended player flow.

## Original shader data

The public runtime also needs `switch/halo/maps/shaders.bin`. The importer finds the supported 34,628-byte shader set inside your own original `default.xbe`, checks its complete SHA-256, and copies only that shader set. It never copies the original executable to the SD. Neither the game shaders nor the executable are bundled in a public download. Select a full game image or extracted game folder containing both `maps/` and `default.xbe`; a maps-only folder is insufficient.

Original Xbox NTSC build `01.10.12.2276` shader extraction was verified. Other executables must contain the exact supported shader set to pass. PAL gameplay remains unvalidated. If your SD has matching older maps and the same generated loading image, the tool adds only the missing `shaders.bin`; it keeps all existing maps, receipts, saves and settings. Different existing data is preserved with repair instructions. Keep all extracted game data private.

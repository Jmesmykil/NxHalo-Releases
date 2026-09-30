# Local installer preparation preview

The recommended flow uses a small, separate runtime installer and your own original Xbox Halo game data. Install the runtime NSP through DBI once, prepare your game's maps onto the selected SD, then launch from HOME. Game preparation needs no private keys or packer, downloads nothing, and never copies game executables or saves.

This is a source preview requiring **Python 3.10+ with Tk**. It is not a packaged Windows or macOS application. No runtime NSP, runtime kit, or packaging executable is included in this directory. The advanced full-installer mode additionally needs a compatible local **hacbrewpack** executable and private keys.

Open `Launch.command` on macOS, `Launch-windows.cmd` on Windows, or `Launch-linux.sh` on Linux. You can also run `python3 wizard.py`. Select your game image or extracted complete game folder, then either explicitly choose the SD root or export a portable data folder. The tool never discovers or ejects drives. It writes only `switch/halo/maps` on a selected SD, preserving saves and configuration. Existing maps must match all expected hashes; different maps are kept with instructions to back them up or rename them yourself before retrying.

Portable output folders must be new. The tool calculates free-space requirements before copying and verifies every copied map without replacing an earlier folder. Full-installer work files use the work disk you choose, rather than an implicit system temporary disk. Keep generated game-data folders and full installers private.

## Supported game input

- Linear 2048-byte-sector Xbox XDVDFS images, recognized by both media signatures at data-partition offsets `0`, `0x0fd90000`, `0x02080000`, or `0x18300000`.
- An extracted game folder containing `maps/`, with its original `default.xbe`.
- All 24 named retail maps with Xbox cache version 5 and one consistent build: `01.10.12.2276` or `01.01.14.2342`. The current physical Switch test used `01.10.12.2276`; accepting a header does not establish full gameplay compatibility for another build.

Compressed archives, CSO/CCI, raw 2352-byte-sector layouts, PC Halo, Halo Custom Edition, and Anniversary data are unsupported. Retail Xbox maps may themselves be compressed; they are copied intact for the game's existing decompressor. AppleDouble `._*` files and unrelated game files are excluded. Header checks do not validate every compressed map payload or prove gameplay.

The XDVDFS layout follows [XboxDev/extract-xiso](https://github.com/XboxDev/extract-xiso/blob/master/extract-xiso.c) and the directory tree units in [xdvdfs-core](https://github.com/antangelo/xdvdfs/blob/main/xdvdfs-core/src/read/dirent_table.rs). The parser checks region bounds, tree cycles, duplicate names, directory record overlaps, file/directory overlaps, safe names, and resource limits before extracting only the required maps.

## Command line

Inspect a game without keys or writes:

```sh
python3 installer.py inspect-game "/path/to/your/Halo.iso"
```

Export game data without keys or a packer:

```sh
python3 installer.py export-data --game "/path/to/your/Halo.iso" --output "/path/to/new/Halo Data"
```

Output contains `maps/` (24 maps plus the original loading image) and a privacy-safe `data-receipt.json`. Copy `maps/` to `switch/halo/maps` later, without replacing a different maps folder.

Or prepare an explicitly selected mounted Switch SD:

```sh
python3 installer.py prepare-sd --game "/path/to/your/Halo.iso" --sd-root "/path/to/mounted/Switch SD"
```

This requires the selected root's existing `switch/` folder. Matching maps are checked and left unchanged. A new maps folder includes `data-receipt.json`; existing different maps cause a clear stop. Saves and configuration are never opened or replaced. The SD remains connected.

Advanced: build a private full NSP into a new output folder:

```sh
python3 installer.py build \
  --game "/path/to/your/Halo.iso" \
  --runtime-kit "/path/to/runtime-kit" \
  --keys "/path/to/your/private/prod.keys" \
  --packer "/path/to/local/hacbrewpack" \
  --workspace "/path/to/large/work-disk" \
  --output "/path/to/new/Halo CE Installer"
```

If `--packer` is omitted, the tool checks `tools/hacbrewpack` (`.exe` on Windows), then `PATH`. The packer receives the selected key path locally. Its raw output is discarded because it may contain sensitive diagnostics. Build errors report the failed stage without revealing key contents or full private input paths.

Successful output contains `Halo_CE.nsp` and `build-receipt.json`. The receipt records the runtime version, required map names and hashes, and installer/content hashes. It contains no absolute input or private key paths. NSP validation checks PFS0 bounds, exactly three encrypted NCA records, and each NCA's SHA-256 filename prefix. It does not decrypt and independently verify the packed RomFS or establish native gameplay acceptance.

On filesystems that support it, the completed output folder is published with an atomic exclusive rename. macOS exFAT/FAT drivers can reject that operation. The safe fallback creates a new destination folder exclusively, copies and hash-checks each file with exclusive creation, and writes the receipt last; whole-folder publication is **not atomic** in that case. On failure it removes only entries it created and preserves unrelated files. Do not disconnect a disk or use the output before the wizard reports completion. The tool reserves space for both staged and publication copies. Existing folders are never reused or replaced.

The installer also generates an original procedural grayscale `maps/loading.tga`: 320×240, uncompressed 24-bit, bottom-up rows. Its provenance and hash are recorded separately from the user-supplied maps. Its appearance still requires a physical Switch test.

## Maintainer runtime kit contract

Create a **new** local runtime kit from five explicit inputs:

```sh
python3 installer.py create-runtime-kit \
  --host "/path/to/halo.nso" \
  --npdm "/path/to/halo.npdm" \
  --nacp "/path/to/halo.nacp" \
  --icon "/path/to/icon.jpg" \
  --guest "/path/to/halo_guest.elf" \
  --build-id "native-build-marker" \
  --output "/path/to/new/runtime-kit"
```

Only these files are staged; extra assets, keys, and symbolic links reject the kit. Benign `.DS_Store` and `._*` metadata files are ignored:

```text
runtime-manifest.json
exefs/main
exefs/main.npdm
control/control.nacp
control/icon_AmericanEnglish.dat
romfs/halo_guest.elf
```

`runtime-manifest.json` has exactly `schema` (integer `1`), `title_id` (`010048414c4f0000`), `build_id` (1–80 safe identifier characters), and `files` (the five exact paths, each with `bytes` and full lowercase `sha256`). The builder validates NSO/NPDM/guest formats, NPDM application title ID, control/icon lengths, and all manifest hashes. The guest uses an ELF64 little-endian AArch64 executable container with a bounded program-header/load layout; its ILP32 pointer ABI is separate from its ELF file class. A manifest detects accidental changes; it is not a publisher signature. Confirm the release's checksum separately. Creating a kit does not approve its publication or its runtime behavior.

Maintainers can privately build the map-free runtime NSP without a game input:

```sh
python3 installer.py build-runtime \
  --runtime-kit "/path/to/runtime-kit" \
  --keys "/path/to/your/private/prod.keys" \
  --packer "/path/to/local/hacbrewpack" \
  --workspace "/path/to/work-folder" \
  --output "/path/to/new/runtime-installer"
```

This produces `Halo_CE_Runtime.nsp` and a privacy-safe `build-receipt.json`; only the five allowlisted runtime files are packed, with no game maps. The runtime's source selects SD maps when its RomFS lacks `maps/ui.map`. This delivery mode still needs physical Switch acceptance before publication. End users do not supply keys or packer for the recommended map-free-runtime/game-import flow.

## Tests and release gaps

Run the synthetic fixture tests with:

```sh
python3 -m unittest discover -s tests -v
```

Fixtures contain no proprietary game payloads or real keys. GUI interaction, real full-disc variants, cross-platform launch wrappers, a packaged app, and a complete private installer round trip still need release acceptance on their intended systems.

## Original shader data

The public runtime also needs `switch/halo/maps/shaders.bin`. The importer finds the supported 34,628-byte shader set inside your own original `default.xbe`, checks its complete SHA-256, and copies only that shader set. It never copies the original executable to the SD. Neither the game shaders nor the executable are bundled in a public download. Select a full game image or extracted game folder containing both `maps/` and `default.xbe`; a maps-only folder is insufficient.

Original Xbox NTSC build `01.10.12.2276` shader extraction was verified. Other executables must contain the exact supported shader set to pass. PAL gameplay remains unvalidated. If your SD has matching older maps and the same generated loading image, the tool adds only the missing `shaders.bin`; it keeps all existing maps, receipts, saves and settings. Different existing data is preserved with repair instructions. Keep all extracted game data private.

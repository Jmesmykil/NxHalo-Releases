# NxHalo Steam Deck preview 1 — network version 11

Halo: Combat Evolved running natively on the Steam Deck (and other x86 Linux), built from the same game and network code as the NxHalo Switch port and brought up to the current community code, so it can join today's public games. This is a **pre-release**.

## What is in it

- **Multiplayer on network version 11**: local network games are found automatically; internet games through Join Game. Based on [ChupathingyCE/chupathingyce](https://github.com/ChupathingyCE/chupathingyce/tree/39cb857f) at commit 39cb857.
- **Campaign characters (experimental)**: play the campaign as a Marine, Grunt, Jackal, Elite, Hunter, the Flood forms, a Sentinel, 343 Guilty Spark or Captain Keyes, with an optional third-person camera. Press X on the main menu.
- NxHalo's loading display, diagnostics and settings fixes.

## Install

1. Prepare your own supported original Xbox Halo game data with the [Build14 setup app](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.1.1-build14). You need the complete `maps` folder, including `shaders.bin` and `loading.tga`. No game maps, ROM, shader instruction data or keys are supplied here.
2. Download `NxHalo-SteamDeck.tar.gz`, extract it, and put your `maps` folder at `assets/maps` inside it.
3. Run `launch_halo.sh`, or add it to Steam as a non-Steam game. It is a native Linux program: do not force a compatibility layer.
4. **On Wi-Fi, turn off Wi-Fi power management** (Settings → System → enable Developer Mode, then Developer → turn off "Enable Wi-Fi Power Management"). With it on we measured delays of several hundred milliseconds to the Deck; with it off, about 4 ms.

Full instructions are in `README-Deck.txt` inside the package.

## What has been tested

On a Steam Deck LCD (SteamOS 3.8):

- Local network matches against a Linux PC, in both host directions, over Ethernet, over Wi-Fi only, and with both connected: discovery, joining, shooting, vehicles and weapon pickup stayed in sync.
- Matches against the NxHalo Switch build (run in an emulator) in both host directions.
- Joining public internet games: connected to three different public hosts from the Deck, and one from a PC, in 2–4 seconds each.
- Campaign as an Elite on The Silent Cartographer.

The Deck multiplayer tests were run on the two builds immediately before this one; the final build differs from the last of them only by the character menu prompt and a log line, and was rechecked with a PC-to-PC local match and a public join.

On a Linux PC with the same build: all twelve campaign characters were swapped in on a level that contains them. Marine, Grunt, Jackal, Elite, Hunter and the two Flood combat forms spawn armed and fight. The Infection form, Sentinel and 343 Guilty Spark spawn but have no weapon. Captain Keyes spawns on The Pillar of Autumn.

## What has not been tested or is known to be wrong

- Nobody has played a full match or campaign level by hand on this exact build; the tests above are scripted.
- Not tested: a physical Switch against the Deck, the Steam Deck OLED, other Linux distributions.
- The Deck's Wi-Fi-only local match was measured on the build before the final network update; on the final build the Wi-Fi run was interrupted by a network dropout and not repeated.
- A match does not start if a player joins after the host has triggered the start.
- Campaign characters: a character only works on levels that contain it; an Elite chosen at the very start of "Halo" is stuck inside the lifeboat; cutscenes, vehicles, checkpoints and level transitions as another character are untested. Multiplayer keeps the standard character.
- Performance in large battles and missing or over-bright effects are unchanged from earlier builds.
- The Switch build has not been updated to this network code yet. It still plays with this client on a local network, but will often fail to join public internet games.

## Reporting a problem

Include the map, player count, wired or Wi-Fi, and what you saw. The game's log can contain network addresses and player names: remove anything personal before posting. Do not upload game images, maps, executables, keys or saves.

## Source and credits

`NxHalo-runtime-source.zip` contains the matching game source, the Switch host source, notices and build guidance (`RELEASE-SOURCE.md`). Retail shader tokens, build outputs, game data, logs and keys are excluded. Licences of the bundled libraries and fonts are in the package's `NOTICES` folder. A bit-identical rebuild is not promised.

Independent community project; not endorsed by the original game or console publishers.

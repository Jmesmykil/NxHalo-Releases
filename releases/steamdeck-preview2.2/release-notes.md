# NxHalo Steam Deck Preview 2.2 — responsive lobby and map browser

Pre-release, native 32-bit Linux client, network protocol 11. This supersedes Preview 2.1's action-button alignment for wide displays.

## Included

- Explicit **Co-op Campaign** mode naming with public internet and cross-console lobby listings.
- Waiting lobby panels sized to the available aspect ratio; action button visuals now share the centered positions of the existing menu hit targets.
- A scrollable player roster UI sized to display all 128 lobby entries. The current practical match size is about 40 by creator estimate; 128-player matches are not claimed or validated.
- **GET MAP & JOIN** for a missing Custom Edition map. The client requests the selected map archive, rejects unsafe member paths, extracts only the requested map, checks the map size/CRC, and returns to the same open lobby.
- Existing experimental campaign character selection.

## Build and package

- Built on the Omarchy Linux host using `python3 configure.py --release --pgo=off --game-browser --portable` and `ninja linux linux64`.
- Distributed player binary is an i686 Linux ELF with SDL3 bundled. Its highest imported glibc symbol version is `GLIBC_2.38`; bundled SDL3 tops out at `GLIBC_2.34`. No game data, maps, ROM, keys, or personal saves are included.
- `release-manifest.json` and `SHA256SUMS` identify the binary, archive and matching source snapshot.

## Device and feature acceptance

- Physical Steam Deck LCD, SteamOS 3.8.28: Preview 2.2 launched in the SteamOS session, loaded the user's existing map data, reached the main menu, rendered through at least frame 900, reported `render_health ... cumulative=1 no_program=0 no_target=0 link_failed=0 debug_skipped=0 source_failed=0`, and exited after the smoke window.
- The new binary has **not** been hand-played. No multiplayer lobby, online browser session, 16:10/ultrawide layout sweep, 128-player match, or real missing-map download/join has been completed on this binary. Prior protocol-11 networking evidence belongs to Preview 1 and Profile33, not this changed UI.
- Practical match size remains an estimate around 40; discovery listings of 32/36 players are not sustained simultaneous-match evidence.

Friend/follow management, player profile/moderation actions, voice chat and proximity chat are not included in this preview.

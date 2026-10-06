# NxHalo Steam Deck Preview 2.2 - responsive lobby and map browser

Pre-release, native 32-bit Linux client, network protocol 11. This supersedes Preview 2.1's action-button alignment for wide displays.

## Included

- Explicit Co-op Campaign mode naming with public internet and cross-console lobby listings.
- Waiting lobby panels sized to the available aspect ratio; action button visuals now share the centered positions of the existing menu hit targets.
- A scrollable player roster UI sized to display all 128 lobby entries. The current practical match size is about 40 by creator estimate; 128-player matches are not claimed or validated.
- GET MAP & JOIN for a missing Custom Edition map. The client requests the selected map archive, rejects unsafe member paths, extracts only the requested map, checks the map size/CRC, and returns to the same open lobby.
- Existing experimental campaign character selection.

## Build and package

- Built on the Omarchy Linux host using `python3 configure.py --release --pgo=off --game-browser --portable` and `ninja linux linux64`.
- Distributed player binary is an i686 Linux ELF with SDL3 bundled. Its highest imported glibc symbol version is `GLIBC_2.38`; bundled SDL3 tops out at `GLIBC_2.34`. No game data, maps, ROM, keys, or personal saves are included.
- `release-manifest.json` and `SHA256SUMS` identify the binary, archive and matching source snapshot.

## Device and feature acceptance

- The Preview 2.2 process ran on the physical Steam Deck LCD with SteamOS 3.8.28 and emitted clean `render_health` logs through frame 5400. The available X11 capture showed a black frame, so visible output and reaching the main menu are not verified by this check.
- This is a launch/render-loop smoke only; the binary has not been hand-played. No multiplayer lobby, online browser session, 16:10/ultrawide layout sweep, 128-player match, or real missing-map download/join has been completed on this binary. Prior protocol-11 networking evidence belongs to Preview 1 and Profile33, not this changed UI.
- Practical match size remains an estimate around 40; discovery listings of 32/36 players are not sustained simultaneous-match evidence.

Friend/follow management, player profile/moderation actions, voice chat and proximity chat are not included in this preview.

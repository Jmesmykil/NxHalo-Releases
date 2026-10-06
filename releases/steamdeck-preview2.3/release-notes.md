# NxHalo Steam Deck Preview 2.3

Native 32-bit Linux pre-release, network protocol 11. Built on the Omarchy Linux host from NxHalo-Releases source commit aa54bd08db15d1806babb2ca89f116262f01b2e8.

## New in Preview 2.3

- Waiting-lobby roster selection now highlights the selected player and shows their name, player slot, team, and local/remote console in a details panel.
- Keeps public internet and cross-console lobby browsing, Co-op Campaign labeling, the 128-entry scrollable roster, responsive lobby panels, and opt-in Custom Edition map download-and-join from Preview 2.2.

## Build and device evidence

- Omarchy build completed for linux and linux64. The Deck package is native i686 Linux and bundles SDL3; the binary's maximum GLIBC requirement is 2.38 and bundled SDL3's is 2.34.
- Installed side-by-side at /home/deck/Games/HaloCE-LobbyPreview2.3/NxHalo-SteamDeck. All packaged file SHA checks pass; the existing /home/deck/Games/HaloCE/halo binary remains unchanged.
- A 12-second Steam Deck process smoke reached main-menu music and ran through frame 626 without a crash. The available X11 screenshot path is black, so visible output is not verified.
- No lobby interaction, real multiplayer match, resolution sweep, 128-player match, or map-download-and-join was exercised on this build. The HaloNet map endpoint was separately verified to serve a ZIP containing bloodgulch.map with a valid CRC; this is not an in-game download-and-join test.

Friends/follow, profile moderation actions, voice and proximity chat remain future work. This preview does not claim completion of those features.

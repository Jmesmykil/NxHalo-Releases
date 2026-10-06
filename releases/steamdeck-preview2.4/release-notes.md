# NxHalo Steam Deck Preview 2.4

Native 32-bit Linux pre-release, network protocol 11. Built on the Omarchy Linux build host.

## Lobby and map-browser updates

- The waiting lobby has host-only Kick Console and Ban Console actions for the selected remote player's console. Each action requires a second confirmation on the same selected console. Ban records the console directly, so duplicate player names do not select the wrong target.
- Retains public lobby discovery across supported cross-console and browser clients, Co-op Campaign listings, selected-player details, a scrollable 128-entry roster, and responsive waiting-lobby panels.
- Retains opt-in Custom Edition map download-and-join through the HaloNet map catalog. Downloads are verified before joining. This flow covers catalogued CE maps; it does not download arbitrary files or other mod types.

## Build and device evidence

- `ninja linux` completed on Omarchy Linux from the included source snapshot with the portable i686 Linux configuration and bundled-SDL-compatible GLIBC symbol wrappers.
- Lobby moderation, live multiplayer, resolution coverage, 128-player matches, and in-game map download-and-join have not been exercised on this preview. A roster display capacity of 128 does not establish a playable 128-player match. The creator's approximate current match-size estimate remains about 40.
- No game maps, game executable, shader instruction data, console keys, or saves are included.

Friends/follow, voice setup and proximity chat remain later development work. Cross-platform feature support depends on the corresponding client and network protocol; this preview does not claim those features.

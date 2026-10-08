# NxHalo Steam Deck Runtime Preview 2.9.4 editor stability update

This additive 2.9.4 prerelease updates the native Linux runtime with synchronized playlist/profile initialization and safer file-enumeration handle lifecycle. Existing 2.9.4 releases remain available.

## Downloads

- `NxHalo-SteamDeck-unified-runtime-2.9.4-editfix1.tar.gz` — native runtime and required shared libraries; SHA-256 `2c75e1911e7225774746e3d65df76744e5bfcc040a9f1dff9696a9a72d1b535e`.
- `NxHalo-Unified-source-2.9.4.zip` — filtered matching source snapshot; SHA-256 `9b318a3037cb41a986d78323867f1aff05306d171a03c92d239fbce10b096ead`.

The package excludes Halo game data, maps, resource files, saves, keys, and user profiles. Import your own compatible game data.

## Verification

The exact runtime SHA-256 is `64761e21783c861123469b8827716690843d878b98e674252edfbe24043f080d`. Evidence includes 800,000 ABI checks; native editor open/close; custom profile save and fresh-process reload with Maximum Health 150%; private two-client sessions on stock Bloodgulch and CE 1bloodgulch; missing-map download/install/retry; and remote custom-rule advertisement. The lobby test proves advertised values, not damage behavior. The two-client tests do not establish 128-player capacity. Voice remains unavailable, and Switch hardware is outside this release.

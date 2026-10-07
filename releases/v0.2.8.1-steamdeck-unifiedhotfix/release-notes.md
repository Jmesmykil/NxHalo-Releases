# Unified Steam Deck Preview 2.8.1

This additive prerelease updates the unified native Linux executable and source. The Linux binary SHA-256 is `d7eb374e9f1c3dc010f18445b6c97092722a6d96e18ded9800b8cfea0330e1c6`. Existing Preview 2.8 packages and rollback files remain available.

## Changes

- TLS setup and map downloads now serialize shared crypto access. The production 32-bit updater objects passed forced-failure cleanup checks. A two-thread HTTPS run downloaded both 188,900-byte CA bundles and matched the expected CA hash.
- Missing-map host rotation now queues an exact CE map download and defers the network abort until before attempting to install a map. Retry remains tied to the same invite's latest advertised map. Production guard mocks passed for missing CE, installed CE and Xbox-map cases; a 12-second native missing-CE host probe exited without a relaunch or crash. This does not prove real Deck host rotation after the new fix.
- Menu diagnostics now bound invalid index/widget pairs to 16 entries. The underlying invalid menu index/widget cause remains unresolved; this is diagnostic coverage, not a claimed menu fix.

## Deck evidence and limits

Version 2.8.1 is now atomically installed on the Deck, and the destination SHA-256 matches `d7eb374e9f1c3dc010f18445b6c97092722a6d96e18ded9800b8cfea0330e1c6`. The pre-hotfix game process remains on Preview 2.8 binary `2154e51b9e80077d71fd3102fe4d375fe5ef70a8a0009f866c7bb5bc23488a65` until the next launch, so the new build is installed but is not yet the running game process. The rollback executable is preserved as `halo.before-maprotation281-20261007`. On the prior 2.8 build, changing from a joined CE map to missing Danger Canyon exited to the dashboard. The new native guard checks do not prove that real Deck host rotation is fixed. The new binary passed a 22-second native Omarchy Death Island run (actor loaded, ticks 31–393, health 1) and the missing-map guard checks above.

Prior Deck evidence on the 2154 build includes successful custom-map download/load/join and both controller inputs; a profile 21 in-progress campaign join on b30/a30 at 00:33:46/01:27:16; a profile 11 Blood Gulch 28-player session from 07:58:17 to a network disconnect at 08:00:49; and an Infinity CE retry followed by remote profile 20 loading at 08:37:32 (October 7). These records are evidence for those runs, not proof of every feature on this new binary.

Full 128-player acceptance, live preset matches, Friends and voice remain unverified. The menu root cause also remains open. No game maps, owner resource companions, shader instruction tokens, console keys or personal saves are included. Use your own compatible game data. CE maps may need your own `bitmaps.map`, `sounds.map` and `loc.map` companions.

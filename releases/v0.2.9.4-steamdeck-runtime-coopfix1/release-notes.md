# NxHalo Steam Deck Runtime Preview 2.9.4 co-op lifecycle update

This additive 2.9.4 prerelease fixes authoritative network co-op pause-menu Revert and Restart behavior. Revert restores the host checkpoint and resynchronizes clients. Restart returns everyone to the same-map pregame lobby. The mission-win path selects the next map for the next lobby. The reset preserves the configured co-op player cap and friendly-fire setting.

The update retains existing controller navigation, custom-map download/install/autojoin, native presets, and profile/version support. This release does not update the Switch NRO.

## Downloads

- `NxHalo-SteamDeck-unified-runtime-2.9.4-coopfix1.tar.gz` — executable and required runtime libraries; archive SHA-256 `1b114487e283567a77acd5e3824b7835537b46a9cb42aab425027a9c43c3df2e`. Executable SHA-256 `05fb520d15722a1954dcd884702ebff49dddc59b1aada4b39f6770ac8fdefed0`.
- `NxHalo-Unified-source-2.9.4-coopfix1.zip` — filtered matching source snapshot; SHA-256 `84b4828dce4f7b1ec9f9ee82ba7ee9a64390e93f9d2e0410f3a7f1f49601b941`.

The package excludes game data, maps, resource files, saves, keys, and private invites.

## Verification

The exact executable was installed and run on Steam Deck using the existing Steam shortcut and configuration. Actual two-native-process tests exercised pause-menu Restart/Revert. Checkpoint creation and mission-win-to-next-map transitions were synthetic tests only. A separate natural Start flow loaded B30 on both peers; the client observed the B30 intro as a spectator. This does not claim a natural A50-to-B30 mission win or campaign completion. A private Protocol 11 GPU session produced a 52-sample network trace with both machines alive and advancing; the count is trace samples, not GPU telemetry samples. The friendly-fire-disabled case was not separately verified in a natural runtime test.

No claim is made for 128-player capacity, voice chat, physical Switch runtime, campaign completion, or broad CE engine-extension support.

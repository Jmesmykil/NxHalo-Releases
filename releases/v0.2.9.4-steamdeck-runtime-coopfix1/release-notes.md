# NxHalo Steam Deck Runtime Preview 2.9.4 co-op lifecycle update

This additive 2.9.4 prerelease fixes authoritative network co-op pause-menu Revert and Restart behavior. Revert restores the host checkpoint and resynchronizes clients. Restart returns everyone to the same-map pregame lobby; completing a mission returns the host to the selected next-map lobby. The reset preserves the configured co-op player cap and friendly-fire setting.

The update retains existing controller navigation, custom-map download/install/autojoin, native presets, and existing profile/version support. This release does not update the Switch NRO.

## Downloads

- `NxHalo-SteamDeck-unified-runtime-2.9.4-coopfix1.tar.gz` — executable and required runtime libraries; archive SHA-256 `06783340ed89aa79cab2529e1f5c8b298e031b95363441f425a0a3d3499fb6b0`. Executable SHA-256 `05fb520d15722a1954dcd884702ebff49dddc59b1aada4b39f6770ac8fdefed0`.
- `NxHalo-Unified-source-2.9.4-coopfix1.zip` — filtered matching source snapshot; SHA-256 `84b4828dce4f7b1ec9f9ee82ba7ee9a64390e93f9d2e0410f3a7f1f49601b941`.

The package excludes game data, maps, resource files, saves, keys, and private invites.

## Verification

The exact executable was installed and run on Steam Deck using the existing Steam shortcut and configuration. Natural lifecycle evidence: A50 restart returned to a lobby with 2/16 connected players; transition to B30 returned to the correct next-map lobby with 2/16, and the client observed the B30 intro as a spectator. The campaign was not completed. Physical Deck Protocol 11 peer evidence recorded 52 GPU samples with both machines 0 and 1 alive and advancing positive ticks. Checkpoint and mission-win paths were also exercised in synthetic tests; these are distinct from a natural completed campaign. The friendly-fire-disabled case was not separately verified in a natural runtime test.

No claim is made for 128-player capacity, voice chat, physical Switch runtime, campaign completion, or broad CE engine-extension support.

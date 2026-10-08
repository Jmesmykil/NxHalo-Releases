# Verification evidence

Runtime executable SHA-256: `05fb520d15722a1954dcd884702ebff49dddc59b1aada4b39f6770ac8fdefed0`. The exact candidate was installed and run on Steam Deck; the original shortcut, launcher, and user configuration were retained.

- Actual two-native-process tests exercised the co-op pause-menu Restart and Revert flows. The next-mission-win path and creation of a checkpoint seed (`game_save_totally_unsafe`) were tested in synthetic fixtures only.
- A separate natural Start flow loaded B30 on both peers; the client observed the B30 intro as a spectator. This does not claim natural mission completion or a natural A50-to-B30 win transition.
- Physical Deck Protocol 11 private host/peer evidence includes 52 network-trace samples collected during GPU gameplay; both machines 0 and 1 remained alive and advanced positive ticks. The 52 count is trace samples, not GPU telemetry samples; this is a two-player result, not a capacity test.
- Source review confirms the co-op player cap and friendly-fire value are captured before playlist setup and restored on round setup. The friendly-fire-disabled case was not separately tested in a natural runtime session.
- Existing controller navigation, custom-map download/install/autojoin, and preset UI are retained; their runtime evidence remains tied to the prior 64761 build.

No 128-player, voice-chat, physical Switch runtime, completed-campaign, or general CE engine-extension claim is made. No user game data, maps, saves, keys, or invites are included.

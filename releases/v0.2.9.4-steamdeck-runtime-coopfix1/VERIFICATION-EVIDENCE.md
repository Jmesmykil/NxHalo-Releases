# Verification evidence

Runtime executable SHA-256: `05fb520d15722a1954dcd884702ebff49dddc59b1aada4b39f6770ac8fdefed0`. The exact candidate was installed and run on Steam Deck; the original shortcut, launcher, and user configuration were retained.

- Natural campaign lifecycle: restarting A50 returned to the correct connected lobby with 2/16 players. Advancing to B30 returned to the correct next-map lobby with 2/16; the client observed the B30 intro as a spectator. This does not claim B30 completion.
- Physical Deck Protocol 11 private host/peer evidence recorded 52 GPU samples with both machines 0 and 1 alive and positive ticks. It is a two-player result, not a capacity test.
- Checkpoint and mission-win branches were exercised in synthetic fixtures; they are not natural checkpoint or completed-mission acceptance.
- Source review confirms the co-op player cap and friendly-fire value are captured before playlist setup and restored on round setup. The friendly-fire-disabled case was not separately tested in a natural runtime session.
- Existing controller navigation, custom-map download/install/autojoin, and preset UI are retained; their runtime evidence remains tied to the prior 64761 build.

The release makes no 128-player, voice-chat, physical Switch runtime, completed-campaign, or general CE engine-extension claim. No user game data, maps, saves, keys, or invites are included.

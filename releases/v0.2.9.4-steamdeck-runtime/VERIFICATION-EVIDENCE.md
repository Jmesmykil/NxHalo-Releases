# Verification evidence — Unified Runtime Preview 2.9.4

This document separates test artifacts from claims about the final release build. `release-manifest.json` pins candidate 8335 and records the tested executable hash for each retained run. Tests on earlier binaries remain labeled by their own hashes.

## 128-slot roster UI

- Tested executable: `5ecb2c2bca91792e9b901e8a4f427b5b94d75633e641e4a6cd60aeea51942a11`.
- Setup: one native host plus 127 synthetic loopback peers.
- Result: host telemetry reported 128 machines; UI displayed 128/128 players. The selected final page showed rows 118–128, player slot 128 / console 128, remote.
- Screenshot digest: `0972a88c8981921f7f954f0fd1c902a153fc8e66f6d6471bfb0318cabf2e5cc1`.
- Limits: synthetic protocol peers are not human clients. This does not prove 128 real players, 128 separate systems, sustained traffic or a match. These visual/roster runs were not repeated on candidate 8335.

## Visible texture override

- Tested executable: `4dd54a46c8554f25386d2ee7619dc904d1a9df60315dcca85071cfdec68b7d3f`.
- Setup: native DeathIsland scene, software Mesa llvmpipe, same-frame baseline and enabled runs.
- Identity: traced `scenery\halo\bitmaps\halo inner ring`, bitmap 0, 2048×128. The enabled log confirms a synthetic TGA override loaded for that identity.
- Result: the synthetic checker is visibly mapped onto the 3D ring in the enabled capture; baseline shows the original texture. The 180×900 ring crop had 8,754 of 162,000 pixels differing by more than 24 in one RGB channel.
- Comparison digest: `083073eb47768c8530ae5d9961f441e23319ae011ed12022b4e0e49f1c34d8b6`.
- Limits: this is software-rendered synthetic texture acceptance, not Deck GPU/performance acceptance or testing every texture pack. These visual/roster runs were not repeated on candidate 8335.

## Campaign profile route, runtime telemetry and Zombie outcomes

The native online campaign menu route/profile override is verified. A natural server-list selection loaded an A30 room. On candidate 8335, natural campaign browser selection joined a private V21 lobby with two players and loaded `a50`. Client W input from 03:37:01.600 to 03:37:04.601 UTC moved the client from (31.630, -32.980, 87.205) to (27.607, -29.640, 88.111) with throttle 1. The host reported the same final position; ticks continued near 31 per second and traffic increased without another silent-connection warning. Profile 11 remained unchanged. The temporary fixture override was cleared during shutdown. This proves bounded movement/state agreement in the loaded mission, not full campaign completion. A private native two-client Protocol 11 Zombies melee-ending run on predecessor 0f6 passed. Host and peer ended at ticks 255 and 264, respectively, and recorded identical results: slot 1 team 0 loss (0 kills/1 death), slot 2 team 1 win (1 kill/0 deaths). The test ELF was predecessor candidate `0f6e0df06a59c5ac189ce75f2c4f68a31f621d735de36049400d500840ae9d1b`, not candidate 8335. Telemetry reported ELF identity, frame/present timing percentiles, RSS, counters, and engine postgame outcomes. This was a private software-rendered llvmpipe run, not Deck FPS proof.

Telemetry is opt-in (`[debug] performance = true` or `HALO_PERFORMANCE=true`), defaults off, and has a 2,048-interval rolling window. GPU utilization, when available, is device-wide and not a game-GPU timestamp.

The roster harness used synthetic peers and does not demonstrate a 128-human match or sustained 128-client traffic. Broad legacy-client interoperability and campaign completion remain unverified. The separate Deck gameplay measurement is described in its own section.


## Catalog search and map import (bounded)

- Earlier single-map test executable: `a647dfba4e87cca696b707acf2612117502a672f4713b0380ad671d615c1abac`; the later two-map test executable is `0f6e0df06a59c5ac189ce75f2c4f68a31f621d735de36049400d500840ae9d1b`.
- Setup: private Xvfb 1280×720; XTest delivered real keyboard events to search `3Tiers`, then selected the filtered first result.
- Result: the exact-row archive lookup downloaded and installed a validated 22,590,613-byte CE cache (`3Tiers.map`, cache header `daeh`). Installed file SHA-256: `71d53da30f2c4719bb912c07e38c39015b409be9bf2a0cf672bc75828a294dad`.
- Limits: this is a private Linux/XTest keyboard path, not physical Deck controller/search acceptance or universal catalog/archive coverage. Both tested downloads were retested on predecessor 0f6.


## Exact catalog archive and locator fallback

- Tested executable: `0f6e0df06a59c5ac189ce75f2c4f68a31f621d735de36049400d500840ae9d1b`.
- Setup: private native XTest keyboard events searched and selected a filtered catalog row for each map.
- `3Tiers`: installed validated 22,590,613-byte CE cache; header `daeh`; SHA-256 `71d53da30f2c4719bb912c07e38c39015b409be9bf2a0cf672bc75828a294dad`.
- `1bloodgulch`: installed validated 16,610,796-byte CE cache; SHA-256 `a676c21c4ad75cdf5dc88ed4f76335ec018948efa84d92107ea5370e86635c9d`.
- This covers two exact catalog selection/download paths on one native test build, not every map, physical Deck controller input or full campaign compatibility. Both exact catalog rows passed on predecessor `0f6e0df06a59c5ac189ce75f2c4f68a31f621d735de36049400d500840ae9d1b`; they were not rerun on candidate 8335.

## Steam Deck install readback

Predecessor 0f6 was atomically installed and hash-read back; PID 599413 ran it from the original Steam shortcut on Jupiter (AMD vangogh), build ID `516b352de9781be01ad32f2b5bad11c47cdc8001`. It reached the native AMD OpenGL menu at 60 fps with wall p95 about 16.81 ms, p99 about 16.865 ms and RSS 207468 KiB.

Candidate 8335 was then atomically installed and exact-hash read back. PID 599911 ran it from the original Steam shortcut, build ID `ecebfaa1f3985674f649d2e73dab077f505dab20`; AMD vangogh OpenGL 4.6 / Mesa 26.1.2 initialized and the post-load menu ran at 60 fps (wall p95 16.815–16.818 ms, p99 16.871–16.873 ms, RSS 201504 KiB). Gameplay capture ran as PID 600095; after configuration restoration, the original Steam shortcut was relaunched and PID 600229 was verified running the same candidate hash. Configuration matched the pre-install backup and the launcher SHA remained `9ea947ee4e95d1f3084497366716526321f8edf88b724da0adb145e6faaa8f57` after restoration.

## Read-only campaign trace candidate

Candidate 8335 adds `HALO_NETWORK_TEST_TRACE=1`, a developer-only read-only diagnostic that logs wall-clock seconds, game ticks and player state. It defaults off and bypasses setup, movement, shooting and game-rule branches. In the private V21 `a50` session, a three-second client W input produced a matching host/client position update. This verifies movement/state agreement in the loaded mission, not campaign completion.


## Candidate 8335 Deck gameplay capture

A two-player Protocol 11 Bloodgulch LAN session ran for 65 seconds in Quality mode on the Deck. Four post-load intervals measured 59.70 fps, wall p95 16.825–16.864 ms, p99 16.909–16.949 ms; latest CPU was 26.22%, RSS 210136 KiB and peak RSS 238484 KiB. The log reports 852×480 scene and 1280×720 draw dimensions. The 1920×1080 screenshot is the composed capture size, not the logical render target. Host and peer exited 0; configuration and launcher were restored. No final winner was recorded in the timed session. This is one map/mode/player-count measurement, not a universal performance claim.

# Unified Steam Deck Runtime Preview 2.9.4

This additive preview updates native online campaign setup, multiplayer roster/runtime telemetry, texture-pack acceptance and Zombie match-result handling. Candidate executable SHA-256: `8335b4f246b592854a91916bb36141c884883eb197eff1f5e59e4fe78aae64fc`. It adds opt-in read-only network trace output; the trace is disabled by default. The exact 8335 build is installed and running on Deck. A private native V21 campaign movement check is described below; 6582 predecessor gameplay performance is identified separately. Earlier releases, including 2.9.3, remain available.

## Online campaign entry

The native menu route and runtime profile override are verified. A natural server-list selection loaded an A30 room. In a private V21 session, the native campaign browser joined a two-player lobby and loaded `a50`. After the client held W for three seconds, its position changed from (31.630, -32.980, 87.205) to (27.607, -29.640, 88.111) with throttle 1; the host reported the same position, ticks continued near 31 per second, and traffic increased without a silent-connection warning. Profile 11 stayed saved as configured. The temporary session profile override was cleared during fixture shutdown. This verifies movement and matching state in the loaded mission; it does not establish campaign completion. The menu applies a compatible network profile when joining or hosting a campaign room, without restarting into a separate executable. The current co-op trace build logs per-second ticks and player state only when `HALO_NETWORK_TEST_TRACE=1`; this does not set up, move, shoot, or alter game rules. The trace is developer diagnostics only, off by default, and does not drive gameplay.

## Lobby roster and selection

A native lobby roster acceptance run reached 128 visible slots and selected the final slot: rows 118–128 of 128 and details for player slot 128 were captured. This run used one native host and 127 synthetic loopback protocol peers. It verifies roster display, paging/selection and selected-player detail for a simulated 128-slot lobby; it does not demonstrate 128 real players, 128 real devices, sustained 128-client traffic or a live 128-player match. The retained roster proof used build `5ecb2c2bca91792e9b901e8a4f427b5b94d75633e641e4a6cd60aeea51942a11` and was not repeated on candidate 8335.

## Visible texture override

A native software-rendered comparison showed a synthetic magenta/cyan TGA override visibly mapped on the traced DeathIsland `scenery\halo\bitmaps\halo inner ring`, bitmap 0. The baseline and enabled captures show the stock texture and override on the same 3D ring. This proves a visible override on the Omarchy llvmpipe test path; it does not prove Steam Deck GPU performance or acceptance of authored texture-pack art. The retained visual proof used a separate pinned build and was not repeated on candidate 8335.

## Match rules and results

Preset application continues to use the cached Server Setup values, clients cannot replace host-selected modes, and the lobby/listing reports the host's replicated rule values. Zombie elimination now changes the infected team at pre-spawn so peer state and engine results agree. A private native two-client Protocol 11 melee-ending run passed. The host ended at tick 255 and peer at tick 264; both reported identical results: slot 1 team 0 loss (0 kills/1 death), slot 2 team 1 win (1 kill/0 deaths). This test covered the stated Zombies melee-ending case; it does not establish every gametype or broad legacy-client compatibility. Infection behavior remains map-aware and preserves inventory when a loadout cannot be granted.

## Native performance and match telemetry

The launcher retains each previous `halo-runtime.log` under `log-archive/halo-runtime-<UTC timestamp>-<PID>.log` before rotating the current session log. Opt-in telemetry reports runtime ELF build ID; ten-second frame/present p50/p95/p99 wall-time percentiles over a bounded 2,048-sample window; process CPU, RSS, page-fault and context-switch counters; optional device-wide GPU-busy counters; existing replication/damage counters; match-start/player snapshots; and actual engine win/loss/tie at postgame. It defaults off and does not change maps, saves, network protocol or match rules. A native private two-client run verified identity, timings, RSS and matching final outcomes. The two-client telemetry run used Mesa llvmpipe. The Deck gameplay captures include the candidate 8335 two-player session below and a separate predecessor 6582 session.

## Verification boundaries

No owner maps/resource companions, shader instruction tokens, console keys or personal saves are included. Real 128-user matches, final-binary 128-roster retest, controlled end-to-end online campaign completion, Friends/voice and broad legacy-client interoperability remain open. The Switch preview is published separately.

## Deck and Switch status

Candidate 8335 was atomically installed and read back. A two-player Protocol 11 Bloodgulch LAN session (PID 600095) ran for 65 seconds in Quality mode: 59.70 fps across four post-load intervals, wall p95 16.825–16.864 ms, p99 16.909–16.949 ms, latest CPU 26.22%, RSS 210136 KiB and peak RSS 238484 KiB. The log records 852×480 scene and 1280×720 draw dimensions; the captured screenshot is 1920×1080 and is not the logical render target. Host and peer exited 0 and restored configuration and launcher; the original Steam shortcut was then relaunched as PID 600229 with the same candidate hash. The timed session did not produce a final winner result. The separate 6582 predecessor capture remains a different binary and measurement. Switch has a separate 2.9.4 candidate staged to SD with hash readback and safe eject; no physical launch or HOME-title acceptance is claimed.

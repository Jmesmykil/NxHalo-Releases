# Native performance and match telemetry

Enable `performance = true` in the existing `[debug]` section of `config.toml`, or launch with `HALO_PERFORMANCE=true`. It defaults to off. Records go to the normal runtime stderr/log. No maps, saves, network protocol or match rules are changed by enabling it.

- `runtime_identity`: process ID, Unix timestamp and the running ELF build ID for matching future crash addresses to the exact executable. Retain that executable and its debug information.
- `perf_frame`: once per ten seconds, frame count and average FPS plus p50/p95/p99 wall-frame intervals and Present duration. The interval includes CPU work, scheduling and frame pacing; Present includes resolve/composition/swap. These are wall timings, not GPU execution timestamps. The rolling sample window is bounded to 2,048 intervals.
- `perf_resources`: process CPU percentage (can exceed 100 with multiple active threads), current/peak resident memory, page faults and context switches. If an AMD device exposes its busy counter, device-wide GPU utilization is recorded; `gpu_available=0` and value -1 explicitly mean unavailable. Device utilization is not the game's GPU frame cost.
- `perf_network`: cumulative existing replication/damage counters: sent/received/corrections and hit reports/dealt/rejected/replayed. These counters are not packet byte totals.
- `match_begin`: loaded scenario, actual variant and engine/team settings.
- `match_player`: per-player score, kills, deaths, assists, betrayals, suicides, shot statistics and team. Snapshots are recorded at match start, every five seconds and at engine postgame. Final win/loss/tie uses the engine's own `game_engine_did_player_win` result, not a guessed score comparison.
- `match_end`: actual engine postgame, or an explicitly unconfirmed leave/unload event. A quit/disconnect is not marked as a completed win.

Create a JSON report from actual logs:

```sh
python3 tools/runtime_telemetry_report.py halo-runtime.log --output match-report.json
```

Acceptance runs can require `--require-perf --require-rss --require-identity --require-final`. Missing evidence fails those checks rather than being converted to a zero or a passed result. A match that has not ended should omit `--require-final`.

Private Linux/software-rendered tests demonstrate the records and gameplay behavior. They do not establish Steam Deck frame rates. Capture Deck play separately with its own runtime identity, renderer startup and hardware metrics. Older uninstrumented matches cannot be reconstructed from these new records.

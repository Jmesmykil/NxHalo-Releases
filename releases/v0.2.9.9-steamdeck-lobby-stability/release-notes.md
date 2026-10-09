# Steam Deck/Linux 2.9.9 — lobby stability

[Download runtime and matching source](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9.9-steamdeck-lobby-stability).
Executable SHA-256: 13661136b9dc51b454d6d665df0c4306415bcd3c01f2c862180166591fc4229b. This exact build is installed through the existing Deck shortcut, with the previous 2.9.8 and stable 2.9.6 rollbacks retained.

## Changes

- A multiplayer round returns to its current map, gametype and custom rules instead of stale playlist defaults. Co-op keeps its separate mission progression.
- Gun Game weapon replacement retains the prior weapon on failure, selects the new stage on success and removes spare weapons.
- The name editor rebinds tag-backed keyboard data and clears it before cache teardown.
- Online Play includes a separate Native V24 view, preserves verified newer-version entries, displays directory/cache state and keeps endpoint selection stable across refresh.
- V24 is an explicit compatibility profile; V21 remains the host default and existing older native profiles remain selectable. V24 handles the official flat custom-map namespace and authored CE609 vehicle selection.
- Installed Maps has searchable, paged family/type/details navigation with capacity4096; invalid/resource files are distinguished. Native folder import/cancel preserves selected and enabled packs.

## Verified evidence

The acceptance receipt identifies exact checks and limits for the real Gun Game restart, keyboard cache transition and independent official v24 client admission. Private Deck AMD GPU gameplay against the unchanged2.9.6 native executable passed; real remote input advanced the weapon ladder. Existing saves, maps, controller settings, launcher and normal audio volume were preserved. Tests use silent audio and do not capture a hardware microphone.

The independent v24 runtime check covers stock Beavercreek and two players; CE609 authored-vehicle behavior and reverse-direction admission require separate checks.

The owner's completed online Gun Game win is retained separately from automated evidence. No synthetic128-row test is described as real128-player gameplay.

## Compatibility and remaining work

Supply your own compatible maps and resource files. Shared-map sword, faction models and authored tracks/towers require the same compatible map on participants. Voice requires compatible native receive/playback support. A classic PC/CE or browser listing does not prove executable/protocol compatibility; private or unannounced games cannot be guaranteed by a public directory.

Complete race-route packs, specialized bots, Prop Hunt, persistent friends, full campaign lifecycle, human microphone use, real capacity and physical Switch acceptance remain separate backlog items. This Linux release does not replace the separate Switch executable.

[Guide](steam-unified/docs/LOBBY-STABILITY-2.9.9.md) ·
[Acceptance](releases/v0.2.9.9-steamdeck-lobby-stability/ACCEPTANCE.json) ·
[Deck hardware evidence](releases/v0.2.9.9-steamdeck-lobby-stability/DECK-GPU.json).

# Steam Deck/Linux 2.9.8 — native gametypes

[Download runtime and matching source](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9.8-steamdeck-native-gametypes).
Executable SHA-256: 30cc23d435bbf4aa4b0103ee71f48838bb9350fb06b9284a360ae054b9735eed. Installed on the Steam Deck through the existing shortcut; 2.9.7 and the stable 2.9.6 rollback are retained.

## Normal gametype selection

The normal STANDARD picker now contains all nine implemented special modes:
**Gun Game, SWAT, Tower of Power, Grenade Dodgeball, Zombies, Native Race,
Covenant-USMC, USMC-Flood, and Flood-Covenant.** Existing standard gametypes remain.

Use **Multiplayer → Edit Gametypes** to edit or rename a template and save a custom
copy, then choose it from **CUSTOM** when hosting. Saved copies retain their special
mode and faction pairing across rename and reload. Built-in templates stay intact.

Entering options for the same base engine preserves the current special mode,
including in Server Setup. Selecting a different engine, an ordinary standard
profile, or MODES → Standard clears it. Gun Game retains seven kills, free-for-all,
and kill-in-order off; other compatible options, including time limits, are editable.
The match advertises the recognizable mode name; your saved custom profile keeps its name.

## Verified

- All nine templates independently enumerated; 32-bit and 64-bit ID/metadata tests passed.
- Actual native menu: Gun Game selected, renamed to testladder, saved, and loaded in
  a fresh process. The saved ten-minute limit survived hosting. An in-lobby edit to
  fifteen minutes survived reopening with Gun Game identity and fixed rules intact.
- Exact installed runtime passed private Steam Deck GPU gameplay against the unchanged
  2.9.6 native client. Real remote mouse input advanced pistol to assault rifle.
- Existing saves, maps, launcher, controller configuration and normal audio were preserved.
  Automated tests used silent audio and did not capture a hardware microphone.

These tests do not close every mode-specific gameplay case. Prior full Gun Game
match evidence and the owner's completed match remain separate earlier evidence.

## Compatibility and remaining work

No game maps, resource companions, keys or personal saves are included. Faction
models and new sword assets still require matching shared maps. Listing a classic
PC/CE or browser server does not establish protocol compatibility. Broader race-map
packs, bots, Prop Hunt, persistent friends, full campaign lifecycle, real-player
capacity and physical Switch acceptance remain separate work. Voice needs compatible
clients. This release updates Linux; the separate Switch executable is unchanged.

[Gametype guide](steam-unified/docs/NATIVE-GAMETYPES-2.9.8.md) ·
[Deck evidence](releases/v0.2.9.8-steamdeck-native-gametypes/DECK-GPU.json) ·
[Gametype evidence](releases/v0.2.9.8-steamdeck-native-gametypes/NATIVE-GAMETYPES.json).

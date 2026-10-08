# Steam Deck/Linux 2.9.7 — host rules and Gun Game

[Download runtime and matching source](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9.7-steamdeck-host-rules).
Executable SHA-256: 090807f68d38770fbd78d40a1f9ebe4e6e230f6f7a37600d0059722bc77988a7. Installed on the Steam Deck through the existing shortcut, with 2.9.6 retained for rollback.

- **Gun Game** in the host's Combat modes: pistol → assault rifle → plasma rifle → needler → shotgun → sniper → rocket. Seven credited enemy kills win. Melee and delayed-projectile kills also count.
- Host inventory and score updates work with the tested **unchanged 2.9.6 native client** on original Blood Gulch. No new weapon assets or Gun Game client code are needed.
- Seven-kill/FFA/no-kill-in-order rules are fixed; map-placed weapons, vehicles and grenades are disabled. Other compatible settings remain editable.
- Required weapon animations are checked before spawning a selected character. The host falls back to a compatible character already loaded in the map when necessary; the saved character preference stays unchanged.
- Changing the base game in the rules editor or choosing Standard clears the special mode. Confirming the selected gametype preserves the chosen Combat mode.
- Retains native map/texture downloads, installed-pack management, in-lobby map/type/rule editing, controller routing, Ghost boost and optional Lobby/Menu Voice from previous releases.

## Verified

The Deck owner also confirmed completing a full Gun Game match. That human run was not independently tied to a runtime hash.

Exact 090807 runtime passed private Steam Deck GPU gameplay and real remote mouse input advanced pistol to assault rifle against the unchanged native client. A separate Linux pair completed all seven actual-fire kills; both peers recorded the same native score-7 win, including a final simultaneous rocket suicide. The fixture used native position gathering and synthetic aim, not scripted damage or score changes. The final repeat completed without a runner error; both native endgame logs and clean process exits are retained.

On the preceding c8c480 build, an isolated Zombies test with a saved Elite choice and the unchanged client retained matching shotgun/sword inventories on an owner-derived private map. The final build changes only the gametype-confirmation callback; the tested loadout code is unchanged. This proves replicated inventories after the compatibility fix, not measured biped identity or a stock-map sword. That map is not included.

At 640×480, the natural MODES → Combat → Gun Game path retained its selection after OK, and Server Setup displayed Gun Game (SLAYER). Explicitly changing the base game to Slayer cleared the special mode and restored ordinary editable settings. Gun Game fixed-field disabling and the separate Standard button were source-verified; their rendered paths were not separately accepted. Detailed receipts retain remaining test limits.

## Remaining limits

Compatibility above refers to the tested native network version, not arbitrary classic PC/CE or browser builds. New models/weapons still require compatible map assets; arbitrary scenery Prop Hunt and the plasma-pistol disguise selector are unfinished. A completed large race-map pack, persistent friends, broad CE engine-mod ports, full campaign lifecycle acceptance, real-player capacity and physical Switch acceptance remain open. Voice requires compatible clients; automated checks did not use hardware microphones.

[Host rules guide](steam-unified/docs/HOST-RULES-2.9.7.md) · [Texture pack guide](steam-unified/docs/NATIVE-MODS-2.9.6.md) · [Deck evidence](releases/v0.2.9.7-steamdeck-host-rules/DECK-GPU.json) · [Gun Game evidence](releases/v0.2.9.7-steamdeck-host-rules/GUN-GAME.json).

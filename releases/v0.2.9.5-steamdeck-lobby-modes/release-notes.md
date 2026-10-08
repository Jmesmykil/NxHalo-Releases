# Steam Deck/Linux 2.9.5 — lobby editing, voice options and content

[Download the runtime and matching source](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9.5-steamdeck-lobby-modes).
Executable SHA-256: `b3d735a6f78fa495464c6589043245fcf8330580931b949e4f099299e3e1c1db`. Installed on the Steam Deck through its existing shortcut, with the previous runtime retained for rollback.

- Edit the host map, game type and rules from the waiting multiplayer lobby; apply them without removing connected players.
- Audio Settings: Voice Chat, Push to Talk/Open Mic, and optional Lobby/Menu Voice. Voice and menu/lobby voice default OFF. Compatible native clients are required.
- Search, category, installed-state, alphabetical sort and pages for maps; searchable texture-pack library, selected-pack status and recoverable removal.
- Strict shotgun survivor/Tower inventories; Ghost L/LB boost; Race vehicle allocation uses available authored spawn flags.

## Verification

Exact executable passed private Deck GPU multiplayer with a native Linux peer. Native tests verified retained-peer map/Oddball/score edits, visible lobby controls, content filters/library actions and saved voice options. Silent generated-PCM tests verified connected-lobby voice and voice through pause menus, with no capture in an unjoined main menu. Component tests are pinned to their hashes in the records; no physical microphone, lossless transport or real-player capacity claim is made.

Lunge runtime acceptance, playable sword assets, turret retention, large races, finished campaign faction maps, a complete online mod store, persistent friends and full natural campaign completion remain open. Physical Switch acceptance remains separate. This release does not replace the Switch executable.

[Feature guide](steam-unified/docs/LOBBIES-MODES-CONTENT-2.9.5.md) · [Deck check](releases/v0.2.9.5-steamdeck-lobby-modes/DECK-GPU.json) · [Lobby voice evidence](releases/v0.2.9.5-steamdeck-lobby-modes/LOBBY-VOICE.json).

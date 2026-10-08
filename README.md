# NxHalo — Halo for Nintendo Switch and Steam Deck

## Current Steam Deck/Linux 2.9.5 — lobby editing, voice options and content

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
## Nintendo Switch

The newest Switch package is [Co-op Lifecycle Preview 2.9.4](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9.4-switch-coopfix1-preview): standalone Homebrew Menu NRO, matching source and build evidence. NRO SHA-256: `e98eea315b6733ff1b3d2f6d3b19d4539332540b32157181ec5d8ac328e2c2d0`. Cross-build and UMS staging/readback passed; a physical launch has not been confirmed.

Use your own compatible data under `sdmc:/switch/halo/maps/`. The preview saves under `sdmc:/switch/halo/save-community24/`. It is an NRO, not an NSP or HOME-menu forwarder. Earlier Profile33 and Build14 packages remain available in Releases. The Linux features above should not be assumed present in this Switch package.

## Install and use

Extract the Steam Deck/Linux runtime, provide your own compatible game maps in `assets/maps`, and run `launch_halo.sh` natively with Proton off. Keep your settings and saves when updating; saves use `~/.local/share/halo-linux`. No game maps, resource companions, shader instruction tokens, console keys or personal saves are supplied.

Online campaign supports remote players without requiring a second local controller. Native multiplayer and campaign discovery stay separately filtered by source and version. Classic PC/CE server snapshots remain separate and require a compatible classic client.

CE609 maps and compatible texture packs are supported. Maps using external resource companions need owner-supplied `bitmaps.map`, `sounds.map` and `loc.map` under `assets/maps/ce`. Chimera/OpenSauce/Lua/DLL and other engine extensions require individual native ports.

## Source and history

The matching Linux source is in [steam-unified/](steam-unified/) and the source ZIP attached to the release. Original upstream attribution and licenses are retained. Earlier releases remain available for rollback and historical evidence.

## Credits

The Halo decompilation, OpenCE, native ports and bundled libraries retain their authors and licenses. This unofficial fan project is not endorsed by Microsoft, Bungie or Halo Studios.

# Steam Deck/Linux 2.9.6 — online texture packs and CE map loading

[Download runtime and matching source](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9.6-steamdeck-native-mods).
Executable SHA-256: e3cdc7999eb273329daca96b90201b8263f20b0963902f1d036170670f474bec. Installed on the Steam Deck through the existing shortcut with the previous build retained for rollback.

- **Online Texture Packs**: search, pages, author/license/map requirements, explicit verified download and install.
- **Installed Packs**: select, enable/disable, cancel removal or move a disabled pack to recovery. Installation preserves the current selection and enabled state.
- Atomic imports reject corrupt, unsupported or unsafe archives; attribution stays with each installed pack.
- The first catalogue entry is **one Alpine-specific cliff texture** by csauve, licensed CC BY-NC 4.0. It requires its matching map tag and does not retexture stock maps.
- CE BSP bounds now use the exact size read by the CE loader. Xbox sector checks remain unchanged.
- Retains 2.9.5 in-lobby map/type/rule editing, strict mode inventories, Ghost boost and optional Lobby/Menu Voice alongside PTT/Open Mic.

## Verified

Exact runtime passed private Steam Deck GPU multiplayer; its native Linux peer exited cleanly. Real native menus fetched the public catalogue, installed the exact PNG and credits, preserved Off, selected/enabled/disabled the pack, cancelled removal and recovered it. Final details fit at 640x480. Focused mixed-ABI catalogue/ZIP/receipt checks passed 274; texture import/cache tests and independent review passed.

The CE fix let two private native clients enter an owner-derived B30 multiplayer map with advancing simulation. It does not establish faction-model acceptance or full campaign completion; that map is not included.

Playable sword/lunge acceptance, large authored races, persistent friends, broad CE engine-mod ports, real-player capacity and physical Switch acceptance remain open. Voice needs compatible native clients; hardware microphones were not used in automated tests. No game assets or personal data are packaged.

[Guide](steam-unified/docs/NATIVE-MODS-2.9.6.md) · [Catalogue](catalog/README.md) · [Deck evidence](releases/v0.2.9.6-steamdeck-native-mods/DECK-GPU.json) · [Native menu evidence](releases/v0.2.9.6-steamdeck-native-mods/NATIVE-MODSHOP.json).

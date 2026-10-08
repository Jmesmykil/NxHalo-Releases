# Steam Deck/Linux 2.9.6 — online texture catalogue

This additive Linux update keeps the 2.9.5 lobby editing, controller fixes and
optional menu/lobby voice, and adds:

- **Online Texture Packs** under Custom Maps & Mods → Mods & Display.
  Search, nine-item pages, explicit pack details, bounded verified download and
  installation, plus the existing installed-pack manager.
- The first entry is one Alpine-specific authored texture, credited to csauve
  under CC BY-NC 4.0. It only affects maps with its matching bitmap tag.
- Atomic image-pack imports, recoverable removal, refreshed GPU textures after
  reimport, and preserved author/license receipts.
- A CE map-loader fix: CE BSP sizes are checked exactly as read; Xbox maps
  retain their sector-rounded bounds checks.

[Content guide](NATIVE-MOD-CATALOG.md) ·
[Previous lobby and voice guide](LOBBIES-MODES-CONTENT-2.9.5.md)

## Evidence and limits

Focused native catalogue/archive/receipt checks passed, including corrupted
archives, traversal, symlinks, size limits, changed catalogue selections and
missing attribution. Mixed-ABI texture tests cover rollback, no replacement of
existing packs, image fallback and refresh after reimport. Independent review
resolved the attribution-reuse and stale-selection findings.

Actual private native UI testing fetched the public catalogue, showed pack
details, installed the exact PNG and attribution files, preserved the disabled
setting, selected/enabled/disabled the pack, rejected removal while active and
moved the disabled pack to recovery. Rendered details were checked separately.

The CE bounds fix let two private native clients load an owner-derived B30 CE609
multiplayer map and advance simulation. That map is not included. This is not
acceptance of its faction models, full campaign, completed sword/lunge behavior,
large races or real-player capacity.

The release includes an explicitly opt-in synthetic melee input mode used only
by private tests. The normal empty debug input setting leaves controls unchanged.
All automated audio checks used dummy/file output; hardware microphone acceptance
remains separate. This Linux release does not replace the Switch binary.

No general DLL/Lua mod compatibility or arbitrary voice/assets on unchanged
clients is claimed. A map can carry compatible weapon/model dependencies;
host rules can assign those loaded objects through existing replication.

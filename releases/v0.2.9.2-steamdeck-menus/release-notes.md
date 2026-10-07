# Unified Steam Deck Content Preview 2.9.2

This additive release improves the native menu organization and adds custom game-type editing and Zombies availability handling. Linux executable SHA-256: `03a94e8d49c3533807c478ba2a3390080db8f74b8b1a207b31510efa2882c84b`. Previews 2.9 and 2.9.1 remain available.

## Menu paths

- Multiplayer actions are grouped as Join, Create, Local Split, and Edit Game Types.
- Create Game groups modes into Combat, Factions, Race, and Standard.
- The saved editor supports Create/Edit for game types, weapons, players, vehicles, and scoring.
- Custom Maps & Mods > Custom Maps > Browse opens the map catalog; texture packs are listed in their own display.
- Create Game > Network Options contains network setup.
- Characters remains a separate menu.

## Zombies support

In a map with the required loaded sword tag and usable assets, the native Zombies path selects the sword for infected players. If the map lacks that content or spawn/inventory setup fails, the code preserves inventory and refuses the infection transition. This does not add sword assets to stock maps or guarantee Zombies support on maps without the required content.

## Verification

Production build completed for the exact binary hash above. An isolated UI test created, named, and saved a 512-byte custom game type. A second X11 input run opened player options, changed health, and saved `customblam.lst` (SHA-256 `bb8901cb6f5c214c600f310d22c995a3c552b9d5a7c7310a72c7c3dca15be053`), exit 0. The production i686 sword harness passed missing-tag, bad-asset, spawn-failure, and inventory-failure preservation/grant selection cases. These are native tests; they do not claim physical Switch validation or broad map coverage.

The binary is atomically installed on the Deck and its destination hash matches. The existing public match remains on the previous process until the next relaunch; installation preserved that session and did not force-stop it. No game maps, owner resource companions, shader instruction tokens, console keys, or saves are included.

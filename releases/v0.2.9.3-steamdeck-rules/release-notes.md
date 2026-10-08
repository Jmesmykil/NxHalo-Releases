# Unified Steam Deck Rules Preview 2.9.3

This additive release improves match-rule setup and public game reporting. Linux executable SHA-256: `e439a36aa08183cf4b27d4e869a5f6b0900c0f8286e9efc9a9aba7d9584f428c`. Previous 2.9, 2.9.1 and 2.9.2 releases remain available.

## Match rules and lobby reporting

- Preset selection applies cached Server Setup values instead of leaving the controls stale.
- Joining clients cannot select or overwrite the host’s match mode.
- Native harness coverage checks replicated shields, health, score, weapon category, time limit and friendly fire values, plus custom mode name and NS/HP listing fields. This is not a populated lobby screenshot or end-to-end old-client proof.

## Zombies and stock objective presets

The tested stock sequence covers Oddball, Flag and pistol. Infection melee-only input and damage are enforced host-side. If the infected loadout cannot be granted, inventory is preserved. This does not prove compatibility with every map, all legacy clients, or every custom gametype. Old-client skull/objective compatibility has not been tested.

## Verification

The 32-bit native production build completed for the exact hash above. Native harnesses passed for preset application, client host-mode guard, replicated rule/listing fields and host-side infection input/damage/loadout behavior. The final menu UI regression passed the cached preset path: TeamSWAT selection changed the private configuration from 0 to 2, displayed “Selected SWAT” and “Applied current Server Setup,” and passed the keyboard/XTest path. The final apply message was shortened to fit; that exact text-only shortening did not receive a separate UI capture. Real controller input and a populated lobby display were not verified.

Root is installing this binary; device install/current-process status will be recorded after readback. No game maps, owner resource companions, shader instruction tokens, console keys or personal saves are included. See `NATIVE-CONTENT.md` and `SUPPORTED-MODS.md` for content scope and limits.

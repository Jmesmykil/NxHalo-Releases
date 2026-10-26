# Unified38 selective OpenCE fixes

Private candidate following user-confirmed Unified37 on Switch1. No U38 hardware or gameplay performance claim. Package and exact hashes in candidate-receipt.json. Working U37 package remains unchanged.

## Integrated
- d17ecdd6adc9: co-op BSP correction tests unit center rather than feet, preserving lunge correction.
- 667c2f29a024 + 07c8430b5ad4: bounded delayed inventory retry, applying available objects immediately and waiting for absent units/weapons; latest wins, expiry, reset, and eviction.
- 5744d2d17475: scenery and devices with shield-only vitality receive damage; clients simulate their shield recharge because these objects lack streamed shield state.

Upstream source: https://github.com/OpenCommunityEdition/OpenCE . Original project attribution/license retained. No original decomp exact-match bug restoration imported.

## Validation
Co-op8 cases and inventory7 cases (plus4 mutation controls) and scenery shield classification/recharge assertions pass with ASan/UBSan. Independent read-only reviews found no must-fix. Android-ABI Switch guest builds/links with no undefined symbols; native host builds. NSP CNMT title/version, container bounds and hashes validated. Version0.2.9.9-dk2, CNMT65561; loghalo-unified38-deko.log.

## Compatibility and remaining integration
Wire fields and version24 unchanged; default compatibility selection remains21. Older peers retain old scenery shield behavior: updated host AND clients are required for consistent scenery shields. Joining older peers is not proof of fixed behavior there.

The CE HUD384d9 patch is NOT included: ours has an older CE loader, while upstream uses a behavior registry, pixel-signature catalogue and new asset layout plus per-draw shader channels. A superficial transplant would be ineffective or corrupt texture selection. Details in hud/dependency-audit.md.

Host-selected shield cosmetics (message91/protocol36) and dedicated server/master browser (protocol39) remain OPEN coordinated Switch/Deck migrations. This candidate does not advertise those protocols or server compatibility. The existing Gun Game implementation differs from upstream's gun_game.c, so its battery fix was not blindly applied.

Public-release update and contribution PR are distinct work; U38 is not automatically substituted for user-accepted U37.

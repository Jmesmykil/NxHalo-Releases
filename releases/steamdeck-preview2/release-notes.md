# NxHalo Steam Deck preview 2 — lobby and map browser

This is a native 32-bit Linux pre-release on network version 11. It is built from the public `source/` tree at the source commit shipped with this release. It does not replace the latest public Switch release.

## Changes

- Renames the online campaign mode to **Co-op Campaign**.
- Replaces the fixed lobby panel with a responsive match details view and a scrollable player roster. The UI can display all 128 roster slots. The current practical match size is roughly 40 players; 128-player matches are not claimed or tested.
- Adds an opt-in **GET MAP & JOIN** action for a missing Custom Edition map. The client downloads the map archive, rejects unsafe archive paths, extracts only the requested map, checks the expected size and CRC, and joins the originally selected open lobby after success.
- Keeps the existing public internet/cross-console browser and campaign character feature.
- Adds a reproducible SteamOS-compatible math-symbol shim to the 32-bit build.

## Build and package evidence

- Built on the Omarchy Linux build host with `python3 configure.py --release --pgo=off --game-browser --portable` and `ninja linux`.
- Result is an i686 ELF and was checked to have no `GLIBC_2.43` undefined symbol requirement.
- SDL3 is bundled in the archive; no game content or console keys are included.
- Archive/client SHA-256 values are in `manifest.json` and the package's `SHA256SUMS`.

## Acceptance status

This is a new feature build, not yet a finished player-tested release. No hands-on Steam Deck launch, resolution sweep, live online lobby match, 128-player session, or real Custom Edition map download/join has been completed on this binary. The prior Preview 1 has Deck local/public network test evidence; that evidence does not validate these changes.

Friends/follow, voice chat, proximity chat, and player moderation/profile inspection are not included in this build.

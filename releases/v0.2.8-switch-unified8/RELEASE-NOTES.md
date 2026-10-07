# NxHalo Unified Switch Preview 2.8

This pre-release is a standalone Switch NRO and matching source snapshot. Guest and host builds completed; the NRO hash is recorded in the release manifest and `SHA256SUMS.txt`. Hardware installation and runtime acceptance are unverified: UMS was not mounted for staging, and this candidate has not been run on a physical Switch.

The candidate uses the existing title ID `010048414C210000` shared with Profile33. It is not an NSP and includes no new full-memory title override. Keep the earlier Profile33 NRO separately if using the same title entry for local testing; the prior release and Build14 stable campaign baseline remain available.

This NRO contains no Halo maps, map resource companions, console keys, or saves. Its host uses `romfs:/` only if `romfs:/maps/ui.map` exists; that file and maps are absent here, so the data root falls back to `sdmc:/switch/halo`. The save path is `sdmc:/switch/halo/save-community24`. Supply your own compatible game data and required CE companions. The included CA bundle is for verified TLS requests only.

The supplied Switch `halo.json` has the existing title ID and no explicit full-memory override. This NRO release does not configure a title override for any loader. Build prerequisites and the SDK-based build recipe are in `BUILD-SWITCH.md`; source and platform evidence are in the release assets. The Linux/Steam release is a separate artifact and does not establish Switch hardware acceptance.

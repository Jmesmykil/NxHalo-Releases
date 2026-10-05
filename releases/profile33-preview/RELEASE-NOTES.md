# NxHalo Profile33 — v0.1.11-p33 (pre-release)

Profile33 brings the Switch runtime to network protocol 11, matching the current Steam Deck / Linux preview. It includes the public-game browser and custom campaign characters. The creator reports cross-console multiplayer and public lobbies, with an approximate current match limit of 40 players.

## Package contents

- `Halo_CE_Profile33.nsp` — Switch runtime, display version `0.1.11-p33`, CNMT `65556`, title ID `010048414C210000`.
- `NxHalo-Profile33-runtime-source.zip` — matching Linux/client and Switch host source snapshot with notices and build guidance.
- `NxHalo-Setup-windows-AMD64.zip`, `NxHalo-Setup-darwin-arm64.zip`, and `NxHalo-v0.1.1-importer-source.zip` — unchanged offline setup tools from the Build14 release.
- Validation receipt, autonomous validation summary, hardware staging audit, manifest and SHA-256 sums.

## Install and data

Use the included setup app to prepare your own supported original Xbox Halo game data, then install the Profile33 NSP through DBI. No game maps, ROM, executable, shader instruction data, console keys or saves are included.

Profile33 has title ID `010048414C210000`; stable Build14 has title ID `010048414C4F0000`. Profile33 is a separate title and does not replace Build14 in place. Keep Build14 available if you want its stable rollback.

## Multiplayer and player count

Profile33 uses network protocol 11, with the in-game browser for public internet lobbies and cross-console multiplayer. The current approximate 40-player match limit is creator-reported. The available native public runs discovered lobbies listing 36 players and 32 players; these are discovery populations, not verified simultaneous participants or proof of a sustained 40-player match. A 100–128-player workload has not been observed or validated.

## Validation status

The NSP SHA-256 matches the Profile33 validation receipt and the UMS readback record. UMS staging verifies the installer file only: the available audit says it was not installed and not accepted on physical Switch hardware. Physical performance, visual-effects correctness, full campaign-route coverage, network loss/latency under load and the Profile33 Switch-to-Deck pairing remain unverified. The release receipt also marks campaign and multiplayer acceptance false under its current acceptance contract; treat this as a pre-release and review the included receipts before installing.

No game data or retail shader instruction tokens are included. A bit-identical rebuild is not promised. This is an independent community project and is not endorsed by the original game or console publishers.

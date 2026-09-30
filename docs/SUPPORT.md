# Updates, saves, and troubleshooting

NxHalo writes its own files under `sdmc:/switch/halo/`:

| Path | Purpose |
| --- | --- |
| `maps/` | User-imported game data and generated loading image |
| `save/` | Campaign save containers and profiles |
| `config.toml` | Settings in the new host candidate |
| `halo.log` | Native startup/performance diagnostics |
| `debug.txt`, `gamestate.txt` | Game diagnostics in the new candidate |

Back up this folder before updating. Install the new small app NSP over the same title; keep imported game data. Keep the folder when investigating installation failures.
Resume and update preservation still need device tests. These SD save containers
are not covered by backing up only DBI's general NAND save listing.

To roll back, reinstall your previous local NSP and retain both save copies.
The importer targets only maps, preserves saves, and requires an explicit SD choice.

## Troubleshooting

| Symptom | Next step |
| --- | --- |
| Image not recognized | Choose an extracted Xbox game/maps folder. Renaming an archive or PC release does not convert it. |
| Missing/mixed/incompatible maps | Use one complete Xbox dataset. The tool identifies missing names or incompatible headers. |
| Not enough space | Choose a larger local working/output disk; packaging temporarily needs several copies. |
| Runtime kit rejected | Re-extract a complete matching kit; keep its files and manifest together. |
| Packer/key failure | Check the matching local packaging tool and your key file. Raw packer output is withheld to protect private material. |
| DBI install error | Record the exact error, build ID, and package SHA-256 from the receipt. |
| HOME launch failure/crash | Record the screen message and copy `switch/halo/halo.log` after closing the title. Inspect crash reports for personal information first. |
| Persistent-storage checksum warning | Record mission, whether loading continued, and whether Resume works after relaunch. Preserve saves and logs. |
| Audio/effect/animation/stalls | Report mission/checkpoint, trigger, handheld/docked mode, and whether it repeats. |

## Reporting

Include build ID, game build, Switch model, Atmosphère/HOS versions, controller,
mission/checkpoint, and reproduction steps. State whether performance samples
include loading. A short clip can help.

Review logs for profile names and private paths before posting an excerpt.
Never attach ROMs/maps, console key files, saves, or your generated NSP to a public
issue. Retain original evidence privately. The issue forms are prepared for the
future public support channel; that channel is not open yet.

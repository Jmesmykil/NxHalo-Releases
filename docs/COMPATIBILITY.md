# Build13 campaign preview evidence

The creator reports improved Pelican flight and no Warthog stutter in the latest tested scenes. Small ground-battle spikes remain. Multiplayer is unsupported and has crashed. Full campaign completion is not certified.

The previous evidence below remains useful history; its FPS numbers are from earlier builds and are not Build13 benchmarks.

# Compatibility and current limits

This is an experimental original-Switch port. Physical gameplay was confirmed
on `native-ipc-20260929-3` and `native-profile-20260929-4`. The creator reports
the newer build loads faster and feels substantially smoother, with vehicle and
large-battle stalls still present. The evidence below is not general certification.

| Area | Evidence / limit |
| --- | --- |
| Native execution | Menu and campaign gameplay work on a physical Switch, without Xbox CPU emulation. |
| Game data | Xbox cache v5. NTSC `01.10.12.2276` tested on hardware; PAL `01.01.14.2342` accepted by the engine but device test pending. |
| Installation | Creator's full local NSP installs through DBI and launches from HOME. Small map-free app plus new automatic SD import still needs a clean device install. |
| Text and general colors | Creator reported readable text and normal colors after fixes. |
| Controllers | Input confirmed; separate Joy-Con/Pro Controller, reconnect, and multiplayer coverage pending. |
| Saves | Two captured Silent Cartographer saves have valid checksums. Resume after relaunch and across updates is unverified. |
| Performance | New run: 31 windows of 300 frames, 16.1–60.0 FPS averages including menu/loading/transitions; 131 frames over 100 ms. Old run: five windows at 18.2–48.7 FPS. Different scenes prevent a direct speedup comparison. Vehicle/fight stalls remain. |
| Loading display | Creator reports absent loading screens. Logs show a missing loading texture; source/render investigation remains open. |
| Audio | Delay and sounds remaining active after Pelican landing reported; unresolved. |
| Effects and animation | Elite shield effects and some mission animations reported incorrect; unresolved. |
| Campaign | Full campaign and every mission transition untested. |
| Other configurations | Switch 2, non-Xbox data, custom maps, languages, and networking unvalidated. |

The newer build repairs writable settings/log paths and guest file metadata,
and adds frame-stall/CRC measurements. Game debug logs now exist and the captured
native log has no read-only filesystem errors. No matched-scene FPS speedup is claimed.
A persistent-storage checksum warning occurred during the working run; valid
captured saves do not establish that every warning or resume case is resolved.

## Tests still required

- New-user image import, runtime-only app installation, and repeated cold launches.
- Joy-Con, Pro Controller, reconnect, checkpoint resume, update, and rollback.
- Mission transitions, audio, animation, shield effects, and frame pacing.
- Exact runtime/tool source and dependency notices, with no game data or keys in
  public packages.
- Desktop builder testing on each advertised operating system.

Linux emulator results help development but do not establish Switch performance
or full campaign readiness.

# NxHalo Build14 — v0.1.1 campaign preview

Build14 is the current public release for the original Nintendo Switch. It retains Build13's camera/vehicle corrections and moves transient in-session checkpoints to a bounded RAM snapshot, with the existing disk fallback when allocation is unavailable. Durable profile saves retain their existing disk path. This is a performance change, not a fix for a previously proven checkpoint failure.

## Install or update

1. Download `Halo_CE_Runtime.nsp` and the Windows x64 or Apple Silicon macOS setup ZIP below. Other platforms can use the importer source with Python 3.10+ and Tk.
2. For a first install, use the setup app to prepare your own supported original Xbox Halo image or complete extracted folder onto the SD. No ROM, maps, executable, shader data or console keys are included in these downloads.
3. If updating from a private build, run Prepare game data once to add the required `switch/halo/maps/shaders.bin`. Existing matching maps and saves are preserved. Existing public Build13 users with this file need only the new runtime.
4. Install `Halo_CE_Runtime.nsp` through DBI over the existing Halo CE title. Keep your imported game data, settings and saves. Launch Halo CE from HOME.

## Evidence and remaining issues

Private Build14 hardware logs recorded 6,900 frames across 271.632 profiled seconds and four successful 16 MiB RAM checkpoint saves taking 6.46–7.04 ms. No restore operation appeared in that captured run. The creator has reported working checkpoint behavior throughout earlier builds. These measurements do not establish a full-campaign pass or comparative frame-rate gain.

Shader compilation remains a source of first-use stutter: 35 of 37 distinct sampled frames over 100 ms included compilation/linking. Those are sampled records, not all frames. Battle spikes, loading display, audio, shield effects and animation remain under testing. Multiplayer is unsupported. No new Halo application crash report was found in the captured Build14 run.

The public package uses the same Build14 host and checkpoint code, with the existing public external-shader loader. Compilation, ABI, loader and checkpoint fixture checks passed. This exact public external-shader package has not yet had its first physical launch confirmed. It is released with that acceptance gap disclosed.

The internal application version remains `0.1.1-test14` and the log marker remains `native-profile-20260930-14-checkpoint`; these identify the frozen Build14 binaries. Public version: `v0.1.1-build14`. Title ID is unchanged, `010048414c4f0000`.

Standalone setup apps are unchanged from the reviewed Build13 additions and are unsigned previews. Source/component notices are included. Exact installed newlib source provenance and a clean-machine cross-build remain validation gaps; no bit-identical rebuild is promised.

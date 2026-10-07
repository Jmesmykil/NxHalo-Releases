# Unified Steam Deck Content Preview 2.9

This prerelease adds the CE community-map catalog, native texture-pack overrides, and Steam Deck rendering options. Linux executable SHA-256: `68e6da65bfd7533d2b13431087b0658e480748057d6e75ca663824956b04da13`. Preview 2.8 and 2.8.1 remain available.

## Community map catalog

The reviewed Deck screen shows a dynamic `SEARCH MAPS` field, 5,020 catalog entries, page 1 of 558, and populated results. Parser query behavior was unit-tested. Physical keyboard/controller search entry was not verified. The native fetch is capped at 8 MiB and rejects HTML/error pages where map data is expected. A listed map does not prove support for every engine extension it uses.

## Native texture packs

The screen shows texture overrides set to OFF. The native path supports unpacked texture packs and PNG RGBA, uncompressed 24/32-bit truecolor TGA, and DDS BC1/BC2/BC3 with validated mip chains. A synthetic TGA was traced through identity, decode, and GL upload for `sky\sky_x10\bitmaps\space`, bitmap 0; the capture did not show the override, so there is no visible pixel proof.

## Deck rendering options

The reviewed screen shows Deck upscaling off. Available modes are Off, Quality, and Performance; this is scene downscaling with spatial scaling and a mild unsharp pass before HUD composition, not FSR. Native software-GL world probes completed at 1280x800 in all three modes without framebuffer or shader errors. These probes do not measure physical Deck performance.

## Install and acceptance

The final Linux candidate passed a 22-second native CE DeathIsland smoke run with the actor loaded and gameplay ticks progressing. It is atomically installed and running on the Deck; PID 566275 and the destination executable both match this release hash. The previous executable remains available as `halo.before-content29-20261007` for rollback.

Full 128-player acceptance, live preset matches, Friends, and voice remain open. No game maps, owner resource companions, shader instruction tokens, console keys, or saves are included. See `NATIVE-CONTENT.md` for details and format limits.

# Supported content and verification — Unified Runtime Preview 2.9.4

## Community maps

The native catalog browses live HaloNet CE listings, shows paged and installed maps, and reports download and validation status. Catalog presence does not guarantee that a map or every engine extension will load. Supply compatible game files and owner-supplied CE `bitmaps.map`, `sounds.map` and `loc.map` companions when required. No maps or resource companions are included. OpenSauce `.yelo`, DLL hooks, Chimera plugins/scripts and other engine extensions need individual native implementations.

## Texture packs

The native override path accepts RGBA PNG, uncompressed truecolor TGA and validated DDS BC1/BC2/BC3 mip chains. Paths mirror original bitmap tag paths and append the bitmap ordinal. The GPU cache is limited to 64 entries / 128 MiB estimated mip storage. Invalid, unsupported or over-limit inputs fall back to original textures. Cube/volume and palette-only textures remain outside this override path.

A synthetic TGA was visibly rendered on the traced DeathIsland `scenery\halo\bitmaps\halo inner ring`, bitmap 0, in a native llvmpipe run. The enabled capture shows the checker mapped on the 3D ring; baseline shows its stock texture. This does not establish Deck texture-pack performance or every-pack compatibility.

## Deck rendering modes

Off / Quality / Performance is gated to Steam Deck hardware and defaults to Off. Quality uses two-thirds scene resolution; Performance uses half resolution. Spatial scaling and mild sharpening occur before HUD/text composition. This is not FSR. Earlier native software-GL probes ran all three modes at 1280x800 without shader/framebuffer errors; they do not measure Deck frame rates.

## Lobby capacity evidence

The retained native test displayed `128/128`, then selected the final roster entry: rows 118–128 of 128 with player slot 128 details. The test used one native host and 127 synthetic loopback peers. It is roster UI and selection evidence only, not a 128-user, 128-device or live 128-player match result.

## Online campaign and custom modes

The native online campaign menu/profile route and multiplayer presets are implemented in the Steam Deck/Linux executable. A natural server-list selection loaded an A30 room, while controlled two-peer campaign completion remains unverified. Match requirements and Zombies asset fallback remain map-specific.

## Acceptance boundaries

Physical Deck keyboard/controller search acceptance, real 128-user matches, sustained broad legacy-client interoperability, controlled end-to-end remote campaign play, and Friends/voice remain unverified. One Deck gameplay session was measured; see the manifest for its exact mode and limits. The separate Switch preview does not share this Steam executable. No owner resources, shader instructions, keys or saves are packaged.

# Supported content and verification — Unified Content Preview 2.9

## Community maps

The native catalog browses the live HaloNet CE listing, displays paged matches and installed maps, and exposes download and validation status. It showed 5,020 matching entries across 558 pages in the reviewed Deck capture. The 8 MiB text-catalog response limit and HTML/error-page rejection apply to download handling. A listing does not imply full game compatibility or support for every engine extension.

Supply your own compatible game files and CE `bitmaps.map`, `sounds.map` and `loc.map` resource companions when required. No game maps or companions are included. OpenSauce `.yelo`, DLL hooks, Chimera plugins/scripts and other engine extensions need individual native implementations.

## Texture packs

The native override path accepts the port's RGBA PNG, uncompressed truecolor TGA and validated DDS BC1/BC2/BC3 mip chains. Paths mirror original bitmap tag paths and append a bitmap ordinal. The cache is limited to 64 entries / 128 MiB estimated mip storage. Invalid, unsupported or over-limit inputs fall back to original textures. Cube/volume and palette-only textures are outside the current override path.

A synthetic TGA override was traced through bitmap identity, decode, GL upload and returned override for `sky\sky_x10\bitmaps\space`, bitmap 0. The rendered frame did not visibly show the override, so no real-pack visual acceptance is claimed.

## Deck rendering modes

Off / Quality / Performance is gated to Steam Deck hardware and defaults to Off. Quality uses two-thirds scene resolution; Performance uses half resolution. Spatial scaling and mild sharpening occur before HUD/text composition. This is not FSR. Native software-GL probes ran all three modes at 1280x800 without shader/framebuffer errors, but physical Deck performance was not measured.

## Acceptance boundaries

The populated catalog capture was reviewed. Physical search input and XTest search were not verified. Full 128-player acceptance, live preset matches, Friends and voice remain unverified. The Switch build keeps Deck-only upscaling disabled. No maps, owner resources, shader instructions, keys or saves are packaged.

# Native content, texture packs and rendering

## Map catalog and downloads

Open **Custom Maps & Mods > Custom Maps > Browse**. Search or page through the CE catalog, then select a map to download. On predecessor 0f6, native keyboard search and exact-row selection downloaded and installed two validated caches: `1bloodgulch` (16,610,796 bytes) and `3Tiers` (22,590,613 bytes). This verifies those two catalog paths only; it does not establish universal catalog coverage or physical Deck controller acceptance.

## Texture packs

Open **Custom Maps & Mods > Texture Packs & Display**. Import an unpacked texture-pack folder, select it and enable overrides. Original textures remain in use when an override is missing or invalid.

Texture paths mirror bitmap tag names, with the bitmap ordinal appended. Supported files include RGBA PNG, uncompressed 24/32-bit truecolor TGA and DDS BC1/BC2/BC3 with validated mip chains. Overrides cover supported 2D color textures; cube/volume and palette-only textures retain originals. The GPU cache is bounded to 64 entries and 128 MiB of estimated mip-chain allocations. Invalid, oversized or unsupported files fall back to the original bitmap. Imports reject symlinks and traversal and enforce file-count and total-size limits.

A native software-rendered visual comparison mapped a synthetic TGA onto the trace-confirmed DeathIsland `scenery\halo\bitmaps\halo inner ring` bitmap 0. The checker pattern is visible on the 3D ring in the enabled capture; the baseline shows the original ring texture. This run used Mesa llvmpipe and is not a Steam Deck GPU or physical performance test. It establishes the bitmap identity/decode/render path, not compatibility with every texture pack.

## Steam Deck upscaling

Off, Quality and Performance modes are available on Steam Deck hardware; Off is default. Quality renders the 3D view at two-thirds output dimensions; Performance uses half, with linear spatial scaling and mild sharpening before HUD/text composition. This is not FSR. Split-screen uses the original rendering path. Software-GL probes are not Deck performance measurements. One Deck Quality-mode Bloodgulch session on predecessor binary 6582 is summarized in VERIFICATION-EVIDENCE.md; it does not establish performance for every map or workload.

## Map and extension support

Supply compatible CE maps and any required owner-provided `bitmaps.map`, `sounds.map` and `loc.map` companions through the native importer. Content listing/import does not imply every map or extension works. OpenSauce `.yelo`, Chimera Lua/DLL and other engine extensions need individual native implementations. Releases contain no game maps or resource companions.

## Testing limits

Keep the 128-slot lobby result separate from actual-user capacity claims: the captured tail selection used one native host and 127 synthetic protocol peers. Full real-client capacity is unverified. Keep llvmpipe texture screenshots separate from Deck hardware rendering and performance evidence.

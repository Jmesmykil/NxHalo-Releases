# Unified Switch Content Preview 2.9

This prerelease provides a standalone Switch NRO and sanitized source archive. NRO SHA-256: `2a60465f0c928ed8007bfbe4e5b6fce7c1bad2c6713261515fb5e1158523d646`.

The guest ELF and native host NRO builds completed after the final menu caption sync. The title is `HALO: CE Content Preview 2.9`, version `2.9-preview`, using title ID `010048414C210000`. That ID is shared with previous NxHalo Switch candidates; keep a separate backup of the prior NRO for rollback. The NRO was copied to `switch/NxHalo-Content29/halo.nro` on the re-identified 65.2 GB USB volume and destination SHA-256 was verified; sync and safe eject completed. It was not installed to the Switch HOME menu, and no physical Switch runtime test was performed.

The build includes Switch SD browser pickers for `.map`/`.zip` map imports and texture-pack folders. Deck-only upscaling is forced off in the Switch guest. These compile/build facts do not establish physical UI or game acceptance. The sanitized source ZIP has 2,313 entries and excludes generated `.inc` files, game maps, owner resources, console keys, saves, and generated binaries. It may require separately sourced shader input for a local rebuild. No game assets are included.

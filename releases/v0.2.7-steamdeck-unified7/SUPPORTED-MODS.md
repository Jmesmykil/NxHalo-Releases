# Custom maps and mod development

Use **Custom Maps & Mods** in the main game. Import one local `.map` or a ZIP containing one map, or copy a HaloNet locator link/map name and choose Download. Existing files are preserved. Missing native CE lobby maps use the same bounded downloader before retrying the selected room.

CE cache version 609 is loaded directly by the native engine. This includes supported custom geometry and embedded tag content; it is not a conversion into Xbox cache version 5. Import required owner-supplied `bitmaps.map`, `sounds.map`, and `loc.map` companions into `assets/maps/ce`. Companion requirements depend on the map. Source ranges and referenced resources are validated before loading. No companion game data is included in releases.

Set Campaign Character or Host Match Character in the same menu. Host presets apply to server-created multiplayer/co-op player units and use existing object-definition replication. The map must contain the chosen biped; otherwise its standard character is used. Stock multiplayer maps do not gain foreign campaign assets automatically. Third-person campaign support remains in the runtime.

The first mod lane is supported map/tag content. Subsequent ports should add explicit capability declarations and tests for each new script function, tag group, shader feature or gameplay behavior, then verify host/client agreement. OpenSauce `.yelo`, Chimera Lua/DLL hooks and other classic PC engine extensions are not implemented merely by importing a map. Those need individual native implementations. See [CE map formats](https://c20.reclaimers.net/h1/maps/) and [OpenSauce](https://c20.reclaimers.net/h1/community-tools/opensauce/).

One executable hosts native V21, V20 or V11 profiles and selects the advertised profile when joining supported native rooms. V11 is multiplayer-only. Keep version filters separate. Classic Halo PC/CE address lists still require a classic-protocol implementation and are not native-joinable.

Verification: archive extraction fixtures and native linking passed; the Deck displays the content manager and network chooser. Private headless hosts initialized under all three profiles after a keepalive decoder memory fix. Real CE gameplay, two-real-client compatibility, and alternate-character multiplayer matches remain unverified. Every CE mod is not a supported claim.

# Custom maps, native texture packs and Steam Deck upscaling

## Find maps in game

Open **Custom Maps & Mods > Search / Download Community Maps**. Search the map name, move between pages, and select a map to download. The catalog uses HaloNet's live CE listing, caches it locally and shows installed maps. Download and validation failures appear in the status line. The catalog contains thousands of CE campaign and multiplayer entries; listing a map does not establish that every engine extension it uses is supported.

Supply your own compatible CE bitmaps.map, sounds.map and loc.map through the local importer. Resource files and downloaded game maps are not included in releases.

## Native texture packs

Open **Custom Maps & Mods > Texture Packs / Steam Deck Upscaling**. Import an unpacked texture-pack folder, choose it from the list, then enable overrides. Original textures are used whenever an override is absent or cannot be decoded. Importing and selecting a pack does not alter game maps.

Files mirror the original bitmap tag path, with its bitmap ordinal appended:

```
MyPack/
  sky/sky_x10/bitmaps/space__0.tga
  weapons/assault rifle/bitmaps/assault rifle__0.png
```

Names use `/` directory separators; ordinary spaces in tag components are supported. The ordinal identifies a bitmap within the bitmap tag, starting at zero. Supported images are the port's 8-bit RGBA PNG format, uncompressed 24/32-bit truecolor TGA, and DDS BC1/BC2/BC3 with validated mip chains. Native overrides currently cover 2D color textures; cube/volume and palette-only textures retain the original path. The GPU cache is bounded to 64 entries and 128 MiB of estimated mip-chain allocations. Oversized, malformed and unsupported files fall back to the original bitmap. Folder imports reject symlinks and traversal and have bounded file count and total size.

Maps can carry compatible custom geometry, weapons, models, sounds and scenario content in their native tags. Windows DLL hooks, OpenSauce .yelo files, and other engine extensions require individual native implementations; they are not installed or executed by this content browser.

## Steam Deck upscaling

The texture-pack settings screen includes **Off / Quality / Performance** on Steam Deck hardware. It is off by default. Quality renders the 3D view at two-thirds of output dimensions; Performance uses half. Linear spatial upscaling and a mild sharpening pass run before HUD/text composition. This is not FSR. Single-view gameplay is supported; split-screen falls back to the original rendering path. Switch does not enable this feature.

## Verification scope

The catalog parser was tested against a live HaloNet listing and rejected malformed/unsafe rows. Native software-GL probes loaded CE Death Island in Off, Quality and Performance at 1280x800 output, with clean exits and no framebuffer/shader errors. These probes do not measure physical Deck performance. Texture tests cover import bounds, malformed-image fallback, tag paths, mip uploads and cache retention; an actual native run recorded a synthetic TGA override being decoded, uploaded and returned for sky\sky_x10\bitmaps\space, bitmap 0. The captured camera did not show that overridden bitmap, so that trace is not screenshot-visible visual acceptance of a real texture pack.

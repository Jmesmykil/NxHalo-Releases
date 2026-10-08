# Online native texture packs

Open **Custom Maps & Mods → Mods & Display → Browse Online Texture Packs**.
Search by title, author, pack ID or required map; results use nine-item pages.
Choose a pack to read its author, license, download size and map requirements.
**Install Pack** downloads and verifies it. Installation leaves texture overrides
disabled and preserves the current selection. Use **Manage Installed Packs** to
select a pack, turn overrides on or off, and move a pack to recovery.

The first catalogue entry is **Alpine cliff detail**, one authored texture by
csauve under **CC BY-NC 4.0**. It only affects a loaded map containing
`levels/alpine/bitmaps/alpine_cliff_detail`; it does not change stock maps or
supply the Alpine map. Its original credits and license accompany the download.
The native PNG is a lossless conversion of the author's TIFF pixels.

## Compatibility

This catalogue installs native PNG, uncompressed true-color TGA and DXT1/3/5 DDS
2D image replacements. Pack paths name the bitmap tag and ordinal, for example
`levels/alpine/bitmaps/alpine_cliff_detail__0.png`. DLL, Lua, custom shaders,
weapon logic and engine extensions require separate native implementations.

Community maps use the separate Map Browser, with category, installed-state,
search and sorting controls. A texture pack does not change map compatibility,
weapon availability or networking profiles.

## Installation and attribution

Each entry declares an immutable ID/version, HTTPS source/archive, SHA-256,
format, map scope, compressed/unpacked size limits and image count. Downloads
are verified before descriptor-relative extraction. Unsupported entries,
traversal, links, duplicate names, encrypted archives, ZIP64, CRC/hash mismatches
and exceeded limits are rejected. Completed image packs are published atomically;
existing pack directories are never replaced. Failed imports leave no partial
pack in the library.

Attribution is retained separately under `native-mod-receipts/<id-version>`
beside the user's configuration. Pack images live in `texture-packs/<id-version>`.
Removing a pack through the menu moves it to `texture-packs-removed` for recovery.
Reimporting a removed name refreshes its GPU textures on the rendering thread.
A failed catalogue refresh retains the last validated catalogue.

The verified public catalogue is maintained at
[Native mods](https://github.com/Jmesmykil/NxHalo-Releases/tree/main/catalog).
Its per-package licenses remain separate from the runtime's source license.

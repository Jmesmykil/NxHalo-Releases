# Steam Deck/Linux 2.9.9 — lobby and round stability

## Play another round
The host's current map, gametype and custom rules remain selected when a multiplayer match returns to its lobby. Change the next map or gametype in Server Setup while everyone stays in the room. Co-op retains its separate mission progression.

Gun Game changes weapons as a transaction: an unsuccessful replacement keeps the previous weapon. Successful replacements remove spare weapons and select the new stage. Each new map resets the ladder.

## Find games
Online Play has separate Native V24, Native V21, legacy-native, broker/announced, campaign, multiplayer and classic PC/CE views. All includes versions outside the known native list; a listing alone does not make its protocol compatible. Selection follows the same endpoint/version when the list refreshes.

The native directory retains verified newer-version entries. Cached, empty, failed, stale-file and limited-count states are visible. Classic snapshot age is file age; it does not prove a server is currently reachable. A bounded directory cannot guarantee that every private or unannounced game exists in its list.

V24 is an explicit compatibility profile; existing 11–21 profiles remain available. V24 supports the official flat custom_maps\\name namespace, CE609 tag limits and authored-map vehicle selection. V21 remains the default host profile. Join acceptance is recorded separately from discovery.

## Maps and compatible mods
Maps → Installed provides a paged local inventory (up to 4096 entries) with search, map family, type, pages and details. Resource companions and invalid files are distinguished from playable maps; removed selections are rejected. A refresh updates the inventory without restarting.

Native folder import handles one real chooser selection or cancellation safely. Import does not enable a pack or change the current selection automatically. Compatible texture packages remain separately managed. Engine extensions such as DLL, Lua, Chimera and OpenSauce require native ports.

## Keyboard and existing controls
The name editor clears and rebinds its tag-backed key data across cache lifetime changes. Missing or malformed keyboard data is rejected. The previously confirmed Deck/PS5 controller route is preserved.

## Data and validation boundaries
Supply your own game data. New sword, faction, race or tower assets require the identical compatible shared map and its resource files. No maps, resource companions, console keys or personal saves are included.

This is a Linux release. Separate Switch hardware acceptance, real-player capacity, complete race routes, mode-specific bot behavior, Prop Hunt, full campaign lifecycle, persistent friends and human microphone testing retain their individual evidence requirements. Automated tests use silent audio and do not capture a hardware microphone.

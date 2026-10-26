# Native gametypes — Steam Deck/Linux 2.9.8

## Choose a mode
Multiplayer → Create Game → Host Multiplayer → select a map → STANDARD.
The normal picker includes SWAT, Tower of Power, Grenade Dodgeball, Zombies,
Native Race, Gun Game, Covenant-USMC, USMC-Flood, and Flood-Covenant.
The original standard gametypes remain available. Faction modes still require
suitable character assets in the selected shared map.

## Save your own rules
Multiplayer → Edit Gametypes → choose a template → edit or rename → OK.
Built-in templates remain read-only; editing one saves a custom copy.
Choose saved copies from CUSTOM when hosting. Renaming a copy preserves its mode.
Keeping the same base engine in Game Options preserves the special mode.
Selecting a different engine, a standard profile, or MODES → Standard clears it.

Gun Game fixes seven kills, free-for-all and kill-in-order off. Other compatible
settings such as the time limit remain editable. The host advertises the canonical
mode name in Halo's short network label; your custom profile keeps its saved name.

## Compatibility
Mode identity uses a checksummed optional local profile extension. Existing
profiles and the network variant layout remain compatible. No maps, keys or saves
are included. Updated hosts enforce rules through the existing native protocol.
This release updates Linux; the separate Switch executable is unchanged.
New weapons/models still require matching shared map assets.

## Evidence
Native UI: normal template picker, Gun Game renamed/saved/reloaded across processes,
custom time retained when hosting, same-engine editing preserves special rules.
32/64-bit tests cover nine virtual IDs, reserved-bit rejection and metadata codec.
Private Steam Deck GPU gameplay against unchanged2.9.6 native client passed,
including real input progressing pistol to assault rifle.
The prior full Gun Game match and user confirmation remain separate2.9.7 evidence.
Other old backlog items, full mode-specific gameplay matrices, physical Switch,
real128-player capacity and human microphone acceptance are not closed by this release.

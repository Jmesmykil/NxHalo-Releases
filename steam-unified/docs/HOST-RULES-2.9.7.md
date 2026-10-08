# Host rules 2.9.7 — Steam Deck and Linux

## Gun Game

Choose Gun Game in the host's combat modes list. It uses stock map weapons in this order:

1. Pistol
2. Assault rifle
3. Plasma rifle
4. Needler
5. Shotgun
6. Sniper rifle
7. Rocket launcher

Each credited enemy kill advances one stage. The next credited kill at the rocket stage wins. This is a kill-count ladder: melee and delayed-projectile kills count too; a kill does not have to come from the currently held weapon.

Rank survives deaths and suicides. A new player, recycled player slot or new match starts at the first stage. The normal Slayer score represents ladder progress; personal kill/death statistics remain separate.

The mode uses free-for-all, a seven-kill finish, no kill-in-order rule, no grenades, no vehicles and no map-placed weapons; pickups cannot change the weapon stage. Those defining Slayer options are fixed in the host's normal options page. Other compatible settings remain editable. Changing the base game in the rules editor or choosing Standard clears the special mode. Confirming the gametype selection preserves the chosen Combat mode.

Joining compatible native clients receive normal weapon, character and score updates. They do not need Gun Game code to join. This does not establish interoperability with unrelated classic PC/CE or browser protocol versions.

## Characters and forced loadouts

Zombies, SWAT, Tower of Power and Gun Game check whether the selected character's standing animations support the required weapons. When needed, the host chooses a compatible character already present in the map. The saved character preference remains unchanged; ordinary and faction matches retain their existing character selection.

Gun Game checks the entire weapon ladder before spawning. Missing weapon assets are not downloaded by this feature.

## Existing maps first

No new game maps, models, weapon assets or personal saves are included. Original-map race checkpoints remain unchanged so joining clients retain the correct race display. A completed large community race-map set is not included.

Arbitrary scenery disguises, new prop assets and client-dependent extensions remain later work. The plasma-pistol Prop Hunt selector is not implemented in this release.

## Updating

Keep the existing launcher, controller settings, maps and saves. Replace the runtime only after retaining the previous executable for rollback. The normal audio setting is preserved.

This package is for Steam Deck/Linux. It does not replace the separate Nintendo Switch executable. See the release evidence for the exact build hash and the tests completed on it.

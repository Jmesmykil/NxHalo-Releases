# Supported content and verification — Unified Preview 2.8.1

The native Linux runtime is tested with Halo CE and Xbox retail map paths. Supply your own game data. The content manager can import supported map files and single-map archives, download a map, and request the exact missing CE map for a join. CE maps may need owner-supplied bitmap, sound and localization resource companions; these are not bundled.

OpenSauce `.yelo`, Chimera scripts/plugins and other engine extensions are not included as native ports. Classic PC/CE address snapshots are separate and do not make classic peers native-joinable. Map-dependent Race geometry and Tower of Power assets must exist in each map/client.

For this candidate, prior Deck runs on the installed 2.8 binary confirmed a custom-map download/load/join, controller input from both tested inputs, campaign join-in-progress, a 28-player profile 11 Blood Gulch session, and profile 20 loading after an Infinity CE retry. The 2.8.1 build adds a guarded missing-CE-map rotation path; production guard mock cases passed and a short native missing-map host probe exited cleanly. That probe is not a real Deck rotation match. The 2.8.1 binary is not yet installed or run on the Deck.

Live preset matches, full 128-player acceptance, Friends and voice are unverified. The invalid menu index/widget cause is still under investigation. No claim is made that every CE mod is supported.

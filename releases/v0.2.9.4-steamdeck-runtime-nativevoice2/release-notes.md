## Current Steam Deck/Linux update: 2.9.4 nativevoice2

[Download runtime and matching source](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9.4-steamdeck-runtime-nativevoice2). Executable SHA-256: `45a62e4765fed3da8c6c895fb824d2e494bd759d7ec96414d743e790c57afc4d`. This exact build is installed on the Steam Deck.

Includes compatible-native proximity voice (default OFF, push-to-talk V/X/C), authenticated player routing, spatial mixing, repaired Audio Settings arrow hitboxes, and visible Race checkpoint warnings. Retains controller fixes, custom-map download/autojoin and co-op lifecycle fixes.

Three actual native clients passed generated-PCM transport, host relay and waveform checks with dummy audio: no sender loopback and expiry after release. Packet loss occurred; this does not establish lossless transport or player capacity. No physical microphone or external audio was used. Voice requires compatible clients; unchanged clients and the current Switch runtime do not gain voice. The mixer has 16 active stream slots and a tunnel budget of roughly five continuous speakers.

Actual Deck GPU multiplayer passed Off, Quality and Performance checks. At 1920x1080 output, observed internal scenes were 1920x1080, 1280x720 and 960x540 respectively. This is spatial scaling with sharpening, not FSR; no comparative performance benefit is claimed. Race warning was visibly verified on a private map copy lacking checkpoints, with the original valid map passing without a warning.

Persistent Friends, physical microphone/human PTT, natural campaign completion, native folder-picker acceptance, physical Switch acceptance and real 128-player capacity remain open. [Release evidence](releases/v0.2.9.4-steamdeck-runtime-nativevoice2/release-notes.md).


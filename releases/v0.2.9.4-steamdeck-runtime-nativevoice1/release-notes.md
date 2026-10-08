## Current Steam Deck/Linux update: 2.9.4 native voice foundation

[Download runtime and matching source](https://github.com/Jmesmykil/NxHalo-Releases/releases/tag/v0.2.9.4-steamdeck-runtime-nativevoice1). Executable SHA-256: `4a083770b6920d2a1f7372ba701967bcf02b840e0d2c3e9bfe7d55bf646964c0`. This exact build is installed on the Steam Deck.

Adds default-OFF compatible-native proximity voice with authenticated joined-player routing, spatial mixing, push-to-talk V/X/C, and expiry after release. Audio Settings spinner arrow hitboxes now match their drawn controls; click OK to save. Compatible native clients are required for voice; unchanged clients and the Switch runtime do not gain voice from this release.

Exact candidate passed a private Deck GPU multiplayer session with a native Linux peer. Two actual native clients passed generated-PCM capture-to-encrypted-transport-to-spatial-mixer testing with dummy audio and waveform analysis: 605 frames received/mapped, zero mapping drops, no sender loopback, and expiry after PTT release. Actual menu writes and fresh-process reload passed. No physical microphone or external audio was used.

Retains confirmed controller fixes, map download/autojoin, match customization and co-op lifecycle fixes. Voice is still a foundation: physical microphone/human PTT, third-client fanout, persistent Friends, natural campaign completion, physical Switch acceptance and real 128-player capacity remain open. The mixer has 16 active stream slots; the tunnel rate budget is roughly five continuously transmitting speakers, not 128-person voice. [Release evidence](releases/v0.2.9.4-steamdeck-runtime-nativevoice1/release-notes.md).


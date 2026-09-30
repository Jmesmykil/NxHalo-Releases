# NxHalo v0.1.0 experimental campaign preview

Native Halo: Combat Evolved campaign gameplay on the original Nintendo Switch. Supply your own compatible original Xbox game data; this project supplies no ROM downloads.

Build13 physical testing reports substantially smoother Pelican flight and no Warthog stutter in the tested scenes. Small spikes remain during large ground battles. This is a campaign preview: multiplayer is unsupported, full campaign completion is not certified, and audio, shield effects, animation, loading display and save/update coverage remain under testing.

The importer source requires Python 3.10+ with Tk. It works offline and prepares only the required maps from your own Xbox image or extracted folder. Existing different maps, saves and settings are preserved. Standalone desktop apps are not included.

Start with the included setup guide and known issues. Campaign stress testing and profiling continue alongside this release.

The public package loads its original shaders from your own game data. Prepare the complete game image or extracted folder with the included importer before launching; it creates `maps/shaders.bin` locally. This public external-shader packaging variant has passed compilation, ABI, loader and importer fixtures. Its first physical Switch launch is still pending; the Build13 core gameplay feedback is separate evidence. No game assets or console keys are included in these downloads.

The source snapshot includes full component notices and musl source. Exact installed newlib SDK source pin and a clean-machine cross-build remain maintainer validation gaps; no bit-identical rebuild is promised.

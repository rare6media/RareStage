# RareStage 0.31.0

## Highlights

* **Launch sequence.** The renderer opens on a six-second launch scene and settles into a standby scene that is also a
  test pattern (the Rare mark, a level ring and seven live audio-lane meters). Picking a scene starts that scene with
  the Director on auto; AUTO starts the Director. The console has its own launch animation.
* **New Dear ImGui console** (`RareStageConsole.exe`). Tabs: Live, Mix, Audio Lab, Director, Output, Production. The
  old console remains as `RareStageConsoleClassic.exe` until its Rehearsal, favourites and per-scene program pages are
  ported.
* **Seven independent audio lanes** (sub, kick body, bass, low-mid, mid, presence, air) with onsets, a Reactivity control
  and per-lane gains, visible in the Audio Lab and available to shaders.
* **Modulation matrix** and **procedural colour harmony**: audio and time sources routed to 32 semantic parameters;
  cosine palettes that follow the Director's colour and the section. Both are off by default (Mix tab).
* **Generated worlds.** Four new scene families built from reproducible specs and compiled in the background:
  Cosmic Space, Circuit Board, Virus Spread, Error Horizon. Each is qualified (GPU-timed offscreen) before use, and the
  Director brings them on at drops and phrase boundaries as a guest over its own show. Off by default (Director tab).
  See `docs/GENERATIVE_DIRECTOR.md`.
* **Eleven scenes retired** from rotation (hidden, not deleted, so numbering and hard-wired shader scene ids are
  unchanged): Signal Rail, Void Gates, Vector Speedway, Kinetic Monument, Domino Circuit, Pendulum Choir, Magnetic
  Assembly, Gearworks, Origami Parade, Radial Fans and Optical Weave. The catalog now has 36 active scenes and 202
  named programs.

## Upgrade notes

* The control protocol is **version 30**. The console and renderer must be from the same build; an older console shows
  "version mismatch".
* The constant buffer grew (1040 to 1280 bytes), so the **first launch of a build compiled from source recompiles the
  main shader** (several minutes, cached afterwards). The release package ships prepared shader bytecode, so it starts
  immediately.
* New command-line switches: `--no-intro` (renderer), `--no-splash` (console).
* New settings keys (all default to off or neutral): `Reactivity`, `BandGain0` to `BandGain6`, `Modulation`,
  `ProceduralPalette`, `Generative`.
* Pool slots 20 to 26 of the modulation parameters are reserved for the engine's motion clocks.

## Verification

Built with Visual Studio 2022 Build Tools (Release, x64). Passing at release: protocol, action director, colour
director, motion worlds, phrase conductor, generative foundation, world spec (including the GPU qualifier and the
launch scene on a real device), generative director policy, `--performance-self-test`, `--audio-self-test`,
`--director-render-test`, the console startup test and the shader cache test.

## Known issues and things not verified

* Generated-world choreography and the new launch scenes were verified with synthetic music values and captured
  frames; they have not been run through a long live show.
* The cross-fade of a generated world over the Director's picture was not watched live.
* GPU cost figures are estimates scaled from a reduced-resolution render.
* Qualification history and favourite worlds are not persisted.
* The GitHub Actions workflow was edited for the new targets and the version but has not been run.
* The README's older sections still describe scenes that are now retired.

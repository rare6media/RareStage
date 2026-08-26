# RareStage

**A native Windows festival-visual engine that turns the selected playback output into a persistent, evolving Rare-themed world in real time.**

[![Windows Build](https://github.com/rare6media/RareStage/actions/workflows/windows-build.yml/badge.svg?branch=main)](https://github.com/rare6media/RareStage/actions/workflows/windows-build.yml)

[Download the latest Windows build](https://github.com/rare6media/RareStage/releases/latest) · [View Windows builds](https://github.com/rare6media/RareStage/actions) · Current version: **0.10.0**

RareStage is a native C++20, Win32, DirectX 11 and WASAPI application. It is not a website, Electron shell, video player, or collection of prerecorded VJ loops. Every frame is produced on the GPU from live audio analysis, authored world logic, the Rare mark, and procedural 2D/2.5D stage systems.

## What changed in 0.10.0

Version 0.10.0 is the **Pulse Architecture pass**. It expands the articulated-panel language into a complete wall family, upgrades live rhythm tracking, and removes hard beat-phase jumps so visual motion stays liquid while kicks, tempo and percussion remain immediate.

- Twelve persistent worlds with environment, structure, atmosphere, foreground and logo layers
- Five authored acts per world: Open, Build, Drop, Breakdown and Drive
- Stagewall now performs eight readable cue families: columns, diagonals, center-out pressure, checker drives, organic sparse motion, helix flow, split gates and Rare mosaic
- Every Stagewall panel contains coordinated rotating micro-pixels with independent kick pressure and hi-hat lighting
- Bass pumps panel depth, scale, radial light and physical wall pressure without shaking the complete image
- A dedicated percussion transient drives short face glints and edge lights for hi-hats and fast percussion
- Original data-vault glyph columns, etched panel detail and brushed procedural surfaces stay crisp at native HD and Ultra 4K render scales
- Morph Dimension runs substantially slower and changes through deliberate column, diagonal and center-out patterns instead of free-running line motion
- Pixel Current cells organize into ribbons, diamonds, edge currents and logo assemblies while retaining independent shape, position and depth motion
- Three additional worlds: Prism Cascade, Kinetic Weave and Rare Eclipse
- Fast onset analysis gives kicks and drops a dedicated near-real-time path while longer envelopes keep movement smooth
- Adaptive multi-band onset analysis separates kick and high-percussion evidence while a recent-interval median stabilizes BPM
- Monotonic phase slewing nudges choreography onto the beat without teleporting visual time or making motion stutter
- Smart Director can enter Drop immediately when bass, onset and spectral evidence agree; other act changes use shorter confirmation without scene roulette
- Every world runs on an autonomous visual clock, so silence changes the composition into an idle performance instead of freezing it
- Destination-authored transitions mapped to each world's visual grammar
- Dynamic Rare-logo roles including panel mosaic, sliced prism mask, woven carrier, eclipse reveal and DVD-style wall bounce
- Independently phased cells, motes, filaments and pixel depth keep every world alive during quiet passages
- Pixel Current can assemble its moving cells into the traveling mark; impacts drive reactive wall light, color washes and a rare corner burst
- Three new wall-led worlds: Kinetic Array, Lightwell and Pixel Loom
- No ray-marched centerpiece, seven-logo pile, repeated watermark stack, global radial-ring template, or universal white transition flash
- Reduced HDR bloom and a stronger filmic detail pass for crisp stage output

The research and design synthesis behind this release is documented in [Stage visual research, 2023–2026](docs/STAGE_VISUAL_RESEARCH_2023_2026.md).

## Engine at a glance

| Area | Current implementation |
|---|---|
| Platform | Native Windows x64, C++20, Win32 and DirectX 11 |
| Audio | WASAPI loopback from the selected active Windows playback device |
| Analysis | 1024-point FFT, adaptive multi-band spectral flux, bass, mids, highs, RMS, peak, stereo energy, separate kick and percussion transients, robust recent-interval BPM and tempo confidence |
| Song structure | Hysteresis-based Open, Build, Drop, Breakdown and Drive detection |
| Visual system | Twelve authored Rare worlds with act-specific 2D animation, layered parallax and selective 2.5D depth |
| Director | Audio/section scoring, world dwell, recent-world memory, phrase timing, palette morphing and beat-aligned chapter changes |
| Renderer | Half-float HDR scene pass, half-resolution bloom, sharpened filmic composite |
| Show control | Direct2D/DirectWrite overlay with mouse, wheel and keyboard operation |
| Output | Borderless stage display, multi-monitor selection and clean Spout2 sender |
| Performance | 62%, 78%, 100% and 125% HDR render scales; VSync or unlocked output |
| Persistence | Settings and four show presets under `%LOCALAPPDATA%\RareStage` |

## Rare worlds

| Key | World | Visual direction | Rare-mark role |
|---|---|---|---|
| `1` | Morph Dimension | Slow circle-to-square fields, controlled negative space and beat-phrased column, diagonal and center-out assembly | A single mark gradually resolves from the field during builds and drops |
| `2` | Rare Biosphere | Flowing membranes, organic tech filaments, living nodes, tendrils and layered cellular currents | One moving mark is carried through and connected to the ecosystem |
| `3` | Celestial Garden | Layered hills, aurora ribbons, stars, spores and bioluminescent landscape motion | A small crest lives inside a drifting moon |
| `4` | Stagewall | A full data-lit cube facade with panel turning, extrusion, disappearance, reassembly and cue-driven column, diagonal, radial, checker and sparse patterns | The mark is distributed across lit panel faces as a mosaic instruction, never pasted over the wall |
| `5` | Neon Ocean | Flowing currents, animated horizon, aurora sky, wave bands and reflected light | A horizon crest creates a broken reflection across the water |
| `6` | Pixel Current | Shape-changing cells organized into moving ribbons, diamonds, edge currents, depth layers and smooth dissolution | One mark follows a DVD-style wall-bounce path; impact lighting reacts to the track and rare corner hits receive a larger burst |
| `7` | Prism Cascade | Bold opposing prism shutters, diagonal seams, staggered lane pulses and musical slice movement | A large sliced mark appears through the prism structure during builds, drops and drives |
| `8` | Kinetic Weave | Seven broad woven ribbons, crossing signal threads, moving junctions and layered occlusion | A moving mark is carried behind the weave and is partially hidden by foreground ribbons |
| `9` | Rare Eclipse | A drifting eclipse body, changing crescent, asymmetric beam fan, partial arc and horizon light | The mark is revealed inside the moving eclipse only during higher-energy acts |
| `Q` | Kinetic Array | Articulated tile banks rotate, drift, vanish and reform through columns, diagonals, ripples, orbital patterns, zippers and coordinated sub-pixel motion | The mark appears only as a temporary negative-space formation |
| `W` | Lightwell | Concentric panel rings, counter-rotating sectors, beam fans and a bass-pressurized central halo | A restrained Rare gobo briefly cuts through the light during higher-energy acts |
| `E` | Pixel Loom | Dense pixels counter-rotate in synchronized groups, then merge into ribbons, diamonds, helixes, curtains and flowing fields | The mark acts as a temporary pixel attractor rather than a composited overlay |

Every world contains five acts. An Open establishes form; Build assembles and adds directional movement; Drop sharpens hierarchy and adds controlled impact; Breakdown opens negative space and atmosphere; Drive sustains kinetic motion without repeatedly shaking or spinning a model.

## Smart Director

Smart Director maintains short- and long-window energy models and confirms musical stages with hysteresis. A section change transforms the **current world first**. Cross-world changes are considered only at a fresh Open, Drop or Breakdown after the minimum dwell, or at the 120-second safety limit. When BPM confidence is usable, the requested dwell is quantized to eight-beat phrases and the final change waits briefly for a beat/spectral opportunity.

World selection scores current energy, band balance and musical act, penalizes the active and recently used worlds, and retains deterministic Show Timeline mode for rehearsed playback. Palettes morph continuously rather than cutting when the director changes color direction.

```text
Windows playback device
        │
        ▼
WASAPI loopback ─► FFT + beat/tempo ─► five-act song model
                                                │
                                                ▼
                                      persistent world director
                                                │
                ┌───────────────────────────────┼───────────────────────────────┐
                ▼                               ▼                               ▼
        environment/atmosphere          structure + motion              logo choreography
                └───────────────────────────────┬───────────────────────────────┘
                                                ▼
                                      HDR ─► bloom ─► filmic output
                                                │
                                clean Spout2 ◄──┤──► Show Control + display
```

The `RareStage Clean` Spout sender is published before Show Control is composited, so OBS, RareOBS, Resolume and other Spout receivers get the finished visual without the operator panel.

## Audio response

| Signal | Primary visual purpose |
|---|---|
| Bass | Sustained structural weight, membrane/floor emphasis, water displacement and broad light pressure |
| Kick | Near-real-time panel depth, controlled composition pump, radial pressure and short lighting hits |
| Mids | Flow speed, parallax, ribbons, currents and environment travel |
| Highs | Fine detail, stars, windows, edges and atmospheric particles |
| Beat | Pattern-grid timing, phrase boundaries and transition opportunities |
| Onset | Immediate kick/drop evidence and transient lighting before the longer envelopes catch up |
| Percussion | Fast hi-hat and percussion edge glints, panel-face specular hits and micro-light accents |
| Spectral flux | Musical-change evidence, build detection and structured signal fragments |
| BPM/tempo confidence | Stable animation phase, monotonic beat nudging and eight-beat dwell quantization |

Reaction profiles and per-band gains remain available. Autonomous motion is continuous; audio changes force, light, density and choreography. Longer movement envelopes stay smooth while the separate onset path preserves immediate impact.

## Show Control

Press `Tab` to show or hide the in-compositor control panel.

| Overlay row | Values or range |
|---|---|
| Reaction | Calm, Balanced, Stage, Aggressive |
| Palette | Rare Cyan, Nebula, Sakura, Solar; automatically morphed by Smart Director |
| Sensitivity | 50–150% |
| Bass / Mids / Highs / Beat | 50–150% each |
| Logo | 75–125% |
| World dwell | 40–120 seconds |
| Transition | 0.2–3.0 seconds |
| Transition FX | Morph Match, Horizon Rise, Signal Veil, Gate Open |
| Render profile | Performance, Balanced, Native HD, Ultra 4K |
| Frame output | Display/VSync or unlocked high refresh |
| Show mode | Smart Director or Show Timeline |
| Spout output | Clean sender on or off |

The panel reports the current world, current act, automation state, preset, automatic palette, playback device, BPM and live bass/mid/high/beat meters.

## Controls

| Input | Action |
|---|---|
| `1`–`9` | Select worlds 1–9 and enter manual world mode |
| `Q` / `W` / `E` | Select Kinetic Array, Lightwell or Pixel Loom |
| `[` / `]` | Previous or next world |
| `0` or `A` | Toggle Smart Director |
| `D` | Cycle active Windows playback devices |
| `M` | Move stage output to the next monitor |
| `P` | Cycle Rare color palettes |
| `Tab` | Show or hide Show Control |
| Left click | Select a row or use its minus/plus buttons |
| Right click | Decrease the clicked row |
| Mouse wheel | Adjust the row under the pointer |
| `Up` / `Down` | Select the previous or next row |
| `Left` / `Right` / `Enter` | Adjust the selected setting |
| `F1`–`F4` | Load show preset 1–4 |
| `Shift+F1`–`Shift+F4` | Save over preset 1–4 |
| `T` | Toggle Smart Director and deterministic Show Timeline |
| `V` | Toggle VSync and unlocked output |
| `S` | Toggle the clean Spout2 sender |
| `Space` | Freeze or resume automatic world changes; live audio motion continues |
| `F11` or `Alt+Enter` | Toggle borderless fullscreen |
| `Esc` | Leave fullscreen, or quit when windowed |

## Factory show presets

| Slot | Direction | Starting world | Default dwell / transition |
|---|---|---|---|
| `F1` Clean Open | Calm, Rare Cyan | Celestial Garden | 88 sec / 1.8 sec Horizon Rise |
| `F2` Melodic Bloom | Balanced, Sakura | Morph Dimension | 72 sec / 1.4 sec Signal Veil |
| `F3` Rare Drop | Stage, Rare Cyan | Rare Biosphere | 56 sec / 1.2 sec Gate Open |
| `F4` Glitch Finale | Aggressive, Nebula, timeline | Stagewall | 48 sec / 0.9 sec Signal Veil |

Saved presets include the current world, reaction tuning, fallback palette, dwell, transition, render profile, output mode, automation, timeline, Spout and monitor.

## Download and run

1. Open [Releases](https://github.com/rare6media/RareStage/releases/latest).
2. Download `RareStage-windows-x64.zip`.
3. Extract the complete `RareStage` folder.
4. Run `RareStage.exe`.

Keep `assets/rare-logo.png` beside the executable in the packaged `assets` folder. If startup fails, inspect `RareStage-startup.log` beside the executable.

### System requirements

- 64-bit Windows 10 or Windows 11
- DirectX 11-capable GPU and current graphics driver
- Active Windows playback device for live reactivity
- Spout-capable receiver only when shared-texture output is needed

## Performance profiles

| Profile | HDR scene scale | Intended use |
|---|---:|---|
| Performance | 62% | Lower-power GPUs or high-refresh output |
| Balanced | 78% | Reduced GPU load with native-resolution final composite |
| Native HD | 100% | Full native scene rendering |
| Ultra 4K | 125% | Supersampled output on high-end GPUs |

VSync is enabled by default. The 0.10.0 shader uses bounded loops, branch-local world logic and procedural 2D/2.5D layers. Stagewall, Kinetic Array, Lightwell, Pixel Loom and the other world systems execute only inside their selected branch to protect high-refresh stage performance.

## Build from source

Install Visual Studio with **Desktop development with C++**, the Windows SDK, Git and CMake. From Developer PowerShell:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --target RareStage --parallel
```

Or run `./build-release.ps1`. CMake retrieves Spout2 and links `SpoutDX_static`; configure with `-DRARESTAGE_ENABLE_SPOUT=OFF` to build without Spout.

## Automated validation and release

Every push and pull request runs the Windows workflow:

1. Configure the x64 CMake project.
2. Build optimized `RareStage.exe`.
3. Compile-test every embedded DirectX shader.
4. Launch the native executable in startup-test mode with the alpha logo present.
5. Package the executable, logo, README and notices.
6. Upload the portable ZIP.

Successful pushes to `main` publish or refresh the **v0.10.0** GitHub release automatically.

## Roadmap

### v0.10.0 — Pulse Architecture (current)

- Twelve persistent environments with five musical acts each
- Adaptive kick, percussion, onset and spectral-flux analysis
- Robust recent-interval tempo tracking with monotonic phase slewing
- Eight Stagewall cue families and synchronized rotating panel sub-pixels
- Kinetic Array, Lightwell and Pixel Loom wall-led worlds
- Slower liquid Morph Dimension without hard audio-clock jumps
- Chapter-aware Smart Director affinities for all twelve worlds
- Stage-production research and documented scene grammar

### v0.11 — Smart Director telemetry

- Predictive build/drop pre-roll for transitions that land on the next musical boundary
- Act-aware scene memory so a set develops without repeating the same world/act combinations
- Director telemetry and diagnostics for tuning automatic decisions from real shows
- Capturable signal traces for kick, hats, tempo confidence, phrase timing and section decisions

### v1.0 — Production reliability

- In-app GPU/audio diagnostics and render-budget telemetry
- Device-loss recovery and clearer crash reports
- Signed Windows installer and update channel
- Automated reference-frame and performance regression tests

### Later stage systems

- NDI output
- Synchronized multi-machine playback
- External show-control API
- Venue profiles and project-level show files

## Repository layout

```text
assets/rare-logo.png            Alpha Rare mark used by the world renderer
src/main.cpp                    Win32 app, audio engine, director, HLSL and renderer
.github/workflows/              Windows build, test, package and release automation
docs/                           Stage-production research and system design notes
CMakeLists.txt                  Native build and Spout2 integration
build-release.ps1               Local one-command Release build
THIRD_PARTY_NOTICES.md          Third-party licensing
```

RareStage includes [Spout2](https://github.com/leadedge/Spout2) under the BSD 2-Clause License. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

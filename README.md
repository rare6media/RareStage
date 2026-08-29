# RareStage

**A native Windows festival-visual engine that turns the selected playback output into a persistent, evolving Rare-themed world in real time.**

[![Windows Build](https://github.com/rare6media/RareStage/actions/workflows/windows-build.yml/badge.svg?branch=main)](https://github.com/rare6media/RareStage/actions/workflows/windows-build.yml)

[Download the latest Windows build](https://github.com/rare6media/RareStage/releases/latest) · [View Windows builds](https://github.com/rare6media/RareStage/actions) · Current version: **0.10.3**

RareStage is a native C++20, Win32, DirectX 11 and WASAPI application. It is not a website, Electron shell, video player, or collection of prerecorded VJ loops. Every frame is produced on the GPU from live audio analysis, authored world logic, the Rare mark, and procedural 2D/2.5D stage systems.

## What changed in 0.10.3

Version 0.10.3 is the **Liquid Stagecraft pass**. Stagewall, Kinetic Array and Panel Cascade establish the quality bar; the rest of the show now uses the same production discipline without repeating their exact geometry.

- Shared eight-beat formation grammar with smooth interpolation and no raw transient-driven position or rotation
- Kick controls short physical pressure, bass controls sustained depth, snare turns coordinated banks, mids conduct travel, and hats illuminate edges
- Morph Wall, Bioframe and Celestial Rig rebuilt as cohesive glass-panel systems with far less background clutter
- Wavewall rebuilt as a controlled articulated-slat stage with six authored depth formations
- Pixel Bank retains the DVD-style Rare mark but replaces wire waves and drifting confetti with a calm rotating support field
- Prism Cascade removes logo slicing and background judder in favor of continuous counter-moving shutter layers
- Kinetic Weave now tells a four-layer visual story: atmosphere, bass pressure, mid-led ribbons and hat detail
- Aperture Stage replaced by **Chroma Blades**, an eight-formation bank of articulated elongated panels
- Lightwell replaced by **Kinetic Canopy**, a suspended ceiling-panel and runway-light environment
- Pixel Loom rebuilt as a 22×14 grouped pixel instrument with eight production cues and negative-space branding
- Matrix Vault now travels in one slow, predictable direction while preserving its data-lighting language
- Signal Atrium replaced by **Signal Rail**, a perspective transport system of synchronized light packets and floor rails
- RareStage Console keeps full control of all fifteen worlds, acts, live mix, Smart Director, output, presets and renderer lifecycle through protocol v3
- The release gate still includes the Console-owned Stage launch soak test that guards against the launch-and-exit regression

The research and design synthesis behind this release is documented in [Stage visual research, 2023–2026](docs/STAGE_VISUAL_RESEARCH_2023_2026.md).

## Engine at a glance

| Area | Current implementation |
|---|---|
| Platform | Native Windows x64, C++20, Win32 and DirectX 11 |
| Audio | WASAPI loopback from the selected active Windows playback device |
| Analysis | Sample-rate-aware 2048-point FFT with 50% overlap, adaptive band-limited spectral flux, sustained bass, kick, snare, hats, mids, highs, RMS, robust recent-interval BPM and tempo confidence |
| Song structure | Hysteresis-based Open, Build, Drop, Breakdown and Drive detection |
| Visual system | Fifteen authored Rare worlds with act-specific 2D animation, layered parallax and selective 2.5D depth |
| Director | Audio/section scoring, world dwell, recent-world memory, phrase timing, palette morphing and beat-aligned chapter changes |
| Renderer | Half-float HDR scene pass, half-resolution bloom, sharpened filmic composite |
| Show control | Bundled RareStage Console plus a Direct2D/DirectWrite in-renderer overlay |
| Output | Borderless stage display, multi-monitor selection and clean Spout2 sender |
| Performance | 62%, 78%, 100% and 125% HDR render scales; VSync or unlocked output |
| Persistence | Settings and four show presets under `%LOCALAPPDATA%\RareStage` |

## Rare worlds

| Key | World | Visual direction | Rare-mark role |
|---|---|---|---|
| `1` | Morph Wall | Glass panels carry circle, square and diamond motifs through columns, diagonals, radial pushes, split gates, checkers and wings | The mark appears only as a negative-space wall formation |
| `2` | Bioframe | Restrained glass-membrane tiles perform rise, branch, bloom, split and field formations over one calm refractive environment | The mark influences one panel formation instead of floating over the scene |
| `3` | Celestial Rig | A cohesive suspended glass rig moves through columns, rows, orbits, diagonals, clusters and open gates | No persistent logo; the rig, horizon and sparse orbital light carry the identity |
| `4` | Stagewall | A full data-lit cube facade with panel turning, extrusion, disappearance and sixteen cue families | The mark is distributed across lit panel faces as a mosaic instruction, never pasted over the wall |
| `5` | Wavewall | Articulated vertical slats perform columns, diagonals, gates, tiers, fans and pressure fronts over a restrained stage floor | No foreground watermark; the complete slat stage carries the Rare movement language |
| `6` | Pixel Bank | Stable grouped pixels slowly rotate in and out as an abstract support loop | One polished DVD-style mark conducts wall impacts, corner bursts and reactive edge light |
| `7` | Prism Cascade | Two continuously moving prism banks perform columns, counters, gates and diagonals with signal-separated lighting | One stable mark sits behind the shutter architecture without slicing or glitch displacement |
| `8` | Kinetic Weave | Atmosphere, bass pressure, mid-led woven ribbons, counter-threads and hat detail evolve as separate story layers | A restrained mark is carried behind the weave and naturally occluded by it |
| `9` | Chroma Blades | Elongated articulated glass blades perform columns, diagonals, radial pushes, gates, fans, blocks, cascades and crosses | No logo; motion, color and stage framing define the world |
| `Q` | Kinetic Array | Articulated tile banks rotate, drift, vanish and reform through fourteen authored formations and coordinated sub-pixel motion | The mark appears only as a temporary negative-space formation |
| `W` | Kinetic Canopy | Suspended ceiling panels perform rows, columns, diagonals, split gates, checks, fans and lifts above a perspective runway | No logo; canopy architecture and converging floor light define the identity |
| `E` | Pixel Loom | A 22×14 grouped pixel instrument performs columns, diagonals, radial waves, gates, blocks, helixes, cascades and a Rare field | The mark becomes temporary negative space inside the pixel system |
| `R` | Matrix Vault | Receding architectural frames move steadily in one direction behind grouped data panels, etched glyph light and corridor pressure | The mark appears as temporary negative space inside the vault, never as a foreground watermark |
| `Y` | Panel Cascade | Multi-panel banks fold, descend, disappear and reform through row waves, diagonals, curtains, blocks and a Rare formation | The mark becomes one synchronized formation during selected high-energy cues |
| `U` | Signal Rail | Nine perspective rails carry synchronized light packets from a distant horizon through six phrase formations | No logo; transport motion, lane color and architectural convergence make the world distinct |

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
| Snare | Coordinated row, bank and face turns that read as one deliberate structural accent |
| Mids | Flow speed, parallax, ribbons, currents and environment travel |
| Highs | Fine detail, stars, windows, edges and atmospheric particles |
| Beat | Pattern-grid timing, phrase boundaries and transition opportunities |
| Onset | Immediate kick/drop evidence and transient lighting before the longer envelopes catch up |
| Hi-hats | Fast edge glints, panel-face specular hits and micro-light accents without moving complete structures |
| Spectral flux | Musical-change evidence, build detection and structured signal fragments |
| BPM/tempo confidence | Stable animation phase, monotonic beat nudging and eight-beat dwell quantization |

Reaction profiles and per-band gains remain available. Autonomous motion is continuous; audio changes force, light, density and choreography. Longer movement envelopes stay smooth while the separate onset path preserves immediate impact.

## RareStage Console

Run RareStageConsole.exe from the same folder as RareStage.exe. Console can launch or focus the renderer and remains isolated from the clean program output.

| Page | Controls |
|---|---|
| Show | All fifteen worlds, five act cues, previous/next, Smart Director, automation hold, Clean Open, stage HUD, load/save presets, blackout, and safe fallback |
| Mix + Director | Reaction profile, palette direction, sensitivity, bass, mids, highs, beat, logo scale, act override, automatic act resume, Smart Director, and timeline mode |
| Output | World dwell, transition duration and language, render profile, VSync/high-refresh output, Spout, fullscreen, HUD, audio device, monitor, timeline mode, and renderer shutdown |

Protocol v3 uses a local shared-memory telemetry snapshot and acknowledged window messages. The renderer remains authoritative, publishes a heartbeat at up to 40 Hz, rejects out-of-range values, and reports the complete live state—including separate kick, snare and hats meters—back to Console. Engine and Console must come from the same release package.

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

The in-renderer panel and Console manipulate the same settings. Both report the current world, act, automation state, preset, automatic palette, playback device, BPM, and live frequency/transient meters.

## Controls

| Input | Action |
|---|---|
| `1`–`9` | Select worlds 1–9 and enter manual world mode |
| `Q` / `W` / `E` | Select Kinetic Array, Kinetic Canopy or Pixel Loom |
| `R` / `Y` / `U` | Select Matrix Vault, Panel Cascade or Signal Rail |
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

When Console has focus, `1`–`9`, `Q`, `W`, `E`, `R`, `Y`, and `U` select all fifteen worlds. `A`, `Space`, `B`, `O`, `F`, `H`, `D`, `M`, `T`, `F1`–`F4`, and `Shift+F1`–`Shift+F4` mirror the primary show cues.

## Factory show presets

| Slot | Direction | Starting world | Default dwell / transition |
|---|---|---|---|
| `F1` Clean Open | Calm, Rare Cyan | Celestial Rig | 88 sec / 1.8 sec Horizon Rise |
| `F2` Melodic Bloom | Balanced, Sakura | Morph Wall | 72 sec / 1.4 sec Signal Veil |
| `F3` Rare Drop | Stage, Rare Cyan | Bioframe | 56 sec / 1.2 sec Gate Open |
| `F4` Glitch Finale | Aggressive, Nebula, timeline | Stagewall | 48 sec / 0.9 sec Signal Veil |

Saved presets include the current world, reaction tuning, fallback palette, dwell, transition, render profile, output mode, automation, timeline, Spout and monitor.

## Download and run

1. Open [Releases](https://github.com/rare6media/RareStage/releases/latest).
2. Download `RareStage-windows-x64.zip`.
3. Extract the complete `RareStage` folder.
4. Run `RareStageConsole.exe` for complete control, then choose **Launch Stage**. Running `RareStage.exe` directly remains supported.

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

VSync is enabled by default. The 0.10.3 shader uses bounded loops, branch-local world logic and procedural 2D/2.5D layers. Only the selected world executes, protecting high-refresh stage performance as the articulated stage family expands.

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
2. Build optimized `RareStage.exe`, `RareStageConsole.exe`, and the protocol tests.
3. Run the Console protocol round-trip tests.
4. Run deterministic kick, snare, and hi-hat classifier separation tests.
5. Compile-test every embedded DirectX shader.
6. Launch-test both native executables with the alpha logo present.
7. Package the renderer, Console, logo, README and notices.
8. Upload the portable ZIP.

Successful pushes to `main` publish or refresh the **v0.10.3** GitHub release automatically.

## Roadmap

### v0.10.3 — Liquid Stagecraft (current)

- Scene 1: cohesive glass morph wall; shape changes and bank motion remain liquid between cues
- Scene 2: retain the glassmorphic material while reducing the background to one authored environment
- Scene 3: preserve the subtle vibration but coordinate it per bank rather than twitching every tile independently
- Scene 4: preserve Stagewall and its sixteen proven cue families as a production benchmark
- Scene 5: replace the overloaded wave treatment with a finished six-formation articulated-slat stage
- Scene 6: polish the DVD behavior and replace wire/confetti motion with slow rotating support pixels
- Scene 7: remove sliced-logo glitch and background judder; separate bass, mids and hats across stable prism layers
- Scene 8: build a smooth narrative stack in which atmosphere, pressure, ribbons and detail each have a musical role
- Scene 9: replace Aperture Stage completely with the eight-formation Chroma Blades system
- Scene 10: preserve Kinetic Array and carry its formation discipline into other worlds
- Scene 11: replace Lightwell completely with Kinetic Canopy and a perspective runway environment
- Scene 12: rebuild Pixel Loom as a dense, grouped, high-detail stage instrument rather than loose moving pixels
- Scene 13: keep Matrix Vault's data language while making depth travel slow, monotonic and predictable
- Scene 14: preserve Panel Cascade as the other production benchmark
- Scene 15: replace Signal Atrium with a distinct perspective light-transport world, Signal Rail
- Keep autonomous motion alive during silence; use kicks, bass, snare, mids and hats as conductors rather than animation clocks
- Preserve complete RareStage Console control and the Console-owned Stage launch soak regression gate

### v0.10.4 — Live calibration and stage expansion

- Capturable per-band traces and detector confidence overlays for real-room kick, snare and hat calibration
- Automatic device/sample-rate calibration profiles with detector presets for EDM, bass music and melodic sets
- Additional Stagewall-grade worlds based on kinetic facades, suspended architecture and large-format negative space
- Reference-frame and timing captures for motion smoothness, cue readability and GPU budget

### v0.11 — Smart Director 2

- Predictive build/drop pre-roll so transitions land on the next musical boundary
- Act-aware world memory so a set develops without repeating the same world/act combinations
- Confidence-aware tempo recovery and musical-ending behavior
- Director diagnostics for world choice, palette choice, section evidence and rejected transitions

### v0.12 — Stage Production Pack

- Venue-safe brightness and contrast profiles
- Alternate stage aspect ratios and mapped-panel compositions
- NDI output and synchronized multi-machine playback groundwork
- Production presets for open, support, peak, breakdown and closing duties

### v0.13 — Show authoring

- Project-level show files and venue profiles
- Editable world affinities, act states and transition grammar
- External show-control API while keeping Smart Director fully capable in automatic operation

### v1.0 — Production reliability

- In-app GPU/audio diagnostics and render-budget telemetry
- Device-loss recovery and actionable crash reports
- Signed Windows installer and update channel
- Automated reference-frame, performance, protocol and long-running Console regression tests

Completed milestones remain documented in Git history and tagged releases, including v0.10.1 Stagecraft Console and v0.10.2 Stage Systems.

## Repository layout

```text
assets/rare-logo.png            Alpha Rare mark used by the world renderer
src/main.cpp                    Win32 app, audio engine, director, HLSL and renderer
src/console.cpp                 Tabbed native operator Console
src/rarestage_control.h         Versioned command and telemetry protocol
tests/control_protocol_test.cpp Command packing and shared-state round-trip test
.github/workflows/              Windows build, test, package and release automation
docs/                           Stage-production research and system design notes
CMakeLists.txt                  Native build and Spout2 integration
build-release.ps1               Local one-command Release build
THIRD_PARTY_NOTICES.md          Third-party licensing
```

RareStage includes [Spout2](https://github.com/leadedge/Spout2) under the BSD 2-Clause License. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

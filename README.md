# RareStage

**A native Windows festival-visual engine that turns the selected playback output into a persistent, evolving Rare-themed world in real time.**

[![Windows Build](https://github.com/rare6media/RareStage/actions/workflows/windows-build.yml/badge.svg?branch=main)](https://github.com/rare6media/RareStage/actions/workflows/windows-build.yml)

[Download the latest Windows build](https://github.com/rare6media/RareStage/releases/latest) · [View Windows builds](https://github.com/rare6media/RareStage/actions) · Current source version: **0.19.0**

RareStage is a native C++20, Win32, DirectX 11 and WASAPI application. It is not a website, Electron shell, video player, or collection of prerecorded VJ loops. Every frame is produced on the GPU from live audio analysis, authored world logic, the Rare mark, and procedural 2D/2.5D stage systems.

## What changed in 0.19.0

Version 0.19.0 is **Sculptures + Show Direction**.

- **21 scenes, preserving all existing worlds.** Ribbon Foundry (`Z`) weaves broad luminous strips; Halo Engine (`X`) counter-rotates solid rings into iris and petal formations; Vector Speedway (`J`) travels through a perspective track and arch rig. Each has an authored motif bank and continuous musical motion.
- **Smoother Pulse Causeway.** Depth layers stay sorted, recycled tiers fade through their end planes, and geometry follows the continuous musical clock. Kick/snare accents illuminate the structure with restrained brightness instead of moving the banks.
- **Scene-aware handoffs.** Surface worlds keep their shared moving lattice. Ribbon, perspective, radial and atmospheric scenes use their own deformation directions and aligned composition anchors. Non-wall pairs no longer turn into an unrelated cube carrier.
- **A Director with route memory.** It compares a next scene and a compatible follow-up, tracks recent visits and category repetition, reserves peak energy during builds, and plans from sustained musical energy. An eight-beat rehearsal lock prevents isolated hits from replacing a prepared route; a confirmed chapter or category change can invalidate it.
- **Pattern development.** Modern scenes establish a motif, repeat it, answer it and develop it. Per-scene motif history favors underused, section-appropriate entry patterns. The motif program stays fixed for a visit, including across hits and identity reveals.
- Timed scene changes start on the next eight-beat motif boundary. Qualified chapter cues retain their quicker musical response. `A` controls automatic cycling and `Space` holds it; manual scene picks preserve the automatic setting.
- Eight palettes, Alt+Enter/F11 fullscreen, synchronized display mirrors and the Pixel Bank drop-to-corner choreography remain available.

Use the matching Stage and Console from this package: Console protocol **v15**, network packet **v3**. Network peers must also use the matching version. Local display mirrors still receive the same rendered frame.

See [validation, performance and design notes](docs/SCULPTURES_SHOW_DIRECTION.md). GPU throughput is measured separately from physical display synchronization; 4K/120 capability depends on the machine and output setup.

## Engine at a glance

| Area | Current implementation |
|---|---|
| Platform | Native Windows x64, C++20, Win32 and DirectX 11 |
| Audio | WASAPI loopback from the selected active Windows playback device |
| Analysis | Sample-rate-aware 2048-point FFT with 50% overlap, adaptive band-limited spectral flux, sustained bass, kick, snare, hats, mids, highs, RMS, robust recent-interval BPM and tempo confidence |
| Song structure | Hysteresis-based Open, Build, Drop, Breakdown and Drive detection plus a sustained musical-ending confidence state |
| Visual system | Twenty-one authored Rare worlds organized into four VDJ categories, with 64/96-beat loop grammar, act-specific 2D animation, layered parallax and selective 2.5D depth |
| Director | Musical-stage prediction, prepared-world pre-roll, internal-loop progression, category filtering, 128/130 club anchors, ending holds, set-wide memory, adaptive cue density, palette morphing and beat-aligned chapter changes |
| Renderer | Half-float HDR scene pass, half-resolution bloom, sharpened filmic composite |
| Show control | Seven-page RareStage Console with editable DJ identity, category and tempo controls, plus a Direct2D/DirectWrite in-renderer overlay |
| Rehearsal | Telemetry recorder, GPU-aware smart captures, review decisions, reason tags and portable CSV/Markdown session evidence |
| Output | Borderless stage display, multi-monitor selection, clean Spout2 sender, four output masters and four mapped compositions |
| Screens | One main Stage window plus clean mirrors on up to seven other displays; aspect-preserving output and direct display selection |
| Synchronization | Optional UDP leader/follower show clock with bounded fixed-layout packets and motion-preserving follower hold |
| Performance | 62%, 78%, 100% and up to 125% HDR render scales (Ultra capped at 4K); VSync or unlocked output; asynchronous GPU timing and profile budgets |
| Persistence | Settings and four show presets under `%LOCALAPPDATA%\RareStage` |

## Rare worlds

| Key | World | Visual direction | Rare-mark role |
|---|---|---|---|
| `1` | Morph Wall | Broad glass panels carry flowing diagonals, waves and counterflow | Fine panel surfaces resolve into the detailed Rare mark or DJ name |
| `2` | Prism Atrium | Travelling architectural ribs reshape through prism sections and sixteen light formations | Identity assembles as light within the atrium |
| `3` | Orbital Garden | A breathing celestial landscape layers a central planet, eight orbiting bodies, atmospheric bands and sparse stars | Identity enters as distant architectural light rather than a foreground watermark |
| `4` | Stagewall | Gliding cube rows join into clean ribbons, chevrons, radial waves and thirteen further motifs | Fine surface pixels align into readable identity while the full wall stays present |
| `5` | Wavewall | Articulated vertical slats perform columns, diagonals, gates, tiers, fans and pressure fronts over a restrained stage floor | No foreground watermark; the complete slat stage carries the Rare movement language |
| `6` | Pixel Bank | Stable grouped pixels slowly rotate in and out as an abstract support loop | One polished DVD-style mark conducts wall impacts, corner bursts and reactive edge light |
| `7` | Prism Cascade | Two continuously moving prism banks perform columns, counters, gates and diagonals with signal-separated lighting | One stable mark sits behind the shutter architecture without slicing or glitch displacement |
| `8` | Aurora Range | Four moving aurora curtains, distant stars and terrain | Identity is carried by the atmospheric light field |
| `9` | Chroma Blades | Elongated articulated glass blades perform columns, diagonals, radial pushes, gates, fans, blocks, cascades and crosses | No logo; motion, color and stage framing define the world |
| `Q` | Kinetic Array | Turning tile banks develop waves, diagonals, blocks and counterflow | Tiles align their illuminated surfaces for a detailed identity resolve |
| `W` | Kinetic Canopy | Suspended ceiling panels perform rows, columns, diagonals, split gates, checks, fans and lifts above a perspective runway | No logo; canopy architecture and converging floor light define the identity |
| `E` | Pixel Loom | A dense 22×15 pixel instrument with sixteen continuously blended patterns | Fine luminous cells resolve the mark or DJ name |
| `R` | Matrix Vault | Receding architectural frames move steadily in one direction behind grouped data panels, etched glyph light and corridor pressure | The mark appears as temporary negative space inside the vault, never as a foreground watermark |
| `Y` | Panel Cascade | Panel banks fold and descend through row waves, diagonals and counterflow | Micro-surfaces roll into a readable identity and back to the cascade |
| `U` | Signal Rail | Nine perspective rails carry synchronized light packets from a distant horizon through six phrase formations | No logo; transport motion, lane color and architectural convergence make the world distinct |
| `I` | Kinetic Facade | A wide articulated elevation with sixteen flowing pattern families | Distributed surface light forms the identity while retaining the facade |
| `O` | Pulse Causeway | Perspective cube tiers, mirrored ceiling banks and light rails move through sixteen motifs | Identity assembles within the architectural space |
| `K` | Void Gates | Seven slow receding gates frame side-panel cues and a deliberately large central negative space | A restrained Rare-shaped void appears only during selected acts |
| `Z` | Ribbon Foundry | Broad ribbons weave, fan, braid and join through authored motif programs | The ribbon membrane carries the identity |
| `X` | Halo Engine | Counter-rotating solid rings develop iris, petal and orbital formations | Radial material illuminates the identity surface |
| `J` | Vector Speedway | A travelling track and arch rig develop lane chases and perspective rhythms | Identity is revealed within the track composition |

Every world contains five acts. An Open establishes form; Build assembles and adds directional movement; Drop sharpens hierarchy and adds controlled impact; Breakdown opens negative space and atmosphere; Drive sustains kinetic motion without repeatedly shaking or spinning a model.

## Smart Director

Smart Director maintains short- and long-window energy models and confirms musical stages with hysteresis. Its prediction layer can arm an approaching Build, Drop, Breakdown or Drive, rehearse a scene route, then commit only when the musical evidence confirms the boundary. A section change transforms the **current world first**. The dwell timer independently schedules the next scene on an eight-beat motif boundary, so the show progresses even without a newly detected section. The existing continuous scene-surface animation carries each change.

World selection scores current energy, band balance and the predicted musical act, penalizes the active and recently used worlds, then also accounts for how often each world has already served that act during the set. An optional category filter narrows automatic choices without disabling manual access. Palette targets change with confirmed sections and scheduled scene cues, and interpolate continuously. If tempo confidence disappears, `AUTO CLUB` returns gradually to its 128 BPM anchor; confident measurements near 128 or 130 receive a stable club-grid bias while clearly different tempos remain respected. Deterministic Show Timeline mode remains available for rehearsed playback.

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
| Bass | Sustained structural weight, membrane/floor emphasis, cube lift and broad light pressure |
| Kick | Near-real-time panel depth, controlled composition pump, radial pressure and short lighting hits |
| Snare | Coordinated row and face-light accents that read as one deliberate structural hit without snapping object rotation |
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
| Show | All twenty-one worlds, five act cues, previous/next, Smart Director, automation hold, Clean Open, stage HUD, load/save presets, blackout, and safe fallback |
| Mix + Director | Reaction profile, palette direction, sensitivity, bass, mids, highs, beat, logo scale, act override, automatic act resume, Smart Director, and timeline mode |
| Audio Lab | Auto/EDM/Bass Music/Melodic detector profile, instrument evidence, accepted/rejected diagnostics, adaptive calibration, sample labeling, replay-ready CSV export, and latency |
| Output | World dwell, scene motion, render profile, live GPU budget, clean PNG capture, VSync/high-refresh output, Spout, fullscreen, HUD, device, monitor, timeline mode, and renderer shutdown |
| Production | Production-duty presets, output mastering, master brightness, mapped stage layouts, sync group and Local/Leader/Follower synchronization |
| Screens | Main display selection and synchronized clean-frame mirrors on up to eight displays |
| Rehearsal | Start/stop set recording, GPU-aware smart captures, cue-density master, capture review, approve/rework tags, event timeline, and session export |

Protocol v15 uses a local shared-memory telemetry snapshot, acknowledged numeric commands and one bounded `WM_COPYDATA` message for the DJ name. The renderer remains authoritative, publishes a heartbeat at up to 40 Hz, rejects malformed input, and reports complete signal, detector evidence, musical narrative, world movement/pattern/loop, Showbrain choreography, identity target, category, tempo anchor, Director decisions, GPU timing, output mastering, synchronization, rehearsal, capture and renderer state back to Console. Engine and Console must come from the same release package.

Press `G` while Console has focus to start or stop a rehearsal. Smart captures are delayed until the important cue has visually settled and GPU utilization is below the preferred threshold, with a bounded deadline so evidence is never lost. Recording never stalls or controls the renderer's animation clock.

To replay an exported, operator-labeled trace without launching the UI, run `RareStageConsole.exe --trace-replay <trace.csv>`. A sibling `.analysis.txt` file records the four-class confusion matrix, labeled-sample count and accuracy. RareStage never presents the bundled synthetic contract fixture as real performance calibration.

## Show Control

Press `Tab` to show or hide the in-compositor control panel.

| Overlay row | Values or range |
|---|---|
| Reaction | Calm, Balanced, Stage, Aggressive |
| Palette | Rare Cyan, Nebula, Sakura, Solar, Laser Lime, Coral Ice, Ultraviolet, Electric Gold |
| Sensitivity | 50–150% |
| Bass / Mids / Highs / Beat | 50–150% each |
| Logo | 75–125% |
| Cue density | 65–125% operator master over authored world/act density |
| World dwell | 40–120 seconds |
| Transition | 0.2–3.0 seconds |
| Transition FX | Row Flow, Ribbon Flow, Orbit Flow, Depth Fold |
| Render profile | Performance, Balanced, Native HD, Ultra 4K |
| Frame output | Display/VSync or unlocked high refresh |
| Show mode | Smart Director or Show Timeline |
| Spout output | Clean sender on or off |

The in-renderer panel and Console manipulate the same settings. Both report the current world, act, automation state, preset, automatic palette, playback device, BPM, and live frequency/transient meters.

## Controls

| Input | Action |
|---|---|
| `1`–`9` | Select worlds 1–9; preserve automatic mode |
| `Q` / `W` / `E` | Select Kinetic Array, Kinetic Canopy or Pixel Loom |
| `R` / `Y` / `U` | Select Matrix Vault, Panel Cascade or Signal Rail |
| `I` / `O` / `K` | Select Kinetic Facade, Pulse Causeway or Void Gates |
| `Z` / `X` / `J` | Select Ribbon Foundry, Halo Engine or Vector Speedway |
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

When Console has focus, `1`–`9`, `Q`, `W`, `E`, `R`, `Y`, `U`, `I`, `O`, `K`, `Z`, `X`, and `J` select all twenty-one worlds. `A`, `Space`, `B`, `Ctrl+O`, `F`, `H`, `D`, `M`, `T`, `F1`–`F4`, and `Shift+F1`–`Shift+F4` mirror the primary show cues. `G` starts or stops a rehearsal recording.

## Factory show presets

Factory slots provide a calm opening, balanced club show, stage peak and aggressive timeline. Each stores its reaction tuning, starting world, palette and motion settings. Smart Director uses bar-length scene animation; manual changes use the saved motion duration.

Saved presets include the current world, reaction tuning, fallback palette, dwell, transition, render profile, output mode, automation, timeline, Spout, monitor, production duty, mastering, mapped layout, master brightness, sync mode and sync group.

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
- Local UDP broadcast permission only when Leader or Follower synchronization is deliberately enabled

## Performance profiles

| Profile | HDR scene scale | Intended use |
|---|---:|---|
| Performance | 62% | Lower-power GPUs or high-refresh output |
| Balanced | 78% | Reduced GPU load with native-resolution final composite |
| Native HD | 100% | Full native scene rendering |
| Ultra 4K | Up to 125%, capped at 3840×2160 | Native 4K scene pass on a 4K display; 8 ms GPU target |

VSync is enabled by default. The 0.16.0 renderer uses bounded loops, branch-local world logic and procedural 2D/2.5D layers. Only the selected world executes. Output mastering and mapped-panel masks run in the final composite pass. Direct3D timestamp queries measure the GPU work asynchronously; they do not stall the stage waiting for timing results.

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
4. Run deterministic kick, snare, hi-hat and sustained-bass separation tests across four detector profiles and three sample rates.
5. Verify liquid monotonic motion at 60, 120 and 144 Hz and replay the labeled-trace CSV contract.
6. Validate all 80 production duty/master/layout combinations and the synchronization packet contract.
7. Compile-test every embedded DirectX shader.
8. Launch-test both native executables with the alpha logo present.
9. Run the Console-owned Stage launch soak regression.
10. Render all 72 world/profile reference PNGs and validate their GPU-budget manifest.
11. Verify publisher/version metadata, optionally sign, then package binaries, checksums and provenance.

Successful pushes to `main` publish or refresh the **v0.16.0** GitHub release automatically. See [SECURITY.md](SECURITY.md) for verification and antivirus-report guidance.

## Roadmap

### v0.10.6 — Field Calibration + Scene Mastering (completed)

- Winner-arbitrated kick/snare/hat classification, a separate sustained-bass envelope and adaptive device calibration
- Protocol v6 Audio Lab evidence, ambiguity, threshold, latency and accepted/rejected trace telemetry
- Liquid 8/16-beat formations with transient lighting separated from object movement
- Predictable Matrix Vault depth travel and scene-specific mastering across all eighteen worlds
- High-refresh motion traces plus an 18-world/5-act/4-profile mastering reference matrix

### v0.10.7 — Operator Capture + GPU Budgets (completed)

- Native asynchronous D3D11 timestamp budgets surfaced in Console
- Clean operator PNG capture controlled from Console
- Deterministic 72-frame world/profile review artifact and budget manifest
- Labelable Audio Lab traces with replay/confusion-matrix analysis; real-set thresholds remain data-gated

### v0.10.8 — Set Review + Cue Density (completed)

- Fifth Console page for full-set telemetry recording and a live cue-event timeline
- GPU-aware smart captures after world/act changes and performance-budget excursions
- Capture approve/rework decisions plus concise visual reason tags
- Timeline CSV, review CSV and Markdown session summaries for production review
- Authored world/act cue-density mastering with a 65–125% operator control
- Continuous motion protected from density and capture automation

### v0.11.0 — Smart Director 2 (completed)

- Predictive musical-stage pre-roll with prepared world and palette selection
- Confirmed-hit commits protected by section hysteresis, dwell and ending holds
- Act-aware set memory that avoids repeating the same world/act roles
- Confidence-aware tempo hold and gradual fallback for uninterrupted motion
- Adaptive cue density layered over operator and authored world/act mastering
- Live and exported Console evidence for accepted, prepared and rejected Director decisions
- Detector thresholds remain reserved for labeled real-set rehearsal traces

### v0.12.0 — Stage Production Pack (completed)

- Studio, LED Safe, Arena and Broadcast output mastering with bounded master brightness
- Full Frame, Ultrawide, Triptych and LED Facade mapped-panel compositions
- Open, Support, Peak, Breakdown and Closing production-duty masters
- Optional local-network Leader/Follower show clock with safe follower hold
- Clean-output adapter boundary preserved for a future optional NDI transport
- Production controls and evidence exported through Console protocol v10

### v0.13.0 — Adaptive Show Grammar (completed)

- Context-aware build/drop/breakdown estimation based on raw calibrated features
- Five world movements and deterministic shuffled pattern variation per visit
- Coherent family-aware transition grammar and prepared-cue validation
- Stable sample and visual clocks with stale-audio/device-loss protection
- Console protocol v11 narrative, movement, pattern and transition telemetry

### v0.14.0 — Club Loop Engine (completed)

- Four category-filtered VDJ content packs across all 18 worlds
- 64/96-beat internal loops that progress before cross-world changes
- Confident automatic 128/130 club-tempo anchoring with monotonic phase slew
- Custom DJ identity choreography in the clean output
- Protocol v12 Console control and deterministic loop validation

### v0.14.1 — Identity Worlds (completed)

- Tile-fragment DJ-name assembly and transition choreography
- Family-specific carrier, architecture and environmental identity treatments
- Lumen Canyon, Orbital Garden and Rare Archipelago world-scale scenes
- Current-frame identity accents with continuous autonomous motion underneath

### v0.15.0 — Choreography Showbrain

- GPU-native Rare-mark and custom DJ-name formation masks
- Establish, Roll, Resolve and Release stanza grammar
- Panel Cascade motion language propagated through the primary panel systems
- World-integrated particle, star and atmospheric identity reveals
- Protocol v13 live choreography telemetry in Console

### Future — Show authoring

- Project-level show files and venue profiles
- Editable world affinities, act states and transition grammar
- External show-control API while keeping Smart Director fully capable in automatic operation
- Rehearsal-driven detector tuning once representative real-set labels exist
- Optional NDI transport adapter after SDK/licensing and deployment validation

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

### v0.17.0 — Continuous Choreography

Moving Stagewall rows, clean connected cube formations, shared animated scene surfaces, bar-length Director motion, Prism Atrium and Pulse Causeway. Includes the v0.16 detailed logo/name surfaces and full-wall coverage fixes. See the current validation notes; a local build does not imply a published GitHub release.

### v0.18.0 — Stage Lighting + Multi-Screen

Sixteen wall/architectural motifs, eight palettes, timed bar-aligned Director cycles, forecast DVD corner arrivals, repaired fullscreen shortcuts and clean multi-screen mirrors. Current validation covers the renderer, choreography, timed cues, window behavior, mirror readback and Screens controls.

### v0.19.0 — Sculptures + Show Direction (current source)

Three additional worlds, sorted continuous Causeway motion, scene-aware material handoffs, rehearsed next/follow-up routes and remembered motif programs. See [validation and design notes](docs/SCULPTURES_SHOW_DIRECTION.md).

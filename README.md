# RareStage

**A native Windows festival-visual engine that turns the selected playback output into a persistent, evolving Rare-themed world in real time.**

[![Windows Build](https://github.com/rare6media/RareStage/actions/workflows/windows-build.yml/badge.svg?branch=main)](https://github.com/rare6media/RareStage/actions/workflows/windows-build.yml)

[Download the latest Windows build](https://github.com/rare6media/RareStage/releases/latest) · [View Windows builds](https://github.com/rare6media/RareStage/actions) · Current version: **0.14.0**

RareStage is a native C++20, Win32, DirectX 11 and WASAPI application. It is not a website, Electron shell, video player, or collection of prerecorded VJ loops. Every frame is produced on the GPU from live audio analysis, authored world logic, the Rare mark, and procedural 2D/2.5D stage systems.

## What changed in 0.14.0

Version 0.14.0 is the **Club Loop Engine**. RareStage now treats each world as a reusable VDJ content pack: continuous autonomous motion, tempo-locked internal movements, category-aware programming, and authored DJ identity choreography suitable for club screens, mapped LED stages, Spout capture and VRChat video-routing workflows.

- Four scene categories: Panel / Pixel, Ribbon / Glass, Depth / Transport and Atmosphere
- Console-selectable category filtering for Smart Director while manual world selection remains unrestricted
- `AUTO CLUB` starts on a confident 128 BPM grid, recognizes the common 128/130 neighborhood quickly and follows clearly different detected tempos
- Explicit 128 BPM and 130 BPM anchors for rehearsed club material
- Every world develops through 64- or 96-beat content loops with five movements and sixteen pattern banks
- When a world reaches its dwell target outside a genuine musical chapter, Smart Director changes its internal movement first instead of channel-surfing
- Custom DJ name editing in the Console Production page
- DJ identity is rendered into clean stage output as an intermittent assembly, carrier, architectural crop or sparse reveal—not a permanent watermark
- Protocol v12 exposes category, effective tempo grid, loop phase/length and DJ identity telemetry
- A deterministic Club Loop contract validates all 18 worlds across both common club-tempo anchors

The v0.13 Adaptive Show Grammar remains the foundation:

- Raw calibrated audio now drives song structure; visual gain controls cannot change the detected act
- Contextual build tension and impact contrast prevent ordinary first kicks from becoming false drops
- Independent kick, snare and hi-hat lanes can illuminate together instead of winner-take-all muting
- Sample-clock BPM estimation is stable across capture batching and CPU stalls
- Stale audio decays safely after endpoint loss instead of leaving the show permanently “hot”
- Every world receives a seeded 16-beat movement and pattern lifecycle, so repeat visits stay recognizable but vary
- Prepared drop cues expire and cannot commit after the predicted musical section has changed
- Monotonic choreography timing removes the former 1024-beat reset and absolute-time confidence jumps
- Console protocol v11 exposes movement, pattern, build tension, drop readiness, novelty and transition fit

- Four post-bloom mastering profiles: Studio, LED Safe, Arena and Broadcast
- Adjustable 65–100% master brightness with LED-safe highlight roll-off and broadcast-range restraint
- Four mapped compositions: Full Frame, 2.39:1 Ultrawide, Triptych and LED Facade
- Five one-touch production duties for Open, Support, Peak, Breakdown and Closing portions of a set
- Local-network Leader/Follower sync broadcasts world, act, palette, tempo phase, cue density and output treatment on UDP 47712, with Console-selectable groups to isolate adjacent shows
- Followers preserve their last state and continuous motion if the leader disappears; Local mode opens no network socket and remains the default
- A sixth glassmorphic Console page controls every production-pack function and reports packet age, phase error and sync state
- Protocol v11 adds musical-narrative and world-progression telemetry to the existing production evidence
- The clean-output boundary remains isolated from Console/HUD and is ready for a future NDI transport adapter; this release does not claim a bundled NDI sender
- A deterministic 80-case production contract validates all duty, mastering and layout combinations plus the fixed-size sync packet

The research and design synthesis behind this release is documented in [Stage visual research, 2023–2026](docs/STAGE_VISUAL_RESEARCH_2023_2026.md).

## Engine at a glance

| Area | Current implementation |
|---|---|
| Platform | Native Windows x64, C++20, Win32 and DirectX 11 |
| Audio | WASAPI loopback from the selected active Windows playback device |
| Analysis | Sample-rate-aware 2048-point FFT with 50% overlap, adaptive band-limited spectral flux, sustained bass, kick, snare, hats, mids, highs, RMS, robust recent-interval BPM and tempo confidence |
| Song structure | Hysteresis-based Open, Build, Drop, Breakdown and Drive detection plus a sustained musical-ending confidence state |
| Visual system | Eighteen authored Rare worlds organized into four VDJ categories, with 64/96-beat loop grammar, act-specific 2D animation, layered parallax and selective 2.5D depth |
| Director | Musical-stage prediction, prepared-world pre-roll, internal-loop progression, category filtering, 128/130 club anchors, ending holds, set-wide memory, adaptive cue density, palette morphing and beat-aligned chapter changes |
| Renderer | Half-float HDR scene pass, half-resolution bloom, sharpened filmic composite |
| Show control | Six-page RareStage Console with editable DJ identity, category and tempo controls, plus a Direct2D/DirectWrite in-renderer overlay |
| Rehearsal | Telemetry recorder, GPU-aware smart captures, review decisions, reason tags and portable CSV/Markdown session evidence |
| Output | Borderless stage display, multi-monitor selection, clean Spout2 sender, four output masters and four mapped compositions |
| Synchronization | Optional UDP leader/follower show clock with bounded fixed-layout packets and motion-preserving follower hold |
| Performance | 62%, 78%, 100% and 125% HDR render scales; VSync or unlocked output; asynchronous GPU timing and profile budgets |
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
| `I` | Kinetic Facade | A wide articulated-panel elevation performs columns, diagonals, radial pushes, split banks, blocks, fans and cascades | The mark appears only as a distributed negative-space facade formation |
| `O` | Suspended Field | Suspended tile banks, cables and runway light perform seven ceiling-scale formations with calm physical drift | No foreground mark; the suspended architecture is the identity |
| `K` | Void Gates | Seven slow receding gates frame side-panel cues and a deliberately large central negative space | A restrained Rare-shaped void appears only during selected acts |

Every world contains five acts. An Open establishes form; Build assembles and adds directional movement; Drop sharpens hierarchy and adds controlled impact; Breakdown opens negative space and atmosphere; Drive sustains kinetic motion without repeatedly shaking or spinning a model.

## Smart Director

Smart Director maintains short- and long-window energy models and confirms musical stages with hysteresis. Its prediction layer can arm an approaching Build, Drop, Breakdown or Drive, prepare the next world and palette, then commit only when the musical evidence confirms the boundary. A section change still transforms the **current world first**. If dwell expires outside a genuine chapter boundary, the current world receives another internal movement and pattern phrase rather than an arbitrary scene cut.

World selection scores current energy, band balance and the predicted musical act, penalizes the active and recently used worlds, then also accounts for how often each world has already served that act during the set. An optional category filter narrows automatic choices without disabling manual access. Palettes begin morphing during a qualified pre-roll rather than cutting at the drop. If tempo confidence disappears, `AUTO CLUB` returns gradually to its 128 BPM anchor; confident measurements near 128 or 130 receive a stable club-grid bias while clearly different tempos remain respected. Deterministic Show Timeline mode remains available for rehearsed playback.

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
| Show | All eighteen worlds, five act cues, previous/next, Smart Director, automation hold, Clean Open, stage HUD, load/save presets, blackout, and safe fallback |
| Mix + Director | Reaction profile, palette direction, sensitivity, bass, mids, highs, beat, logo scale, act override, automatic act resume, Smart Director, and timeline mode |
| Audio Lab | Auto/EDM/Bass Music/Melodic detector profile, instrument evidence, accepted/rejected diagnostics, adaptive calibration, sample labeling, replay-ready CSV export, and latency |
| Output | World dwell, transition language, render profile, live GPU budget, clean PNG capture, VSync/high-refresh output, Spout, fullscreen, HUD, device, monitor, timeline mode, and renderer shutdown |
| Production | Production-duty presets, output mastering, master brightness, mapped stage layouts, sync group and Local/Leader/Follower synchronization |
| Rehearsal | Start/stop set recording, GPU-aware smart captures, cue-density master, capture review, approve/rework tags, event timeline, and session export |

Protocol v12 uses a local shared-memory telemetry snapshot, acknowledged numeric commands and one bounded `WM_COPYDATA` message for the DJ name. The renderer remains authoritative, publishes a heartbeat at up to 40 Hz, rejects malformed input, and reports complete signal, detector evidence, musical narrative, world movement/pattern/loop, category, tempo anchor, Director decisions, GPU timing, output mastering, synchronization, rehearsal, capture and renderer state back to Console. Engine and Console must come from the same release package.

Press `G` while Console has focus to start or stop a rehearsal. Smart captures are delayed until the important cue has visually settled and GPU utilization is below the preferred threshold, with a bounded deadline so evidence is never lost. Recording never stalls or controls the renderer's animation clock.

To replay an exported, operator-labeled trace without launching the UI, run `RareStageConsole.exe --trace-replay <trace.csv>`. A sibling `.analysis.txt` file records the four-class confusion matrix, labeled-sample count and accuracy. RareStage never presents the bundled synthetic contract fixture as real performance calibration.

## Show Control

Press `Tab` to show or hide the in-compositor control panel.

| Overlay row | Values or range |
|---|---|
| Reaction | Calm, Balanced, Stage, Aggressive |
| Palette | Rare Cyan, Nebula, Sakura, Solar; automatically morphed by Smart Director |
| Sensitivity | 50–150% |
| Bass / Mids / Highs / Beat | 50–150% each |
| Logo | 75–125% |
| Cue density | 65–125% operator master over authored world/act density |
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
| `I` / `O` / `K` | Select Kinetic Facade, Suspended Field or Void Gates |
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

When Console has focus, `1`–`9`, `Q`, `W`, `E`, `R`, `Y`, `U`, `I`, `O`, and `K` select all eighteen worlds. `A`, `Space`, `B`, `Ctrl+O`, `F`, `H`, `D`, `M`, `T`, `F1`–`F4`, and `Shift+F1`–`Shift+F4` mirror the primary show cues. `G` starts or stops a rehearsal recording.

## Factory show presets

| Slot | Direction | Starting world | Default dwell / transition |
|---|---|---|---|
| `F1` Clean Open | Calm, Rare Cyan | Celestial Rig | 88 sec / 1.8 sec Horizon Rise |
| `F2` Melodic Bloom | Balanced, Sakura | Morph Wall | 72 sec / 1.4 sec Signal Veil |
| `F3` Rare Drop | Stage, Rare Cyan | Bioframe | 56 sec / 1.2 sec Gate Open |
| `F4` Glitch Finale | Aggressive, Nebula, timeline | Stagewall | 48 sec / 0.9 sec Signal Veil |

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
| Ultra 4K | 125% | Supersampled output on high-end GPUs |

VSync is enabled by default. The 0.14.0 renderer uses bounded loops, branch-local world logic and procedural 2D/2.5D layers. Only the selected world executes. Output mastering and mapped-panel masks run in the final composite pass. Direct3D timestamp queries measure the GPU work asynchronously; they do not stall the stage waiting for timing results.

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

Successful pushes to `main` publish or refresh the **v0.14.0** GitHub release automatically. See [SECURITY.md](SECURITY.md) for verification and antivirus-report guidance.

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

### v0.14.0 — Club Loop Engine (current)

- Four category-filtered VDJ content packs across all 18 worlds
- 64/96-beat internal loops that progress before cross-world changes
- Confident automatic 128/130 club-tempo anchoring with monotonic phase slew
- Custom DJ identity choreography in the clean output
- Protocol v12 Console control and deterministic loop validation

### v0.15 — Show authoring

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

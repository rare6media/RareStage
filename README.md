# RareStage

**A native Windows festival-visual engine that turns the selected playback output into a persistent, evolving Rare-themed world in real time.**

[![Windows Build](https://github.com/rare6media/RareStage/actions/workflows/windows-build.yml/badge.svg?branch=main)](https://github.com/rare6media/RareStage/actions/workflows/windows-build.yml)

[Download the latest Windows build](https://github.com/rare6media/RareStage/releases/latest) · [View Windows builds](https://github.com/rare6media/RareStage/actions) · Current source version: **0.27.0**

RareStage is a native C++20, Win32, DirectX 11 and WASAPI application. It is not a website, Electron shell, video player, or collection of prerecorded VJ loops. Every frame is produced on the GPU from live audio analysis, authored world logic, the Rare mark, and procedural 2D/2.5D stage systems.

## What changed in 0.27.0

**Phrase-Aware Director** lets a program finish its formation before a beat-aligned
departure. It prepares two candidate handovers through the production renderer
at low resolution, when measured GPU capacity permits. Completed image checks
can influence the continuation and supply its entry program. Holds, changed
context and GPU pressure cancel or skip background work.

The Console shows the current motion intention and departure countdown. Music-fit
scoring now includes the four Motion Worlds. This is deterministic, local planning
with bounded composition heuristics; no model inference or runtime token use.

Includes all v0.26 scenes and programs. Use matching Stage and Console protocol 22;
network protocol 9 remains unchanged. See [behavior, validation and limitations](docs/PHRASE_AWARE_DIRECTOR.md).
Actual-song comparison and complete multi-screen 4K/120 qualification remain open.

## What changed in 0.26.0

**Motion Worlds** adds Optical Weave, Iris Assembly, Shape Exchange and Orbit Press,
with ten geometric programs each. Bands exchange with solid shapes; curved blades
reform into nested arcs on four authored routes with persistent piece correspondence.
Graphic-only draws use a dedicated shader and new poses are prepared once per frame.

The catalog now contains 42 scenes (35 auto eligible), 228 named programs and 244
previews. Use matching Console protocol 21 and network protocol 9. See
[Motion Worlds](docs/MOTION_WORLDS.md) for behavior, validation commands and limits.
Full-song listening and complete 4K/120 output qualification remain open.

## What changed in 0.25.0

**Graphic Worlds** adds Contour Currents, Ribbon Braid and Radial Fans: three filled 2D visual languages with ten named programs each. Contours compress and redirect, ribbons weave and exchange, and broad fans counter-rotate and open on the musical clock.

- Thirty new geometry programs, coordinated palette colors and continuous within-scene development.
- Travelling folds preserve readable compositions during graphic scene handovers. Rare/name reveals inherit moving band colors while the surrounding world continues.
- The seven rejected scenes (IDs 28–34) remain manually selectable and are excluded from automatic routes and timelines.
- The catalog contains 38 scenes, 188 named programs and 204 rendered previews; 31 scenes are eligible for automatic selection. The 100+ scene goal remains open.
- Matching Console protocol 20 and UDP protocol 8. Existing shortcuts, fullscreen and synchronized local mirrors are preserved.

See [programs, handoff behavior and validation](docs/GRAPHIC_WORLDS.md). Full-song listening review and complete 4K/120 output qualification remain open.

## What changed in 0.24.0

**Choreography Sequences** connects visual motifs across scene visits and adds Gearworks, Switchyard and Origami Parade, with ten named programs each. The library now has 35 scenes and 158 selectable named programs; sixteen scenes also retain continuous authored choreography.

- The Director carries a movement motif through establish, carry, develop, payoff and release roles. Confirmed musical evidence and a shared cooldown protect major payoffs.
- Incoming object formations are chosen using sampled position, size and velocity, while shared wall-pattern seeds continue through a chapter.
- Gear shafts and drive links, closed conveyor circuits and folding paper formations use continuous musical motion. Percussion accents their surfaces.
- The new Catalog supports scene/program search, category filters, saved favorites, 174 rendered previews and explicit program selection with a hold/resume control.
- Matching executables use Console v19 and network v7. Existing scene IDs, shortcuts and synchronized local mirrors are preserved.

See [behavior, controls and validation](docs/CHOREOGRAPHY_SEQUENCES.md). Full-song listening review and complete 4K/120 output qualification remain open.
## What changed in 0.23.1

**Pattern Materials** carries the Stagewall and Morph Wall pattern vocabulary into six object scenes. The catalog remains at 32 scenes.

- The wall shader and object choreography now compile the same sixteen authored patterns: row waves, opposing groups, column travel, ripples, chevrons, radial sweeps and checker exchanges. Their phase and smooth motif changes follow the existing musical clock.
- Orbit Relay bends these patterns around its discs; Iris Bloom opens and turns its petals; Domino Circuit slides complete rows; Elastic Loom exchanges ribbon banks; Pendulum Choir coordinates swinging groups; Magnetic Assembly shifts and docks its plates.
- Each scene has a different stylized material response: satin discs, folded iridescent petals, enamel cards, directional metal ribbons, ceramic tokens and brushed plates. Surface properties interpolate with the pieces during native scene handoffs.
- The six scenes keep their ten base formation programs. Shared pattern motion and lighting add another layer of development. Stagewall, Morph Wall and Panel Cascade retain their existing geometry and shading.
- Pattern samples are prepared on the CPU with the object poses. The GPU receives twenty extra signal values, keeping the added per-pixel work small. Console v18 and UDP v6 formats remain compatible; use the matching bundle for consistent visuals.

See [pattern reuse, materials and validation](docs/PATTERN_MATERIALS.md). Complete 4K/120 output qualification and full-song listening review remain open.

## What changed in 0.23.0

**Kinetic Playgrounds** adds eight scenes with ten named movement programs each, bringing the catalog to 32 scenes. The focus is readable actions, coordinated motion and smoother scene handoffs.

- **Axis Puzzle and Sliding Shutters:** alternating row/column strokes, opposing banks, zipper moves, lane locks, stairs and cross shuttles. Color groups remain visible while the pieces move. Both join the shared wall geometry rig; Stagewall and Panel Cascade retain their established movement.
- **Orbit Relay, Iris Bloom, Domino Circuit, Elastic Loom, Pendulum Choir and Magnetic Assembly:** twenty persistent pieces become circulating tokens, opening petals, tipping cards, woven surfaces, swinging groups and docking formations. Their ten programs are named in [the scene guide](docs/KINETIC_PLAYGROUNDS.md).
- The six object scenes transform their pieces directly into each other. Together with the new wall bridges, the catalog now has **106 directed native geometry handoffs**. The other **886 ordered pairs** use category-aware material transport with deformation bounded to the frame edges.
- Automatic handoffs involving a new scene take eight beats. Other cross-family handoffs receive six beats during active sections; existing quiet-section and Blades/Canopy eight-beat behavior remains. Manual duration still follows the operator's setting.
- Object poses are calculated once per scene pass on the CPU and uploaded as a small pose array. The GPU shades the current pieces; it does not evaluate every movement program for every screen pixel.
- The Console fits all 32 scenes. New scenes are selected there or with Previous/Next; existing keyboard shortcuts remain. Matching Console protocol is **v18**, network protocol **v6**.

This update adds 80 authored programs to the eight new scenes; it does not claim ten programs in every older scene. The 100+ scene / 10+ pattern catalog target, stronger musical-evidence evaluation, live image rehearsal and 4K/120 qualification remain roadmap work. The runtime stays deterministic and offline.

## What changed in 0.22.0

**Phrase Conductor** gives eight non-wall ensembles musical programs that establish, hold, answer, develop and resolve.

- Prism Cascade, Chroma Blades, Kinetic Canopy, Matrix Vault, Halo Engine and the three ribbon worlds now receive deliberate movement choices from Smart Director. Each keeps six named programs with its own shapes and movement.
- A stable groove holds a recognizable formation before a related answer. Sustained builds and confirmed drops can develop it; breakdowns and approaching departures can resolve it. Decisions use eight-beat boundaries, and pose changes unfold over eight beats without restarting the musical clock.
- Program memory spaces recent formations and action combinations. Stronger drop articulation has a 96-beat cooldown shared across scene visits. Identity alternates Rare mark/name, waits for a settled scene, and is spaced by at least 64 beats across the set.
- Blades and Canopy share eighteen pieces that continuously change pose, taper and bevel. These two new directed bridges bring the total to **50 native handoffs**; the remaining 502 ordered pairs use category-aware material transport.
- Console shows the current named formation and musical intention. Local mirrors receive the same completed frame. Network v5 includes current/outgoing conductor samples and pattern seeds; peers must use the same version.

The scene count stays at 24. Stagewall retains its existing movement programs. Musical-corpus accuracy, live image evaluation and 4K/120 qualification are not claimed by this update. See [Phrase Conductor implementation and validation](docs/PHRASE_CONDUCTOR.md).

## What changed in 0.21.1

This visual revision develops eight non-wall scenes into fuller ensembles with different silhouettes.

- **Prism Cascade (7):** staggered streams of faceted prisms descend and rise through coordinated waves and rotations.
- **Chroma Blades (9):** broad opposing blade banks interlock, scissor, fan and sweep through six phrase programs.
- **Matrix Vault (R):** varied-size square frames form a linked rotating mechanism, including an independently turning inner square.
- **Kinetic Canopy (W):** two opposing banks of tapered, folded fins form arches, fans, braids and sweeping waves. Six continuous 16-beat pose programs replace the suspended bars.
- **Halo Engine (X):** three coordinated, segmented rotors replace thin nested rings. Broad faceted blades move in counter-rotation; narrow displays stack the supporting rotors vertically.
- **Ribbon Foundry (Z), Ribbon Racetrack (V), Kinetic Monument (N):** wider folded surfaces, interleaved travelling waves, stronger color separation and surrounding motion. Monument spreads its formation to fit wide screens. The shared rig continues to gather, turn and release during native handoffs.
- The new ensembles keep structural motion on the musical clock. Percussion lights their edges without moving the geometry.
- A dedicated composition check samples the eight scenes over 96 beats, four arrangements and landscape, portrait and wide displays. It measures foreground occupation above the dim background in every quadrant.

See [the non-wall visual revision](docs/ENSEMBLE_WORLDS.md). These changes develop existing worlds; the broader musical-judgment and Director roadmap remains in progress.

## What changed in 0.21.0

Version 0.21.0 is **Show Composer**, combining shared movement rules with conditional planning, visual themes and route cost estimates.

- **Three new scenes.** Folding Cathedral stretches the wall into folded architectural ribs. Ribbon Racetrack carries broad, sweeping lanes across the stage. Kinetic Monument raises and folds a bank of sculptural columns. All have four arrangements, sixteen motif seeds and the existing varied identity choreography.
- **Geometry carries across scenes.** The seven wall scenes share a continuous lattice bridge. Ribbon Foundry, Ribbon Racetrack and Kinetic Monument share a continuous ribbon rig. Those 48 directed pairings interpolate geometry in one scene pass. Other pairings retain the tested category-aware material handoff.
- **Three prepared continuations.** The Composer prepares continued movement, a drop and a breakdown together. Confirmed sections select the relevant branch. Plans expire after 32 beats and reject changed scene, category or stale section context. Manual cues and automation holds take precedence.
- **Six visual themes.** River, Crown, Orbit, Pulse, Ascent and Tidal carry a common deformation and accent bias across scenes. A theme develops over at least three scene visits. Theme memory spaces callbacks; changes start on a safe 16-beat boundary and unfold over eight beats. **Hold Theme** preserves it across automatic scene changes; **Next Theme** queues a new one.
- **Route rehearsal and GPU cost.** The Director compares nominal anchor and velocity envelopes and samples scene GPU cost asynchronously. Under budget pressure, measured expensive handoffs lose priority. This is an analytic estimate, not a live image-quality evaluator or a frame-rate guarantee.
- Stagewall, continuous musical motion, varied Rare/DJ identity entrances, smooth Pixel Bank departures, Alt+Enter/F11 fullscreen and synchronized local mirrors remain available.

Use the matching Stage and Console: Console protocol **v18**, network packet **v6**. Network packets carry the leader's theme and phrase state; network peers must match. Local mirrors still receive the same completed frame.

See [Show Composer implementation and validation](docs/SHOW_COMPOSER.md), [v0.20 scene improvements](docs/PERFORMANCE_WORLDS.md) and the [roadmap to v0.30](docs/ROADMAP_V0_30.md). Musical-corpus evaluation, runtime composition feedback and full 4K/120 qualification remain future work.

## Engine at a glance

| Area | Current implementation |
|---|---|
| Platform | Native Windows x64, C++20, Win32 and DirectX 11 |
| Audio | WASAPI loopback from the selected active Windows playback device |
| Analysis | Sample-rate-aware 2048-point FFT with 50% overlap, adaptive band-limited spectral flux, sustained bass, kick, snare, hats, mids, highs, RMS, robust recent-interval BPM and tempo confidence |
| Song structure | Hysteresis-based Open, Build, Drop, Breakdown and Drive detection plus a sustained musical-ending confidence state |
| Visual system | Thirty-two authored Rare worlds organized into four VDJ categories, with 64/96-beat loop grammar, act-specific 2D animation, layered parallax and selective 2.5D depth |
| Director | Conditional continued/drop/breakdown branches, six shared themes, analytic route rehearsal and measured GPU cost; musical-stage prediction, prepared-world pre-roll, internal-loop progression, category filtering, 128/130 club anchors, ending holds, set-wide memory, adaptive cue density, palette morphing and beat-aligned chapter changes |
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
| `5` | Resonance Range | Ten opaque contour terraces move through interlocking wave and relief patterns | Material fragments assemble into a readable mark or DJ name within the terrain |
| `6` | Pixel Bank | A circuit field and musical waves surround a continuously travelling mark | Smooth corner arrivals precede local scene departures; hits illuminate the background |
| `7` | Prism Cascade | Three staggered streams of faceted prisms travel in coordinated waves | Material fragments enter, resolve and disperse using independent choreography |
| `8` | Aurora Range | Five luminous curtains, a reflective basin, foreground ridges and stars | Identity is carried by the atmospheric light field |
| `9` | Chroma Blades | Broad opposing blade banks form scissors, zippers, fans and travelling sweeps | Choreographed material fragments form the mark or DJ name |
| `Q` | Kinetic Array | Turning tile banks develop waves, diagonals, blocks and counterflow | Tiles align their illuminated surfaces for a detailed identity resolve |
| `W` | Kinetic Canopy | Interlocking tapered fins form arches, braids, fans and opposing waves | Segmented identity enters and releases between the fin formations |
| `E` | Pixel Loom | A dense 22×15 pixel instrument with sixteen continuously blended patterns | Fine luminous cells resolve the mark or DJ name |
| `R` | Matrix Vault | Varied-size square frames and an inner square form a linked rotating mechanism | Segmented identity assembles and releases within the vault |
| `Y` | Panel Cascade | Panel banks fold and descend through row waves, diagonals and counterflow | Micro-surfaces roll into a readable identity and back to the cascade |
| `U` | Signal Rail | Nine solid perspective lanes carry repeating note sequences toward a moving cross-bank | Material identity appears as a planned formation within the lane composition |
| `I` | Kinetic Facade | A wide articulated elevation with sixteen flowing pattern families | Distributed surface light forms the identity while retaining the facade |
| `O` | Pulse Causeway | Perspective cube tiers, mirrored ceiling banks and light rails move through sixteen motifs | Identity assembles within the architectural space |
| `K` | Void Gates | A solid radial iris opens through six, eight, ten or twelve petal arrangements | Independent fragment entrances and releases form the identity at the iris |
| `Z` | Ribbon Foundry | Broad folded surfaces weave and overlap through travelling wave programs | The ribbon membrane carries the identity |
| `X` | Halo Engine | Three segmented rotors counter-rotate through coordinated fan formations | Radial material illuminates the identity surface |
| `J` | Vector Speedway | A travelling track and arch rig develop lane chases and perspective rhythms | Identity is revealed within the track composition |
| `C` | Folding Cathedral | Folded architectural wall ribs | Ribs assemble the identity into a readable surface |
| `V` | Ribbon Racetrack | Wide folded lanes curve, braid and sweep across the frame | Lane material carries the fragmented identity |
| `N` | Kinetic Monument | Aspect-aware sculptural folds spread across the stage | Columns unfold into the identity membrane |
| Console | Axis Puzzle | Colored cube groups trade horizontal rows and vertical columns | Moving faces settle into the mark/name |
| Console | Sliding Shutters | Wide slats interlock, exchange lanes and open in opposing banks | Slat surfaces carry the identity |
| Console | Orbit Relay | Circulating tokens trace relays, figure eights and rendezvous | Tokens gather into identity surfaces |
| Console | Iris Bloom | Petals open, spiral, fan and return | Petals flatten into an identity formation |
| Console | Domino Circuit | Cards tip through chains, racks, grids and fans | Cards align to carry the mark/name |
| Console | Elastic Loom | Broad strips weave, pleat and unfold | Strips settle into readable identity surfaces |
| Console | Pendulum Choir | Coordinated swinging groups trade waves and answers | Tokens assemble into an identity formation |
| Console | Magnetic Assembly | Pieces dock into grids, rings, arches and corner banks | Docked pieces carry the identity |

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
| Show | All twenty-four worlds, five act cues, previous/next, Smart Director, automation hold, Clean Open, stage HUD, load/save presets, blackout, and safe fallback |
| Mix + Director | Reaction profile, palette direction, sensitivity, bass, mids, highs, beat, logo scale, act override, automatic act resume, Smart Director, and timeline mode |
| Audio Lab | Auto/EDM/Bass Music/Melodic detector profile, instrument evidence, accepted/rejected diagnostics, adaptive calibration, sample labeling, replay-ready CSV export, and latency |
| Output | World dwell, scene motion, render profile, live GPU budget, clean PNG capture, VSync/high-refresh output, Spout, fullscreen, HUD, device, monitor, timeline mode, and renderer shutdown |
| Production | Production-duty presets, output mastering, master brightness, mapped stage layouts, sync group and Local/Leader/Follower synchronization |
| Screens | Main display selection and synchronized clean-frame mirrors on up to eight displays |
| Rehearsal | Start/stop set recording, GPU-aware smart captures, cue-density master, capture review, approve/rework tags, event timeline, and session export |

Protocol v17 uses a local shared-memory telemetry snapshot, acknowledged numeric commands and one bounded `WM_COPYDATA` message for the DJ name. The renderer remains authoritative, publishes a heartbeat at up to 40 Hz, rejects malformed input, and reports complete signal, detector evidence, musical narrative, world movement/pattern/loop, Showbrain choreography, identity target, category, tempo anchor, Director decisions, GPU timing, output mastering, synchronization, rehearsal, capture and renderer state back to Console. Engine and Console must come from the same release package.

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
| `C` / `V` / `N` | Select Folding Cathedral, Ribbon Racetrack or Kinetic Monument |
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

### v0.21.0 — Show Composer

Three new scenes, native wall/ribbon bridges, conditional branches, analytic route rehearsal with measured GPU-cost feedback, recurring themes and theme hold controls. [Details and evidence](docs/SHOW_COMPOSER.md).

### v0.20.0 — Performance Worlds

Nine rebuilt scenes, eight identity entrances/exits, four arrangements, long-set session memory and smooth Pixel Bank corner departures. See [release notes](docs/PERFORMANCE_WORLDS.md).

### v0.19.0 — Sculptures + Show Direction

Three additional worlds, sorted continuous Causeway motion, scene-aware material handoffs, rehearsed next/follow-up routes and remembered motif programs. See [validation and design notes](docs/SCULPTURES_SHOW_DIRECTION.md).

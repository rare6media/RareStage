# RareStage

**Native Windows festival visualizer with real-time audio analysis, GPU-rendered Rare visuals, operator controls, and clean Spout2 output.**

[![Windows Build](https://github.com/rare6media/RareStage/actions/workflows/windows-build.yml/badge.svg?branch=main)](https://github.com/rare6media/RareStage/actions/workflows/windows-build.yml)

[Download the latest Windows build](https://github.com/rare6media/RareStage/releases/latest) · [View build status](https://github.com/rare6media/RareStage/actions) · Current version: **0.8.1**

RareStage is a native Win32 and DirectX 11 application. It captures the selected Windows playback device through WASAPI loopback, analyzes the music in real time, and directs thirteen authored festival scenes without a browser, Electron, prerecorded video, or per-frame asset uploads.

## At a glance

| Area | Current implementation |
|---|---|
| Platform | Native Windows x64, C++20, Win32 and DirectX 11 |
| Audio | WASAPI loopback, selectable active playback device |
| Analysis | 1024-point FFT, bass, mids, highs, RMS level, peak, stereo energy, spectral flux, beat envelope, BPM and tempo confidence |
| Visuals | Thirteen authored festival scenes: twelve 2D-led motion systems and one restrained 2D/3D architectural scene |
| Renderer | Half-float HDR scene pass, half-resolution bloom, sharpened filmic composite |
| Show control | Direct2D/DirectWrite overlay with mouse, wheel and keyboard operation |
| Automation | Audio-directed scene selection or deterministic show timeline |
| Presets | Four named show slots with load and overwrite controls |
| Output | Borderless stage display, multi-monitor selection and clean Spout2 sender |
| Performance | 62%, 78%, 100% and 125% HDR render-scale profiles; VSync or unlocked output |
| Persistence | Show settings and preset slots saved under `%LOCALAPPDATA%\RareStage` |

## Signal and render pipeline

```text
Windows playback device
        │
        ▼
WASAPI loopback ──► FFT + beat/tempo analysis ──► show director
                                                        │
                                                        ▼
Rare logo + procedural 2D/3D scene ──► HDR ──► bloom ──► filmic composite
                                                                  │
                                                 clean Spout2 ◄───┤
                                                                  ▼
                                                  Show Control overlay
                                                                  │
                                                                  ▼
                                                        stage display
```

The `RareStage Clean` Spout sender is published before Show Control is composited. OBS, RareOBS, Resolume, and other Spout-capable receivers therefore receive the finished visual without operator controls.

## Festival scenes

| Key | Scene | Direction |
|---|---|---|
| `1` | Rare Monolith | Layered panels with one restrained dimensional architectural accent |
| `2` | Crystal Bloom | Seamless circle-to-square cells, color wash and flat petal loops |
| `3` | Celestial Horizon | Wide animated landscape, drifting logo, moving ridges and clouds |
| `4` | Cyber Shrine | Sliding 2D gates, tiled panels and clean scanning light |
| `5` | Vector Rush | Kinetic ribbons, chevrons and diagonal graphic motion |
| `6` | Liquid Core | Smooth abstract contour loops, flowing blobs and ribbon currents |
| `7` | Signal Break | The dedicated glitch scene: scan fields, fractured blocks and displaced logo slices |
| `8` | Hologram Garden | Floating logo path, botanical petals, ribbons and drifting spores |
| `9` | Rare Ascension | Vertical graphic rays, rising bands and restrained peak energy |
| `10` via `[` / `]` | Neon Runway | Cel-shaded cyber-city parallax, moving tracks and foreground occlusion |
| `11` via `[` / `]` | Synthetic Meadow | Calm animated landscape, moving field lines, clouds and stars |
| `12` via `[` / `]` | Modular Fold | Original mathematical tiles that smoothly merge between circles, squares and rotated grids |
| `13` via `[` / `]` | Bass Reactor | 2D speaker field, EQ bands, controlled lasers and drop punctuation |

Automatic mode selects all thirteen scenes from calm, medium, or high-intensity groups using the live audio signal. Timeline mode follows a curated thirteen-scene order associated with the active show preset. Keys `1`–`9` remain direct scene shortcuts; `[` and `]` step backward or forward through the complete scene library.

The renderer is 2D-first. Twelve scenes skip the ray-marched hero pass entirely and instead use seamless animated backgrounds, parallax, graphic layers, occlusion and scene-specific logo movement. Rare Monolith is the only scene retaining a restrained 3D architectural accent.

Bass controls measured scale and structural impact, mids control loop speed and parallax, highs reveal detail, and beats add short accents instead of repeatedly spinning or shaking a model. Signal Break is intentionally the only heavily glitch-led scene; the rest prioritize smooth continuous motion.

## Show Control

Show Control is rendered inside the DirectX compositor, so it remains visible over the flip-model swap chain in both windowed and fullscreen output. Press `Tab` to show or hide it.

| Overlay row | Values or range |
|---|---|
| Reaction | Calm, Balanced, Stage, Aggressive |
| Palette | Rare Cyan, Nebula, Sakura, Solar |
| Sensitivity | 50–150% |
| Bass | 50–150% |
| Mids | 50–150% |
| Highs | 50–150% |
| Beat | 50–150% |
| Logo | 75–125% |
| Scene time | 8–60 seconds |
| Transition | 0.2–3.0 seconds |
| Transition FX | Panel Cut, Prism Iris, Glitch Hold, Bass Shutter |
| Render profile | Performance, Balanced, Native HD, Ultra 4K |
| Frame output | Display/VSync or unlocked high refresh |
| Show mode | Audio Directed or Show Timeline |
| Spout output | Clean sender on or off |

The panel also reports the active preset, scene, automation state, playback device, BPM, and smoothed bass, mid, high, and beat meters.

## Controls

| Input | Action |
|---|---|
| `1`–`9` | Select a scene and enter manual scene mode |
| `[` / `]` | Previous or next scene across the complete thirteen-scene library |
| `0` or `A` | Toggle automatic scene direction |
| `D` | Cycle active Windows playback devices |
| `M` | Move stage output to the next monitor |
| `P` | Cycle Rare color palettes |
| `Tab` | Show or hide Show Control |
| Left click | Select a row; use its cyan minus or plus button |
| Right click | Decrease the clicked Show Control row |
| Mouse wheel | Adjust the row under the pointer |
| `Up` / `Down` | Select the previous or next Show Control row |
| `Left` / `Right` | Decrease or increase the selected setting |
| `Enter` | Increase the selected setting |
| `F1`–`F4` | Load show preset 1–4 |
| `Shift+F1`–`Shift+F4` | Save the current show over preset slot 1–4 |
| `T` | Toggle Audio Directed and Show Timeline modes |
| `V` | Toggle VSync and unlocked high-refresh output |
| `S` | Toggle the clean Spout2 sender |
| `Space` | Freeze or resume automatic scene changes |
| `F11` or `Alt+Enter` | Toggle borderless fullscreen |
| `Esc` | Leave fullscreen, or quit when windowed |

## Named show presets

| Slot | Factory direction | Starting scene | Default timing |
|---|---|---|---|
| `F1` Clean Open | Calm, Rare Cyan | Celestial Horizon | 28-second scenes, 1.8-second Prism Iris |
| `F2` Melodic Bloom | Balanced, Sakura | Crystal Bloom | 18-second scenes, 1.2-second Prism Iris |
| `F3` Rare Drop | Stage, Rare Cyan | Vector Rush | 12-second scenes, 0.8-second Bass Shutter |
| `F4` Glitch Finale | Aggressive, Nebula, timeline enabled | Signal Break | 10-second scenes, 0.45-second Glitch Hold |

Preset slots can be overwritten with `Shift+F1` through `Shift+F4`. Saved presets include reaction tuning, palette, timing, transition FX, render profile, frame output, automation state, timeline state, Spout state, monitor, and current scene.

## Performance profiles

| Profile | HDR scene scale | Intended use |
|---|---:|---|
| Performance | 62% | Lower-power GPUs or high-refresh output |
| Balanced | 78% | Reduced GPU load with a native-resolution final composite |
| Native HD | 100% | Full native scene rendering |
| Ultra 4K | 125% | Supersampled stage output on high-end GPUs |

The final composite always matches the active window or fullscreen output resolution. VSync is enabled by default; unlocked mode removes the present interval cap for high-refresh and 120 Hz stage systems.

## Download and run

1. Open [Releases](https://github.com/rare6media/RareStage/releases/latest).
2. Download `RareStage-windows-x64.zip`.
3. Extract the complete `RareStage` folder.
4. Run `RareStage.exe`.

Keep `assets/rare-logo.png` beside the executable in the packaged `assets` folder. RareStage writes `RareStage-startup.log` beside the executable if startup fails.

### System requirements

- 64-bit Windows 10 or Windows 11
- DirectX 11-capable GPU and current graphics driver
- An active Windows playback device for live reactivity
- A Spout-capable receiver only if shared-texture output is needed

## Build from source

Install Visual Studio with **Desktop development with C++**, the Windows SDK, Git, and CMake. From a Developer PowerShell:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --target RareStage --parallel
```

Or run:

```powershell
.\build-release.ps1
```

CMake retrieves the official Spout2 source and links `SpoutDX_static`. Use `-DRARESTAGE_ENABLE_SPOUT=OFF` during configuration for a build without Spout output.

## Automated validation and releases

Every push and pull request runs the Windows workflow on GitHub Actions:

1. Configure the x64 CMake project.
2. Build the optimized RareStage target.
3. Compile-test all embedded DirectX shaders.
4. Launch the native executable in startup-test mode.
5. Package the executable, Rare logo, README, and third-party notices.
6. Upload the portable ZIP as a workflow artifact.

Successful pushes to `main` publish or refresh the `v0.8.1` GitHub release automatically.

## Roadmap

### v0.8 — Festival art direction (current)

- Thirteen authored Rare scene environments
- Four beat-aware transition systems with brief hold, cut and afterimage stages
- Twelve 2D-first motion systems, one restrained hybrid scene, moving logo choreography and dynamic backgrounds
- Extended timeline and audio-directed scene groups

### v0.9 — Live show operation

- MIDI and OSC control mapping
- Visual cue editor with timed scene and preset changes
- Importable and exportable preset bundles
- User shader-pack foundation with safe validation

### v1.0 — Production reliability

- In-app GPU and audio diagnostics
- Device-loss recovery and clearer crash reports
- Signed Windows installer and update channel
- Expanded automated render and performance testing

### Later stage systems

- NDI output
- Synchronized multi-machine playback
- External show-control API
- Venue profiles and project-level show files

## Repository layout

```text
assets/                         Rare logo used by the renderer
src/main.cpp                    Win32 app, audio engine, shaders and renderer
.github/workflows/              Windows build, test, package and release automation
CMakeLists.txt                  Native build and Spout2 integration
build-release.ps1               Local one-command Release build
THIRD_PARTY_NOTICES.md          Third-party licensing
```

## Third-party software

RareStage includes [Spout2](https://github.com/leadedge/Spout2) under the BSD 2-Clause License. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

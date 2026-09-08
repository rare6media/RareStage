# RareStage 0.18.0 validation

Validated locally on September 8, 2026 with an NVIDIA GeForce RTX 4070 SUPER, Windows x64, MSVC 19.29, Release configuration and Spout support compiled in. The operator's v0.17 renderer remained active during GPU measurements. This is not a controlled isolated-GPU comparison.

## Behavior and controls

The six wall systems have sixteen authored motifs. Stagewall, Morph Wall, Kinetic Array and Kinetic Facade share the clean moving-cube treatment; Panel Cascade and Pixel Loom retain their finer surface detail. Eight palette directions are available in Mix + Director. The Director uses all eight, while adjacent colors remain coordinated by row, column and active formation. Prism Atrium and Pulse Causeway have sixteen motif families within their persistent geometry.

Timed automatic cycling is enabled once when loading settings from an older version. Dwell is configurable on Output. At expiry, the Director waits for the next musical bar on its continuous clock; it no longer requires a new song section before progressing. Confirmed chapter cues can still advance a qualified change. Manual scene selections preserve the current automatic mode and restart dwell. `A` toggles automatic cycling, and `Space` holds it. Silence uses the established/fallback tempo, so an otherwise steady or silent input does not permanently stall the show.

Pixel Bank's corner choreography uses a latched trajectory from the current position and velocity to a corner. A forecast drop supplies the intended arrival beat. An unexpected confirmed drop initiates a one-beat move instead of teleporting. Prediction quality determines whether an anticipated arrival coincides with the actual drop. The mark resumes bouncing after arrival and remains bounded inside portrait or landscape output.

The Screens page selects the main Stage display and enables clean mirrors independently or on all other displays. Up to eight displays are enumerated. All mirrors sample one rendered frame, preserve its aspect ratio and exclude the operator HUD. Different display shapes use black letterboxing; the source resolution is the main Stage frame, so choose the 4K display as the main fullscreen Stage for a native 4K source. Display changes rebuild the mirror windows and preserve selections by display device name where available. ESC on a mirror closes mirror outputs.

Alt+Enter and F11 work in Stage and Console. Fullscreen is borderless, uses the selected monitor, ignores key repeats, and restores the previous window placement. Moving the window between displays updates its selected monitor. DXGI automatic fullscreen handling is disabled following [Microsoft's window-association guidance](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgifactory-makewindowassociation). Console binds commands to the process identified by its telemetry, preventing an older open Stage window from receiving commands intended for the new build.

## Local validation

- Native Stage/Console build and HLSL compilation; control protocol, audio, motion, Director, production and Club Loop contracts.
- **16,896 artifact frames**: 12,288 wall frames cover all sixteen motifs with logo/silent and DJ-name/instrument passes; Prism Atrium and Pulse Causeway add 4,096 frames, and the other two environmental scenes add 512. Guard thresholds remain mean luminance at least 0.10, at least 45% of sampled pixels above 0.13 luminance, and silent adjacent-frame change below 0.06. Sampling is every fourth pixel at 1280×720 with an eighth-beat step.
- **5,808 scene-continuity frames**: twelve scene pairs, four motion paths and 121 phase positions, with different pattern seeds per path to include the expanded motifs. Exact endpoints must match independent scene renders within 0.00001 mean RGB; near-endpoint difference must remain below 0.003, and adjacent frames below 0.06.
- **640 color/motif cases**, four rendered positions each: five updated cube/architectural worlds × eight palettes × sixteen motifs. The wall cases use the same luminance/coverage guard. Sixty-four representative images are saved.
- **18 display/timing/corner checks** exercise the actual Alt+Enter window handler, key repeat, F11 restoration, aspect fitting, creation/removal/recreation of mirror swap chains, pixel readback against the source, black letterboxing, predicted corner arrivals at 60/120/144 Hz, unexpected-drop motion, timed automatic cycling through a steady track and manual hold.
- Screens UI is rendered at 1280×640 with eight synthetic display cards. The test checks button rectangles and saves a bitmap for direct visual inspection.


Observed minimum wall luminance: **0.1397**; minimum lit coverage: **63.72%**; maximum silent adjacent-frame change: **0.05855**. Maximum scene-continuity change: **0.04648**. All corresponding guards passed.

Display tests create real hidden swap chains with portrait and wide dimensions, including negative desktop coordinates. They do not claim a physical eight-monitor installation or hardware frame lock. Mirrors share the rendered animation frame; scanout timing still depends on displays, refresh rates, drivers and the Windows compositor.

A 28-second 60 FPS preview uses synthetic 128 BPM instrument lanes and authored cues. It shows the expanded wall patterns, identity, wall morphing, Pulse Causeway and a forecast Pixel Bank corner arrival. It has no audio track.

## GPU measurements

The scene benchmark uses 3840×2160, 100% HDR scale, half-resolution bloom and native composite. Each of eighteen scenes has 120 warmup frames and 960 individually timestamped frames: **17,280 measured frames**. Its compressed musical clock sweeps 128 beats during those 960 samples to cover all sixteen motifs, including identity formation. Scene-motion timing adds **1,452 measured frames** across twelve representative pairs, with 60 warmup frames per pair and 121 animation positions covering the expanded motif bank. GPU query slots are drained in batches so every measured frame is collected.

These are render-path timings, including the active world pass(es), surface morph, bloom and final composite. They exclude mirror copy/presentation, primary display Present/VSync, Spout transfer, audio capture, Console work and PNG encoding. Submission throughput in the CSV is not display FPS. Native mirrored 4K/120 remains an end-to-end validation task with the intended screens and receiver.

| World | Mean ms | P95 ms | P99 ms | Maximum ms |
|---|---:|---:|---:|---:|
| Morph Wall | 2.200 | 2.977 | 3.114 | 3.293 |
| Prism Atrium | 3.787 | 4.737 | 4.964 | 5.254 |
| Orbital Garden | 2.169 | 2.935 | 3.102 | 3.398 |
| Stagewall | 2.245 | 2.986 | 3.208 | 3.559 |
| Wavewall | 1.758 | 2.678 | 2.821 | 3.052 |
| Pixel Bank | 2.096 | 2.952 | 3.085 | 3.241 |
| Prism Cascade | 1.981 | 2.823 | 3.007 | 3.206 |
| Aurora Range | 2.746 | 3.252 | 3.363 | 4.112 |
| Chroma Blades | 1.938 | 2.852 | 2.998 | 3.163 |
| Kinetic Array | 2.241 | 3.019 | 3.176 | 3.342 |
| Kinetic Canopy | 2.039 | 2.912 | 3.038 | 3.120 |
| Pixel Loom | 2.239 | 3.034 | 3.194 | 3.369 |
| Matrix Vault | 1.932 | 2.990 | 3.242 | 3.593 |
| Panel Cascade | 2.261 | 2.987 | 3.160 | 3.493 |
| Signal Rail | 3.373 | 4.309 | 4.498 | 4.745 |
| Kinetic Facade | 2.275 | 3.003 | 3.107 | 3.358 |
| Pulse Causeway | 4.535 | 5.620 | 6.193 | 6.602 |
| Void Gates | 2.309 | 3.033 | 3.203 | 3.707 |

| Scene animation | Mean ms | P95 ms | P99 ms | Maximum ms |
|---|---:|---:|---:|---:|
| Stagewall → Panel Cascade | 2.339 | 3.042 | 3.169 | 3.236 |
| Panel Cascade → Stagewall | 2.310 | 3.151 | 3.209 | 3.257 |
| Stagewall → Kinetic Array | 2.330 | 3.106 | 3.237 | 3.287 |
| Kinetic Array → Pixel Loom | 2.312 | 3.045 | 3.138 | 3.182 |
| Kinetic Facade → Morph Wall | 2.403 | 2.995 | 3.145 | 3.199 |
| Prism Atrium → Stagewall | 5.973 | 7.365 | 7.591 | 7.862 |
| Stagewall → Prism Atrium | 6.046 | 7.286 | 7.518 | 7.845 |
| Pulse Causeway → Stagewall | 6.735 | 7.671 | 8.031 | 8.081 |
| Stagewall → Pulse Causeway | 6.663 | 7.698 | 7.953 | 8.098 |
| Prism Atrium → Pulse Causeway | 7.976 | 9.172 | 9.507 | 9.576 |
| Pulse Causeway → Aurora Range | 7.375 | 8.027 | 8.542 | 8.703 |
| Orbital Garden → Void Gates | 4.229 | 5.541 | 5.629 | 5.804 |

The slowest individual-world sample was **6.602 ms**. The slowest scene-animation sample was **9.576 ms**, with a worst pair P99 of **9.507 ms**. Some mixed-scene samples exceed the **8.333 ms** budget for 120 FPS while the older renderer is also active. This does not certify uninterrupted native 4K/120, and mirror/receiver/display presentation remains unmeasured. The modern scenes bypass unused legacy ambience calculations; internal render resolution was not reduced.


For a single initialization that runs the complete rendered suite and GPU measurements, use `--release-validation --preview --no-preview`: the first preview flag enables the v0.18 showcase, while `--no-preview` skips the older choreography capture.

## Reproduce

```powershell
ctest --test-dir build -C Release --output-on-failure
Start-Process build/Release/RareStage.exe -ArgumentList "--self-test" -WindowStyle Hidden -Wait
Start-Process build/Release/RareStage.exe -ArgumentList "--visual-regression-test" -WindowStyle Hidden -Wait
Start-Process build/Release/RareStage.exe -ArgumentList "--choreography-test --no-preview" -WindowStyle Hidden -Wait
Start-Process build/Release/RareStage.exe -ArgumentList "--stage-lighting-test --preview" -WindowStyle Hidden -Wait
Start-Process build/Release/RareStageConsole.exe -ArgumentList "--screen-ui-test" -WindowStyle Hidden -Wait
Start-Process build/Release/RareStage.exe -ArgumentList "--benchmark-4k" -WindowStyle Hidden -Wait
Start-Process build/Release/RareStage.exe -ArgumentList "--choreography-benchmark" -WindowStyle Hidden -Wait
```

Lighting, mirror, corner and palette evidence is under `RareStage-lighting-validation/`. Omit `--preview` to skip its 1,680 PNG capture sequence. The other output directories remain `RareStage-visual-validation/`, `RareStage-choreography-validation/` and `RareStage-benchmark-4k/`. Render-validation modes use hidden windows, skip the live control endpoint, disable audio/network/Spout output and do not load or save operator settings.

Stage and Console use protocol v14; network leaders/followers use sync packet v2 for the expanded palette range. Use a matching v0.18 pair and upgrade network participants together. Local mirrors require no network configuration. The portable package includes checksums and source-commit provenance. A local package is not a published GitHub release, and local results do not imply a finished CI run.

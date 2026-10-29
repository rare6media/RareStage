# RareStage 0.16.0 validation (historical)

These measurements describe the earlier v0.16 build. For the current scene set and results, see [v0.17 validation](CONTINUOUS_CHOREOGRAPHY_VALIDATION.md).

Measured locally on September 8, 2026 with an NVIDIA GeForce RTX 4070 SUPER, driver 581.57, Windows x64, MSVC 19.29, Release configuration and Spout support compiled in. The existing operator's Stage and Console remained running throughout these tests.

## Outcome

The native Stage and Console build, all four HLSL entry points, control protocol, audio classifier, motion timing, Director, production and Club Loop contracts passed. The new visual regression passed 7,168 production-rendered frames. Those include 6,144 wall frames spanning all eight pattern banks, both identity targets, a silent pass and a synthetic instrument pass. Four environmental worlds received another 1,024 phase samples. One hundred clean PNGs were saved; representative formation and environment frames were inspected directly.

The wall guard rejects average display luminance below 0.10, fewer than 45% of sampled pixels above 0.13 luminance, or a silent adjacent-frame mean change above 0.06. The observed minimum wall coverage was **60.52%**, the minimum average luminance **0.1987**, and the largest silent mean change **0.03494**. Sampling uses every fourth pixel at 1280×720 with a 1/8-beat time step. These are artifact regression thresholds, not a claim that every possible user setting, musical input or display layout has been certified.

The production stanza scheduler was also exercised through all five section revisions while an identity held; the identity target and weight remained stable. The logo's internal R/6 detail and the default RARESIX name were inspected in the rendered wall frames. The custom-name rasterizer and bounded input protocol are retained.

## 4K GPU measurements

Each of the 18 scenes rendered at **3840×2160**, 100% HDR scene scale, half-resolution bloom and native final composite. After 120 warmup frames, 960 frames per scene were individually timestamped, for **17,280 measured frames**. The fixture drives the same scene/bloom/composite path as live playback, with synthetic 128 BPM instrument lanes and an identity cycle. No resolution upscaling was used.

| Scene | Mean GPU ms | P95 ms | P99 ms | Maximum ms |
|---|---:|---:|---:|---:|
| Morph Wall | 1.508 | 4.161 | 4.702 | 4.932 |
| Ember Basin | 3.249 | 5.706 | 6.156 | 8.421 |
| Orbital Garden | 2.068 | 4.770 | 5.225 | 5.519 |
| Stagewall | 1.594 | 4.397 | 4.957 | 6.447 |
| Wavewall | 1.582 | 4.289 | 4.927 | 5.660 |
| Pixel Bank | 2.080 | 4.808 | 5.264 | 5.777 |
| Prism Cascade | 1.780 | 4.616 | 5.108 | 6.225 |
| Aurora Range | 3.415 | 5.755 | 6.100 | 9.137 |
| Chroma Blades | 1.916 | 4.685 | 5.068 | 5.534 |
| Kinetic Array | 2.040 | 4.820 | 5.223 | 6.117 |
| Kinetic Canopy | 1.964 | 4.721 | 5.117 | 6.131 |
| Pixel Loom | 2.105 | 4.849 | 5.226 | 5.450 |
| Matrix Vault | 2.366 | 4.995 | 5.352 | 6.386 |
| Panel Cascade | 2.168 | 4.898 | 5.159 | 5.453 |
| Signal Rail | 3.256 | 5.682 | 6.094 | 9.082 |
| Kinetic Facade | 2.012 | 4.668 | 5.100 | 6.226 |
| Tidal Expanse | 2.046 | 4.824 | 5.290 | 6.516 |
| Void Gates | 2.330 | 4.998 | 5.317 | 6.126 |

Every scene's P99 was below the **8.333 ms** budget for 120 frames/second. The worst P99 was **6.156 ms**; the worst observed frame was **9.137 ms**. Three scenes had occasional frames over the budget. This was not an isolated GPU run: the user's previous renderer and other desktop applications remained active.

These timings cover GPU rendering only. They exclude display Present/VSync, the Spout transfer, audio capture, Console and PNG readback/encoding. Reported submission throughput in the CSV is not display FPS. A sustained end-to-end 4K/120 show with the intended receiver and display is still needed before certifying uninterrupted output. Performance on other GPUs is unmeasured.

The new environments are procedural 2D/2.5D real-time worlds. Fire uses advected fractal fields; water uses seven analytical wave components; aurora uses four layered curtains. They are not volumetric fluid simulations or prerecorded video.

## Run the checks

Build Stage and Console together, with assets/rare-logo.png next to the executables in their assets directory. From the repository root:

```powershell
ctest --test-dir build -C Release --output-on-failure
Start-Process build/Release/RareStage.exe -ArgumentList "--self-test" -WindowStyle Hidden -Wait
Start-Process build/Release/RareStage.exe -ArgumentList "--visual-regression-test" -WindowStyle Hidden -Wait
Start-Process build/Release/RareStage.exe -ArgumentList "--benchmark-4k" -WindowStyle Hidden -Wait
```

Visual captures and metrics go to `RareStage-visual-validation/`; timings go to `RareStage-benchmark-4k/`. GPU adapter identity is written to `RareStage-adapter.txt`. Validation uses a hidden window, skips the live control endpoint, disables audio/network/Spout output and does not load or save operator settings.

The existing Console-to-Stage launch soak was not rerun locally because it would contend with the active operator session. The Windows CI workflow retains that test and adds the rendered artifact regression; a local result does not imply a completed CI run or a published release.

## Operator use

- Close the old Stage and Console before launching the new matched pair.
- In Production, set the DJ name and enable DJ identity. The mark appears in the first eligible stanza, the name in the next identity stanza, with a pattern-only stanza in between. At 128 BPM, the first name reveal begins around 16 seconds after entering a scene and reaches full resolve around 18.5 seconds.
- Select Ember Basin (`2`), Aurora Range (`8`) or Tidal Expanse (`O`) directly, or use the Atmosphere category with Smart Director.
- For 4K output, use Native HD (100%) or Ultra 4K. Ultra now caps its internal pass at 3840×2160. Use a 120 Hz output display and the appropriate VSync/frame-output setting; watch the GPU timing on the Output page with the intended receiver running.

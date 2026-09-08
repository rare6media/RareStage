# RareStage 0.17.0 validation

Measured locally on September 8, 2026, on an NVIDIA GeForce RTX 4070 SUPER, Windows x64, MSVC 19.29, Release configuration, with Spout support compiled in. Normal desktop applications remained active; this was not a controlled laboratory run.

## Rendered behavior

Stagewall now inverse-maps whole travelling rows before selecting cube cells. Bright neighboring faces join into ribbons. Eight authored motifs hold for the opening 58% of an eight-beat phrase and reshape over its closing portion. The retained identity mask samples the logo's internal detail and the editable DJ name across fine panel surfaces.

Wall-to-wall animation uses a shared lattice with interpolated spacing, row travel, pattern field and panel treatment. Mixed scenes render both worlds into HDR surfaces and carry incoming material across opening or forming panels. Other scene pairs share a flowing surface carrier. The old spatial old/new mask and seam flash have been removed. Two extra HDR targets cost approximately 126.6 MiB at native 4K; they are allocated during resize, not at the moment of a scene change.

Prism Atrium and Pulse Causeway replace fire and water. Each has eight beat-driven pattern families within persistent architectural geometry. Their motion and illumination use the continuous musical clock, with separate bass, snare and hat accents. The Director calculates four-beat scene motion for active sections and eight beats for openings/breakdowns from the tempo at cue time. Manual duration remains adjustable. These are procedural 2D/2.5D worlds, not prerecorded loops.

## Validation

- Native Stage and Console builds, all five HLSL entry points, control protocol, audio, motion, Director, production and Club Loop contracts passed.
- Artifact regression: **7,168 frames**, including 6,144 wall frames covering eight motifs, both identity targets and silent/instrument passes. Minimum sampled wall luminance was **0.1750** and minimum lit coverage **60.76%**. The guard requires luminance at least 0.10 and coverage at least 45% above 0.13 luminance. Silent adjacent-frame change must remain below 0.06. Samples use every fourth pixel at 1280×720.
- Scene continuity: **5,808 frames** across 12 scene pairs, four motion paths and 121 phase positions per pair/path. Maximum adjacent-frame mean RGB change was **0.04469**, below the 0.06 guard. Endpoints match their independent scene render within 0.00001 mean RGB; samples immediately next to each endpoint must differ by less than 0.003. These tests use 640×360 and sample every second pixel.
- The continuity test also checks rapid cue queuing, cancellation, preservation of a queued follower's motion duration/path, four/eight-beat Director durations, and acceptance of longer sync durations with rejection above eight seconds.
- A 20-second, 60 FPS, 640×360 rendered preview shows Stagewall motion, the Rare formation, Stagewall opening into Prism Atrium, and Prism Atrium moving into Pulse Causeway. The preview uses synthetic 128 BPM instrument lanes and has no audio track.

The metrics catch the reported dark/sparse wall and abrupt scene-change regressions. They do not certify every possible cue, track, display layout or custom setting. Representative wall, identity, architecture and intermediate frames were inspected directly.

## Native 4K GPU timing

All tests below use 3840×2160 at 100% HDR scale, half-resolution bloom and native composite. No resolution upscaling. GPU timing includes scene, morph when active, bloom and final composite. It excludes display Present/VSync, Spout transfer, audio capture, Console, and PNG readback/encoding. CSV submission throughput is not display FPS.

Each individual world has 120 warmup frames and 960 individually timestamped frames: **17,280 measured frames**. Each scene pair has 60 warmup frames and 121 individually timestamped animation positions: **1,452 measured frames**. Pair timings cover the Row Flow path; continuity testing covers all four paths. Not every possible scene pair or driver/GPU is benchmarked.

| World | Mean ms | P95 ms | P99 ms | Maximum ms |
|---|---:|---:|---:|---:|
| Morph Wall | 1.222 | 2.145 | 2.353 | 2.535 |
| Prism Atrium | 1.895 | 4.407 | 4.745 | 5.204 |
| Orbital Garden | 1.422 | 2.359 | 2.537 | 2.732 |
| Stagewall | 1.408 | 2.246 | 2.479 | 2.687 |
| Wavewall | 1.061 | 1.940 | 2.217 | 2.510 |
| Pixel Bank | 1.334 | 2.265 | 2.498 | 2.658 |
| Prism Cascade | 1.237 | 2.167 | 2.409 | 2.696 |
| Aurora Range | 2.340 | 3.387 | 3.666 | 3.852 |
| Chroma Blades | 1.307 | 2.208 | 2.399 | 2.690 |
| Kinetic Array | 1.647 | 2.509 | 2.688 | 2.948 |
| Kinetic Canopy | 1.307 | 2.236 | 2.402 | 3.213 |
| Pixel Loom | 1.807 | 2.528 | 2.700 | 3.293 |
| Matrix Vault | 1.544 | 2.453 | 2.650 | 2.741 |
| Panel Cascade | 1.641 | 2.497 | 2.650 | 2.881 |
| Signal Rail | 2.303 | 3.446 | 3.712 | 3.942 |
| Kinetic Facade | 1.615 | 2.668 | 4.162 | 5.115 |
| Pulse Causeway | 2.753 | 3.887 | 4.090 | 4.294 |
| Void Gates | 1.613 | 2.447 | 2.874 | 3.227 |

| Scene animation | Mean ms | P95 ms | P99 ms | Maximum ms |
|---|---:|---:|---:|---:|
| Stagewall → Panel Cascade | 1.854 | 2.481 | 2.673 | 2.909 |
| Panel Cascade → Stagewall | 1.429 | 2.283 | 2.748 | 2.764 |
| Stagewall → Kinetic Array | 1.865 | 2.617 | 2.857 | 2.869 |
| Kinetic Array → Pixel Loom | 1.774 | 2.492 | 2.812 | 2.984 |
| Kinetic Facade → Morph Wall | 1.476 | 2.231 | 2.396 | 2.773 |
| Prism Atrium → Stagewall | 3.653 | 4.929 | 5.238 | 5.669 |
| Stagewall → Prism Atrium | 3.572 | 4.737 | 4.966 | 5.205 |
| Pulse Causeway → Stagewall | 4.552 | 5.672 | 6.369 | 6.401 |
| Stagewall → Pulse Causeway | 4.270 | 5.694 | 5.966 | 6.311 |
| Prism Atrium → Pulse Causeway | 5.318 | 6.333 | 6.705 | 6.920 |
| Pulse Causeway → Aurora Range | 5.358 | 6.829 | 7.118 | 7.560 |
| Orbital Garden → Void Gates | 2.941 | 4.348 | 4.546 | 5.317 |

All measured GPU frames were below the 8.333 ms budget for 120 frames/second. The worst scene-animation P99 was **7.118 ms**, and the slowest measured frame was **7.560 ms**. A sustained end-to-end 4K/120 show with the intended receiver and display remains necessary before certifying uninterrupted output. Other hardware is unmeasured.

## Reproduce

```powershell
ctest --test-dir build -C Release --output-on-failure
Start-Process build/Release/RareStage.exe -ArgumentList "--self-test" -WindowStyle Hidden -Wait
Start-Process build/Release/RareStage.exe -ArgumentList "--visual-regression-test" -WindowStyle Hidden -Wait
Start-Process build/Release/RareStage.exe -ArgumentList "--choreography-test --no-preview" -WindowStyle Hidden -Wait
Start-Process build/Release/RareStage.exe -ArgumentList "--benchmark-4k" -WindowStyle Hidden -Wait
Start-Process build/Release/RareStage.exe -ArgumentList "--choreography-benchmark" -WindowStyle Hidden -Wait
```

Omit `--no-preview` to render the 1,200 PNG preview sequence. Artifact output is under `RareStage-visual-validation/`, scene continuity and preview output under `RareStage-choreography-validation/`, and individual-world GPU timings under `RareStage-benchmark-4k/`. Validation uses a hidden window, skips the live show control endpoint, disables audio/network/Spout output and does not load or save operator settings.

## Use the build

Close the older Stage/Console pair and launch the matched v0.17 pair. Choose Stagewall (`4`), Prism Atrium (`2`), or Pulse Causeway (`O`). Set the DJ name and enable identity on Production. The logo and name alternate on selected 16-beat stanzas with a pattern-only stanza between them; let a scene run long enough for both reveals. A new identity stanza starts after scene motion completes.

Console's Output page now calls the controls Motion Time and Motion Path. Smart Director uses bar-length scene motion; manual selection uses Motion Time. Repeated scene cues update one pending destination rather than restarting the current animation.

Protocol v13 and the fixed-size sync packet layout are retained. Network leaders and followers should all use v0.17: older receivers reject scene-motion durations above three seconds. Native 4K timing does not change the need for a 120 Hz display and an appropriate receiver configuration.

Windows CI includes the artifact and scene-continuity regressions as well as the existing Console launch soak. Local validation is not evidence that GitHub CI has finished or that a release has been published.

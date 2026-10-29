# RareStage v0.21.0 — Show Composer

September 8, 2026. Deterministic C++/HLSL; no runtime AI, network inference or token consumption.

## What shipped

Folding Cathedral (C), Ribbon Racetrack (V) and Kinetic Monument (N) join the existing 21 scenes. Cathedral interpolates the common wall lattice into stretched, folded ribs. Racetrack and Monument share a parametric ribbon rig with Ribbon Foundry. Each scene retains four arrangements and sixteen authored motif seeds, with persistent musical-clock development and eight identity entrance/eight exit choices.

The native bridge groups are Morph Wall, Stagewall, Kinetic Array, Pixel Loom, Panel Cascade, Kinetic Facade and Folding Cathedral (42 directed pairs), and Ribbon Foundry, Ribbon Racetrack and Kinetic Monument (six directed pairs). Geometry, arrangement and old/new motif programs interpolate through a quintic progress curve. The remaining 504 directed pairs use category-aware HDR material transport. This release does not claim universal mesh correspondence.

## Show planning

At a 16-beat planning boundary the Director scores three continuations: continued movement, drop and breakdown. It retains the existing current/following route lookahead, category filters, recent exposure/edge fatigue and final eight-beat route lock. A confirmed section may select its prepared branch; a forecast alone cannot commit it. Prepared plans expire after 32 beats, reject a changed scene/category, and permit only the source section revision or its immediate successor. Manual world cues invalidate them, including a cue to the current world.

The Composer preserves the prepared route when its section is confirmed. The independent 40–120 second dwell and musical cue rules still govern when the world may actually change. Holds do not stop rendering or the musical clock.

## Route estimates

A single scene catalog provides CPU/GPU geometry-family data, anchors, nominal pace/scale, energy/density and native-bridge group. A bounded nine-sample estimate compares nominal anchor/velocity envelopes over eight future beats. It is a compatibility heuristic, not a sampled scene image or measured vertex trajectory.

Completed GPU timestamps are tagged with the submitted scene and pixel count. Only steady live frames contribute. After eight samples per scene, an exponential moving average supplies the route cost estimate. Native bridges use the larger scene estimate plus headroom; material handoffs combine the estimates with an extra pass allowance. Unknown scenes use conservative catalog estimates and a much smaller budget penalty so exploration continues. Estimates scale with render pixel count, which is approximate because post-processing and output costs do not scale perfectly.

This alters future route priorities; it does not submit extra preview draws or abruptly reduce the live output's quality. Hardware contention, thermal changes, output configurations and nonlinear GPU workloads can invalidate estimates. Timing results are reported separately from visual correctness.

## Recurring themes

River, Crown, Orbit, Pulse, Ascent and Tidal apply a shared continuous spatial deformation and formation-color bias. Beat and phrase clocks keep their original timing. Pixel Bank's mark position remains outside theme deformation. Theme selection balances prior uses; after at least three scene visits an unheld theme may change on a safe 16-beat boundary, outside a scene handoff or readable identity formation. The new parameters settle over eight beats.

Hold Theme keeps the current theme while scene automation continues. Next Theme queues a different theme for the next safe boundary and preserves the hold state. Session memory resets on restart. Theme names describe authored visual programs, not musical or semantic understanding.

## Controls and outputs

The Console shows all 24 scenes, Hold Theme and Next Theme. Shared telemetry includes theme, hold, all three branch candidates, continuity estimate and estimated/measured route cost status. Console protocol is v16. UDP packet v4 is 88 bytes and carries theme id/parameters with finite/range validation; old packet versions are rejected. UDP remains a show-clock system, not frame-perfect remote video duplication. Local display mirrors receive the same completed render.

## Validation

The local build passed protocol and seven CPU suites (audio, motion, Director, production/network packet, club-loop, performance/Composer and shader-cache corruption checks), plus the native Console grid check. Both executables report product version 0.21.0.

| Production rendering check | Coverage | Result |
|---|---|---|
| Scene/program coverage | 24 scenes × 64 programs = 1,536 renders | Minimum mean luma 0.086499 |
| Identity choreography | 144 sequences, 34,704 frames | Maximum adjacent-frame mean RGB change 0.052898; hold-style difference 0 |
| Every directed scene pair | 552 pairings × 121 frames | Maximum change 0.050787; endpoint error 0.000000363; near-endpoint error 0.000077706 |
| New native bridges across screen shapes/themes | 126 combinations, 15,246 frames | Maximum change 0.050351; endpoint error 0.000001089 |
| Dense handoff regression | 27 pairings × four styles × 121 = 13,068 frames | Maximum change 0.042823 |
| Artifact, identity and instrument regression | 29,184 phase samples | Passed existing artifact gates |
| Stage lighting and output contracts | Existing 640-pattern/palette matrix and native display checks | Passed |

The bridge review caught a broad sweep during the Foundry/Monument quarter-turn. Its final choreography gathers the ribbons, turns the compact formation, then releases it. Cathedral also received a smaller transient-light response because its ribs occupy more screen area. The existing numerical limits were retained.

Synthetic musical fixtures exercise continuity and determinism; they do not establish real-song section-detection accuracy. Physical multi-display/Spout pacing and isolated 4K/120 qualification remain open.


## Native 4K timing

The seven-stage local release suite completed successfully, including 24 single-scene timing runs (960 samples each) and 36 representative handoffs (121 samples each). GPU: NVIDIA GeForce RTX 4070 SUPER, driver 32.0.15.8157. One older v0.17 Stage remained active; it was not terminated by validation. This is a competing-workload measurement, not an isolated hardware qualification or a direct comparison with the v0.20 timings.

| Workload | Mean GPU ms | P99 GPU ms | Maximum GPU ms |
|---|---:|---:|---:|
| Folding Cathedral | 1.6772 | 3.3260 | 4.2608 |
| Ribbon Racetrack | 4.0355 | 6.0095 | 6.8617 |
| Kinetic Monument | 3.9462 | 5.7907 | 6.6782 |
| Slowest average tested handoff: Chroma Blades → Kinetic Canopy | 6.5666 | 8.3903 | 8.9385 |
| Highest P99 tested handoff: Pulse Causeway → Ribbon Foundry | 6.3594 | 8.7071 | 8.7261 |

All individual scene P99 values were at most 6.1348 ms. Some handoff samples exceeded the 8.3333 ms budget for 120 fps. Timings include the scene, bloom and composite draws; they exclude physical presentation pacing, live audio analysis, Spout transfer and mirror output overhead. **4K/120 remains unqualified.** The runtime cost model is a routing hint, not admission control or a guaranteed deadline.

The portable build includes only the five current source-keyed shader cache entries. The preview is a 66-second, 960×540, 60 fps render using synthetic 120 BPM accents, with four-beat scene handoffs. It has no audio track and does not demonstrate song-recognition accuracy.

A final CPU-only correction makes returning from network theme following interpolate from the last received shape rather than snapping to its theme's target. The final binary passes that resume/settling contract, the other Composer contracts, protocol serialization and malformed packet/cache checks. Rendering code and the five validated shader keys are unchanged by that correction.

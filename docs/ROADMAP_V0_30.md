# RareStage roadmap: v0.21.0 to v0.30.0

Status: revised after the combined v0.21 implementation, September 8, 2026. “v30” means v0.30.0 in this roadmap. These are planned capabilities and acceptance criteria, not completed features or calendar commitments.

## The destination

By v0.30.0, Smart Director should compose a coherent live performance: establish a visual theme, develop it across worlds, rehearse several possible musical outcomes, commit on confirmed evidence, and evaluate the resulting composition. Scene changes should preserve the movement and intention of the outgoing scene.

The runtime remains deterministic and local, with no model inference or token consumption. The Director cannot know unheard music. Its intelligence comes from authored choreography, musical evidence, conditional planning, rehearsal, composition checks and memory.

The performance has four levels:

- **Set:** pacing, recurring themes, contrast, identity frequency and visual callbacks over several minutes.
- **Phrase:** motif development and conditional routes over roughly 8–32 bars, adjusted when musical evidence changes.
- **Beat:** coordinated gestures, accents and prepared releases over 1–8 beats.
- **Frame:** continuous movement, restrained lighting response and the output deadline.

## Starting point: v0.19.0

The v0.19 build has 21 scenes, a next/follow-up route scorer, recent-scene and entry-motif memory, an eight-beat rehearsal lock, shared wall motion, category-aware material handoffs and synchronized local display mirrors. Local validation passed 420 ordered scene pairs and 23,040 artifact/identity/instrument frames.

4K/120 remains unqualified. The heaviest measured handoff reached 12.9126 ms with two older renderers concurrently active. Establishing an isolated, reproducible baseline is the first milestone; that result cannot be treated as an isolated performance measurement. See [v0.19 validation](SCULPTURES_SHOW_DIRECTION.md).

## Work brought forward into v0.20

The user's combined-update request brings selected parts of the choreography, set-memory and identity milestones into **Performance Worlds**. It rebuilds nine existing scenes, adds four stable arrangements, eight identity entrance and exit programs, remembers complete program/identity combinations and scene pairings, and separates Pixel Bank's travel from beat corrections. Kinetic Canopy now explores suspended pendulum movement; Resonance Range replaces Wavewall with opaque contour terrain.

These implementations do not close the entire later milestones. There is no new predictive AI, musical-corpus accuracy claim, live composition evaluator or runtime GPU rehearsal. Set-memory validation uses deterministic simulated song chapters. Native bridges and physical multi-output frame pacing still require further work. See [v0.20 details](PERFORMANCE_WORLDS.md).

## Work brought forward into v0.21

**Show Composer** implements the shared catalog, Folding Cathedral, 48 directed native bridges, three conditional continuations, bounded analytic route estimates, measured scene GPU-cost feedback, six recurring deformation/accent themes, Hold Theme / Next Theme controls, Ribbon Racetrack and Kinetic Monument. This combines the practical core of 0.21 with portions of 0.24, 0.25, 0.27 and 0.28.

The shared catalog covers names, operator keys, geometry families, anchors, nominal pace/scale, density, energy, bridge group and relative cost. It is used by CPU planning and GPU handoff logic. Rich authored pose libraries, full symmetry/identity-surface contracts and universal mesh correspondence remain open. Route rehearsal estimates nominal movement; it does not inspect candidate images. Theme memory is session-local and balances six themes rather than inferring narrative meaning.

These are completed implementation slices, not completion claims for every acceptance criterion below. The remaining milestones retain their original numbers for traceability. See [Show Composer](SHOW_COMPOSER.md) for exact behavior and evidence.

## Release sequence

| Release | Main milestone | What the audience or operator should notice |
|---|---|---|
| **0.20** | Performance Worlds, identity choreography and session memory | Nine scene rebuilds, more arrangements, varied identity entrances/exits and smooth Pixel Bank departures. Performance measurement continues; isolated qualification remains open. |
| **0.21** | **Implemented: Show Composer combined update** | Shared wall/ribbon bridges, three new scenes, conditional branches, recurring themes, theme controls and route cost estimates. |
| **0.22** | Musical choreography programs | Patterns establish, repeat, answer and resolve. Pendulum Hall and Resonance Terrain extend the repertoire. |
| **0.23** | A stronger musical timeline | The Director distinguishes sustained groove, rising tension, release and uncertainty more reliably. |
| **0.24** | Conditional show planning | Builds have prepared continuations for a drop, a continued build or a breakdown. |
| **0.25** | Internal rehearsal | Candidate routes are checked before appearing live. Ribbon Racetrack exercises continuous spatial journeys. |
| **0.26** | Composition feedback | The Director rejects or gently corrects unintended flicker, broken formations and overloaded compositions. |
| **0.27** | Set memory and visual callbacks | A show develops recognizable themes over time. Kinetic Monument and Tension Loom add new interpretations. |
| **0.28** | Clear operator direction and identity choreography | Operators can steer intention; the logo/name follows a planned role within the world. Prism Procession completes the proposed scene set. |
| **0.29** | Output performance and multi-screen hardening | Quality, frame pacing and synchronized mirrors behave predictably on a declared hardware setup. |
| **0.30** | A validated show-composer release | The complete system demonstrates coherent long sets, dependable recovery and documented performance limits. |

## v0.20 — Stabilize and measure

Establish an isolated benchmark with one renderer, followed by realistic main-output, mirror and Spout configurations. Measure frame-time distributions, presentation pacing, GPU memory and startup/shader preparation. Profile the most expensive paths before choosing optimizations; investigate scene-specific shaders and shared rendering work where the measurements justify them.

Preserve Stagewall as the movement reference. Audit tier recycling, thin edges, identity recovery and simultaneous pattern/scene changes. Make duplicate running versions visible to the operator without silently closing another show.

**Exit:** reproducible baseline and hardware record; existing regression gates pass; identified expensive paths have measured fixes or explicit supported-quality limits. A benchmark's successful exit must never be presented as proof of 120 fps.

## v0.21 — Shared movement rules

**Current status:** Implemented core in v0.21: shared catalog, seven-wall lattice and three-scene ribbon rig. Extend native bridges and richer authored pose contracts next.

Create one scene description for geometry family, focal anchor, flow direction, velocity, scale, symmetry, density, identity surfaces and available entrance/exit poses. Use that description consistently in planning, rendering, diagnostics and tests.

Author native bridges for the most useful scene pairings. Match position and velocity at the join; constrain acceleration where practical. Give incompatible shapes an authored intermediate pose. Keep the existing material transport as a controlled fallback while native bridges expand.

Add **Folding Cathedral**: Stagewall rows stretch into ribs, then unfold into architectural wings and corridors.

**Exit:** representative bridges preserve momentum through entry and exit, pass endpoint tests, and remain readable at landscape, portrait and wide aspect ratios. Every fallback route is explicitly identified and tested.

## v0.22 — Choreography programs

Turn motifs into reusable musical gestures: row travel, column lifts, opposing banks, fans, rotations, convergence, expansion, suspension and release. Compose them into programs with a readable hold, an answer, a development and a resolution.

Allow the Director to extend or vary a program at safe musical boundaries. Keep base movement alive during weak or missing percussion. Separate sustained structural movement from short lighting accents.

Add **Pendulum Hall** and **Resonance Terrain**. Their patterns must have distinct silhouettes and movement, rather than differing only in color or speed.

**Exit:** each program remains identifiable with audio muted, aligns with the musical clock when audio is present, and can extend or exit without a motion reset. Preserve the favorite Stagewall/Cascade behavior through comparison captures.

## v0.23 — Musical evidence and uncertainty

Improve the musical timeline using sustained energy, rhythmic density, repetition, change evidence and section confidence. Keep beat, bar and phrase estimates distinct. A stable groove should support development without falsely announcing a new chapter on every fill.

Evaluate with a varied, legally available listening corpus and recorded feature traces: melodic material, bass-heavy material, steady grooves, half/double-time ambiguity, quiet passages, breakdowns and live transitions. Separate tuning material from held-out evaluation material.

**Exit:** publish section/cue precision, missed events, timing error and false switches per minute against the v0.19 baseline. Promotion requires improvement on held-out material and listening review. When uncertain, the Director visibly maintains a coherent program rather than repeatedly changing its mind.

## v0.24 — Conditional show planning

**Current status:** Implemented core in v0.21: three section-dependent branches, expiry and intervention handling. Longer hierarchical program planning and corpus replay evaluation remain.

Build a hierarchy of set theme, phrase program and immediate gesture. During a build, prepare compatible branches for continued tension, a confirmed drop and a breakdown. Branches share enough initial movement that selecting one does not require a cut or a clock reset.

Add expiration rules, confidence requirements, interruption handling and a budget for major visual changes. Manual scene/theme holds take precedence. A confirmed musical event may supersede a rehearsal lock through a compatible continuation.

**Exit:** replay tests cover correct forecasts, wrong forecasts, late drops, missed drops and operator intervention. No repeated mid-phrase plan churn; planned branches preserve animation continuity and do not bypass hold controls.

## v0.25 — Internal rehearsal

**Current status:** Implemented first stage in v0.21: nominal motion-envelope estimates and measured per-scene GPU costs. Offscreen image preflight, composition analysis and broader budget scheduling remain.

Evaluate a small bounded set of possible continuations using inexpensive movement/composition estimates. When GPU time is available, preflight selected routes with small offscreen renders through production scene logic.

Score continuity, musical suitability, readability, repetition, composition and estimated cost. Rehearsal is asynchronous, cancellable and limited by the live output budget. Results are tagged with their scene, plan and audio-context revisions; stale results cannot replace the current plan.

Add **Ribbon Racetrack**, connecting perspective travel with broad looping structures.

**Exit:** rehearsal catches known bad candidate routes and improves blinded comparisons against the planner without rehearsal. Under load, rehearsal work is dropped before it delays live output. Its benefit and overhead are both measured.

## v0.26 — Evaluate the actual composition

Add checks for unintended luminance jumps, motion discontinuities, disconnected formations, identity readability, focal competition and scene cost. Compare observations to the intended composition: a spacious breakdown and deliberate silhouette are valid outcomes.

Respond through bounded actions: reduce a detail layer, extend a hold, postpone a reveal or choose a rehearsed continuation. Use hysteresis and cooldowns so corrective behavior cannot oscillate between opposing decisions. Severe failures use a known continuous fallback.

**Exit:** seeded artifact fixtures are detected while intentional negative space and authored accents remain valid. Publish false positives as well as detections. Verify the correction itself creates no larger discontinuity.

## v0.27 — Set memory and callbacks

**Current status:** Implemented first stage in v0.21: six recurring deformation/accent themes and Kinetic Monument. Long-term narrative structure, richer callbacks and Tension Loom remain.

Remember scene/motif/palette combinations, recent transitions, intensity peaks and theme development across the set. Carry a visual theme through different geometry families using shared direction, symmetry, pace and accent structure.

Use repetition deliberately: establish a motif, let it rest, then return with a stronger or transformed version. Reserve high-impact combinations for meaningful moments. Optional operator feedback can adjust explicit preferences; it must be inspectable and resettable.

Add **Kinetic Monument** and **Tension Loom**.

**Exit:** extended-set comparisons show recognizable callbacks, fewer accidental repeated combinations and controlled intensity pacing. New themes remain possible; memory must not trap the show in a small favorite subset.

## v0.28 — Operator intention and identity

Expose a small set of meaningful controls: energy, variety, patience, keep this scene, hold this theme and allow the next chapter. Show the current intention and prepared continuation in plain language.

Give identity its own choreography role: introduce, assemble, hold, fragment and release through scene materials. Protect readability and spacing, and avoid revealing it so often that it becomes a persistent watermark. Preserve the logo treatment the user already likes.

Add **Prism Procession**. Export reproducible rehearsals with build version, seeds, cues, relevant feature traces and settings; replay guarantees apply to the matching build and recorded inputs.

**Exit:** operator controls have predictable precedence and clear recovery paths. Scene-native identity passes portrait/wide tests and listening review. The complete proposed catalog reaches 28 worlds only if all seven additions meet the same quality bar.

## v0.29 — Output and frame pacing

Finish measured renderer optimization, resource preparation, mirror presentation and quality management. Include actual output copies and presentation in testing. Quality adjustments should preserve major geometry, identity and timing, and should avoid visible resolution or detail oscillation.

Exercise mixed-resolution displays, mismatched refresh rates, fullscreen changes, monitor reconnects, Spout receivers, audio-device changes and GPU pressure. Local mirrors should share the same source frame; this does not imply hardware genlock across displays.

**Exit:** publish a supported hardware/output matrix. For any setup advertised as 4K/120, verify the 8.333 ms frame budget and measured presentation behavior in that actual configuration, including transitions and enabled outputs. Unsupported combinations receive an explicit quality/performance recommendation.

## v0.30 — Release qualification

Run the complete composer through extended real-music sessions and reproducible traces. Compare it with v0.19 on the same inputs: musical fit, continuity, repetition, theme development, readability, operator intervention and frame pacing.

Complete failure recovery for missing audio, stale rehearsal results, display changes and renderer/device errors. Document what recovers automatically and what needs operator action. Package a matching Stage/Console pair, migration notes, rehearsals, benchmark evidence and a practical operator guide.

**Exit:** a 60-minute soak for each declared output configuration with no crashes or unplanned blank frames; regression coverage for every ordered scene pair plus native-resolution inspection of high-risk routes; demonstrated recovery behavior; and a reviewed performance/quality report. No blanket 4K/120 or autonomous-directing claim based solely on synthetic fixtures.

## Delivery rules

1. Each release needs a visible improvement and a short comparison reel. Architecture work earns its place by enabling an observable result.
2. Performance work begins in v0.20 and continues throughout; v0.29 qualifies the integrated output system.
3. New scenes demonstrate the evolving choreography system. A scene that fails the quality bar stays out of automatic selection until repaired.
4. Keep movement, identity, operator holds and output timing as separate responsibilities with explicit coordination.
5. Review milestone scope after measurement. Calendar estimates follow the v0.20 baseline and the v0.23 evaluation corpus; version numbers are sequencing, not deadlines.
6. Record completed work and measured acceptance results against this plan.

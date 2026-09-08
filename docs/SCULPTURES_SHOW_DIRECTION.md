# RareStage 0.19.0 — Sculptures + Show Direction

## Implemented behavior

The catalog grows from 18 to 21 scenes. All earlier scene IDs are preserved. The Console cue grid adds Ribbon Foundry (18 / Z), Halo Engine (19 / X), and Vector Speedway (20 / J). They use bounded analytic shading with no ray marching, simulation buffers, prerecorded video, inference or network requests.

Pulse Causeway now renders eight depth-sorted tiers. Tier attributes derive from continuous depth, so recycling does not exchange the compositing order of visible banks. Geometry moves from tempo phase; transient envelopes are confined to restrained light accents. Resolution-aware edge widths reduce distant shimmer. This is a production shader change, not a motion-blur filter.

Scene descriptors separate operator category from geometry: surface, ribbon, perspective, radial and atmosphere. Prism Atrium and Pulse Causeway now belong to Depth / Transport; Kinetic Canopy belongs to Ribbon / Glass. Halo Engine shares the atmospheric operator category with Orbital Garden, but the handoff planner knows both have radial geometry.

Wall pairs retain a shared deforming lattice. Mixed wall pairs transport material through the wall's opening/closing surface. Non-wall pairs align scene anchors and share an interpolated geometry-specific deformation: ribbon waves, radial rotation or perspective expansion. These are procedural material morphs, not a claim that arbitrary meshes have matching vertices. At the terminal opening pose, antialiased wall faces reach full coverage regardless of output resolution; this prevents translucent edge gaps from surviving into the destination.

## Show direction

The route scorer considers both the next scene and a plausible follow-up. It uses musical fit, density, energy trend, geometric compatibility, recent scene visits and consecutive visits to a geometry family. After developing one geometry family it favors a change of shape. Builds reserve peak intensity, while breakdowns favor less crowded compositions. The second scene is a conditional continuation, not a prediction of unheard music.

A route can be reconsidered on a 16-beat planning boundary. Hysteresis requires a meaningful alternative; the final eight beats before the dwell cue lock the rehearsal. Confirmed chapter changes and operator category changes invalidate stale plans. Timed changes use an eight-beat boundary, while qualified confirmed chapters retain next-beat response. Existing minimum dwell, freeze, queued-cue and manual controls remain in force.

Modern motif programs use establish/repeat/answer/develop progressions. Each scene remembers its entry-motif usage for the current session. The pattern morph occupies roughly the latter four beats of each eight-beat block; repeated motifs receive longer uninterrupted holds. The selected program stays fixed during a visit; audio hits do not replace either endpoint of an in-flight pattern blend. The Console reports the rehearsed route in its Director reason text.

This is deterministic local show-control software, not AGI. Its section classifier can misread ambiguous live mixes. It has no advance access to the track and does not guarantee a forecast drop will occur at the predicted beat.

## Reference study

These are original analytic scenes; no artist footage, logos, copyrighted stage assets or prerecorded loops were incorporated.

- [Reza Ali's deadmau5 production account](https://www.syedrezaali.com/work/deadmau5/) describes live audio-reactive masks and stage-wide mapping that adapted to a changing set. The relevant design lesson here is to give geometry continuous motion and use audio as a controlled expressive layer.
- [SENSORED's REZZ Spiral Tour case study](https://www.sensored.xyz/projects/rezz) documents an immersive stage visualization built around the tour's visual identity. Halo Engine applies an original radial/iris vocabulary; it does not reproduce that tour's visuals.
- [LEVITATE's Marshmello portfolio](https://www.levitate.design/) describes bespoke assets, environments and cinematic contributions. That supports treating each world as a coherent scene with its own shape language instead of changing only a wall texture.

The named Maths, Infra Turbo Pigcart Racer, Slushii and Ray Volpe references informed the requested direction. This review does not claim frame-by-frame analysis of those complete performances.

## Validation

All six stages of the final combined local release suite returned exit code 0. The suite uses the actual production scene, bloom, transport and output shaders.

- 500 route decisions across all five category settings; route legality, follow-up validity, variety, repeated-family contrast, rehearsal locking and filter invalidation.
- 420 ordered scene pairs × 121 frames at 320×180, including exact and near endpoints. Adjacent RGB change limit 0.06; exact endpoint difference 0.00001; near-endpoint difference 0.003.
- 1,920 frames per new sculpture and Pulse Causeway at 60 fps: autonomous motion without hits, maximum frame change and hit-only brightness influence.
- 23,040 artifact/identity/instrument frames at 1280×720, plus 9,680 dense handoff frames across 20 pairs and four paths.
- Existing fullscreen, mirrored output, corner arrival, audio, motion, section prediction, production and control-protocol contracts. The Console fixture also checks all 21 scene buttons and captures the expanded cue grid.
- Full-resolution GPU timing for all 21 scenes and 20 representative handoffs. These measurements exclude presentation, mirrors, audio and Spout overhead.

## Operator notes

Use the matching v0.19.0 Stage/Console pair (shared control v15, network v3). Use Screens for synchronized local mirrors; different physical display refresh rates are not hardware genlock. Select the highest-resolution output as the main Stage if mirrors need a 4K source.

Close older Stage/Console pairs before launching the new package to avoid duplicate audio capture and competing GPU workloads. The local validation run left the user's existing v0.17.0 and v0.18.0 Stage processes running.

## Final local measurements

Artifact sweep: **23,040 frames passed**. Minimum wall luma 0.141011; minimum active coverage 0.634757; maximum silent wall change 0.055837.

All **420 ordered pairs passed**: maximum adjacent RGB change 0.048664; exact endpoint difference 0; maximum near-endpoint difference 0.00008388. The dense handoff sweep passed **9,680 frames**, maximum change 0.042823.

| Scene | Maximum silent frame change | Mean ongoing motion | Mean hit-only change |
|---|---:|---:|---:|
| Pulse Causeway | 0.00669608 | 0.00430721 | 0.0251173 |
| Ribbon Foundry | 0.00878831 | 0.00343538 | 0.00573908 |
| Halo Engine | 0.00190959 | 0.00108772 | 0.00206845 |
| Vector Speedway | 0.00255447 | 0.00193927 | 0.00528357 |

GPU measurements used 960 timed samples per scene and 121 per handoff, after warmup, at native 3840×2160. The user’s v0.17.0 and v0.18.0 renderers remained active; this is a competing-workload measurement, not an isolated hardware certification.

| GPU case | Mean | P99 | Maximum |
|---|---:|---:|---:|
| Heaviest individual scene: Ribbon Foundry | 5.1102 ms | 8.16435 ms | 10.4509 ms |
| Heaviest handoff: Pulse Causeway → Ribbon Foundry | 8.94357 ms | 12.4906 ms | 12.9126 ms |

**4K/120 is not certified by this run.** The 120 fps budget is 8.333 ms, and the heaviest measured handoff exceeded it. Timing covers the scene, transport, bloom and composite passes; mirror copies, Spout, audio, CPU work and physical presentation are additional. The benchmark exit code confirms valid complete measurements, not compliance with the 120 fps budget.

Audio, motion, section-prediction, production, club-loop and protocol checks passed. The Console screen/21-world-grid fixture passed and was visually inspected. The 40-second, 960×540 preview is a deterministic render fixture with synthetic 128 BPM instrument envelopes; it is not a live-song recognition test.

# RareStage v0.22.0 — Phrase Conductor

The eight rebuilt non-wall ensembles now receive movement decisions from a deterministic phrase conductor. Their shapes remain distinct: prism streams, scissoring blades, folded fins, rotating square mechanisms, segmented rotors and broad ribbon structures. The catalog stays at 24 scenes. Stagewall and Panel Cascade retain their existing movement programs.

## Musical behavior

Each supported scene has six named movement programs. The conductor establishes a formation, holds the groove, gives it a related answer, develops its articulation, and resolves. Its decisions occur when the persistent musical clock crosses an eight-beat boundary. A scene first receives at least 16 beats to establish itself. An ordinary stable groove receives two hold decisions before an answer. An identity reveal or scene handoff can extend that hold.

An authored mode change takes eight beats, using a quintic pose blend with zero blend velocity at its endpoints. The musical clock continues throughout; a decision does not replace the clock or multiply its phase by a new speed. Gain, spacing and the opposing-bank answer also interpolate. Muting percussion preserves the structural movement. Hits mainly light the existing edges and inlays.

The decision layer consumes the existing section detector. A build or breakdown needs section confidence of at least 0.30 and four beats of sustained section age. A confirmed drop may develop the formation at the next eligible boundary. A short fill without that section evidence does not replace it. Departures can request a resolving program during the final 16 beats. Existing automatic scene dwell and musical cue rules remain in force.

This is a bounded authored policy, not an inference model, learned song understanding, or prediction of unheard audio. Weak tempo evidence can still require the existing fallback clock. There is no new claim of section-detection accuracy.

## Program and identity memory

Session memory counts scene/program/action combinations and penalizes recently used formations. Answers prefer a related formation within the family; development explores alternatives; resolution prefers the family's returning poses. Stronger drop articulation has a 96-beat cooldown shared across scene visits. It also requires a new confirmed section revision, so a continuing drop does not repeatedly trigger the peak.

Identity decisions retain the existing eight entrance and eight exit programs. The global spacing policy alternates the Rare mark and DJ name, waits at least 16 scene beats, requires a settled program, avoids builds/drops and imminent automatic exits, and spaces reveals by at least 64 beats across world changes. Once chosen, the target stays fixed for the stanza. Existing material-mask identity rendering remains; this release does not implement universal geometry-to-letter correspondence.

Automation Hold prevents further conductor decisions while allowing an in-progress blend to finish and base movement to continue. Manual scene changes establish the selected scene normally. The Console shows the current formation name and musical intention instead of a generic pattern number in supported scenes.

## Blades and Canopy handoffs

Chroma Blades and Kinetic Canopy now use one bank of eighteen material pieces: two opposing sets of nine. Their shared scene pass interpolates each piece's position, angle, extent, taper, skew and facet appearance. A blade folds into a fin, and a fin broadens back into a blade. The outgoing and incoming phrase states are evaluated separately against the same moving musical clock.

Automatic bank-to-bank handoffs take eight beats. Manual transition duration still follows the operator's setting. The new two directed bridges bring native geometry coverage to 50 ordered pairs. The other 502 pairs retain category-aware HDR material transport; they do not have universal shared topology.

Outgoing conductor state is preserved during all handoffs. The source pass receives its own phrase state, including an unfinished blend, rather than adopting the new scene's program. The ribbon rig similarly evaluates both phrase states during its native bridges.

## Implementation and compatibility

- `phrase_conductor.h`: musical policy, family names, bounded frame state, program memory and identity spacing.
- `phrase_shader.h`: common shader access to current/outgoing program state, with the prior authored cycle retained for fixed regression fixtures.
- `bank_worlds.h`: shared Blades–Canopy material geometry.
- Production constant buffer: 288 bytes; current/outgoing phrase state occupies 64 bytes after the existing 224-byte layout.
- Matching Console protocol v17 uses its own shared-memory endpoint. Scene IDs, keys and saved selection meanings are unchanged.
- UDP v5 is 160 bytes and carries current/outgoing conductor samples and pattern seeds in addition to existing show state. Values are checked for finiteness and bounds. Peers must match. Network return preserves the received pose before resuming local decisions.
- Local display mirrors receive the same finished frame. UDP remains show-state synchronization, not remote frame-perfect video or display genlock.

## Validation

The new production-render fixture covers 192 beats at 120 BPM and 60 samples per second for each of eight scenes in landscape, portrait and wide formats. Its programs respond to scripted groove, build, drop and breakdown sections with percussion muted. Thresholds require mean adjacent-frame RGB change below 0.04, ongoing mean motion above 0.0001, foreground coverage above 25%, and at least 10% in every quadrant. These occupation checks apply to the deliberately filled ensembles, not all artistic compositions.

| Check | Result |
|---|---|
| Phrase/aspect sequences | 24; 138,240 rendered frames |
| Largest mean adjacent-frame RGB change | 0.0118763 |
| Minimum foreground occupation | 35.11% |
| Least occupied quadrant | 15.61% |
| Bank handoff combinations | 216; all 6 × 6 source/target programs, both directions, three aspects |
| Bank handoff frames | 26,136 |
| Largest bank handoff change | 0.048863, below the unchanged 0.06 gate |
| Largest endpoint / near-endpoint error | 0.000000726216 / 0.00000266279 |
| Favorite-scene comparison | Eight Stagewall/Panel Cascade arrangement captures byte-identical to v0.21.1 |
| Conductor contracts | Holds, sparse-frame boundaries, short fills, drop spacing, identity spacing, network return and 20 simulated songs |

The bank fixture uses a two-second handoff with different outgoing/incoming blend positions, arrangements, gains and answer directions. It does not qualify every user-selectable manual duration or every display refresh rate. The RGB metric is a screen-wide mean, not a perceptual proof that every edge is artifact-free.

Five local music clips totaling 252.37 seconds also passed through the production FFT analyzer, the exact live tempo-advance function, the production section detector and the conductor on sample time. They produced 19 program changes; the minimum observed separation was 15.9983 beats. All frame states remained valid. A second replay produced identical CSV traces and summary. These are short, unlabeled demos, not complete songs or a held-out musical-accuracy benchmark. Beat confidence was weak on some clips; the existing fallback clock remained active. Source audio is not distributed with the validation package.

The review reel is 96 seconds at 960 × 540 / 60 fps. Each chapter includes a conductor pre-roll so the compact reel demonstrates answers and development as well as entry. Lighting accents are synthetic; the reel has no soundtrack.

The existing six rendering suites also pass: 1,536 arrangement samples, 192 identity sequences (46,272 frames), all 552 ordered scene pairs, 126 native bridge/theme/aspect combinations, 13,068 dense handoff frames, the 29,184 artifact/identity/instrument samples, stage-lighting/output contracts, and 9,216 ensemble composition samples. All seven previous CPU suites, the new conductor tests, protocol tests, Console controls and Console-owned renderer startup/control/shutdown pass. The identity scheduling fixture now explicitly starts an eligible reveal, checks target latching, verifies cross-scene spacing, and checks the next alternating target; it no longer assumes a reveal every other stanza.

The release package includes detailed new and existing regression CSVs and status records. Native 4K timing is measured separately. 4K/120 qualification, live image-quality feedback, full-set listening review, and broad physical-output qualification remain open roadmap work.


## Native 4K timing

The final local measurement uses an RTX 4070 SUPER, driver 32.0.15.8157, with one older v0.17 Stage instance still running. It is not an isolated comparison against v0.21.1. Both the steady-scene and handoff benchmarks bind active conductor state for the new scene paths. Each of 24 scenes has 960 valid GPU samples; each of 36 handoffs has 121.

The slowest scene average was Chroma Blades at 11.4163 ms, with P99 15.9914 ms. The slowest handoff average was Pulse Causeway to Ribbon Foundry at 15.7613 ms. The highest handoff P99 was 23.0950 ms, for Pulse Causeway to Ribbon Foundry. These exceed the 8.333 ms target in some cases; **4K/120 remains unqualified**.

An initial timing attempt ended with an incomplete table. The harness now reports rejected/skipped timestamp counts and permits one whole-window retry while still requiring the full sample count. The final attempt completed with no retries. Timing includes scene, bloom and composite, but excludes physical presentation, audio analysis, mirrors and Spout. The next performance milestone needs an isolated baseline and profiling of expensive scene paths and handoffs; these figures are not an FPS guarantee.

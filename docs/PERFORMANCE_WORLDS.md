# RareStage 0.20.0 — Performance Worlds

## Combined implementation

This release brings selected choreography, session-memory and identity work forward from the roadmap. It retains 21 scene IDs and rebuilds nine of their visual designs. It does not claim completion of every milestone through v0.30.

| Scene | New direction |
|---|---|
| Prism Cascade | Broad folded prism ribbons with bank movement and travelling facet light |
| Chroma Blades | Long solid fins unfold between horizontal racks and a large radial fan |
| Kinetic Canopy | Suspended beams with pendulum phrases, a supporting truss and travelling light |
| Signal Rail | Solid perspective lanes and repeating lane sequences with two-beat offsets |
| Matrix Vault | Opaque nested rectangular portal slabs with depth-sorted recycling |
| Void Gates | A radial iris with distinct segments and four petal-count arrangements |
| Aurora Range | Five broader light curtains, reflected light and layered foreground contours |
| Resonance Range, formerly Wavewall | Ten opaque contour terraces with continuously developing wave shapes |
| Pixel Bank | Continuous mark travel, a musical circuit field and planned corner departures |

Stagewall remains the motion reference. Four stable arrangements extend its cube sizes and row/column proportions. Non-wall worlds use wider, tilted or compressed compositions. Sixteen motif seeds develop through the existing establish/repeat/answer/develop grammar; these are program combinations, not a claim of 1,344 wholly independent animations. Orbital Garden retains its environmental orbit animation with four framing arrangements.

## Identity choreography

The Rare mark and customizable DJ name use eight entrance and eight exit programs: rows, columns, orbital assembly, expansion, diagonal fragments, folding, weaving and separated halves. Programs remain latched through the appearance. The exit never replaces the material finish at the readable hold. During a scene handoff, the active identity program is retained as its weight releases.

Wall identities continue to form in the surface pixels. Non-wall identities use visibly segmented luminous strips and chips. Split exits restrict each half to its original source region, preventing duplicated words. Pixel Bank retains the full-color Rare logo as its travelling mark.

## Motion and set memory

Pixel Bank uses elapsed seconds, independent of the beat-phase correction loop. A forecast with at least 1.5 seconds of lead may guide a corner arrival; an unexpected drop changes background light only. Local manual and scheduled scene departures queue a smooth corner approach before committing the new scene. Automatic hold delays a queued automatic departure. Network follower mode cancels a local queued departure and follows the leader's scene timing.

The approach blends the continuing orbit into an exact corner using a quintic curve. Position stays within the screen bounds. Terminal velocity and acceleration reach zero, followed by a smooth relaunch when the corner hold is released. The corner timing is based on the tempo estimate at planning time; there is no advance access to unheard audio or guaranteed prediction of a drop.

Session memory tracks scene exposure, directed scene pairings, 64 arrangement/motif combinations per scene, palette usage and 64 entrance/exit pairings per identity type. It balances underused choices while retaining section and geometry preferences. It resets when Stage restarts. This is deterministic bookkeeping and authored planning, with no model inference, online learning or token consumption.

## Startup and output

The portable package includes compiled DXBC for the five shader entry points. Cache keys include the embedded source, entry point, profile and compiler flags. Length, header and payload hash checks reject missing, stale or damaged files; Stage then compiles its embedded source and caches the result. A checksum is a corruption check, not a signature. Local display mirrors still copy the same completed frame. Console protocol remains v15 and the UDP packet remains v3.

## Validation scope

The native suite exercises 1,344 scene/program renders and 96 identity sequences, each with 241 frames, plus same-time hold comparisons. The scene handoff test covers all 420 ordered pairs while also changing the arrangement bank. Existing wall artifact, instrument-response, dense handoff, fullscreen and mirror contracts remain enabled.

CPU tests cover all 64 program combinations before reuse, all 64 identity pairings before reuse, smooth corner arrivals at 60/120/144 Hz, bounded speed and acceleration, cancellation continuity, and unexpected drops that leave the path unchanged. A deterministic 20-song simulation evaluates scene and pairing variety. This simulation is not a test of live song-section recognition.

The 66-second preview renders the production shaders with synthetic 128 BPM envelopes. It is silent and demonstrates the scene designs and identity choreography; it is not recorded artist footage or a music-classifier benchmark.

## Measurements

All seven stages of the complete local visual release suite returned exit code 0: performance worlds, lighting/output, professional routing, artifacts, dense continuity, world timing and handoff timing. Final package checks additionally exercise bytecode corruption rejection and cached production rendering. The complete cached startup, fullscreen/mirror and 640-case lighting sweep completed in **2.14 seconds**; the log confirmed all five shader entry points loaded from bytecode.

- 1,344 scene/program renders: minimum scene luma **0.086499**.
- 96 identity sequences / 23,136 frames: maximum adjacent RGB change **0.039098**; changing entrance to exit at full hold produced **zero** difference.
- 420 ordered scene pairs: maximum adjacent RGB change **0.051445**, exact endpoint error **0.0000003632**, near-endpoint error **0.00007372**.
- 23,040 wall/identity/instrument frames: minimum wall luma **0.141011**, minimum wall coverage **0.634757**, maximum silent change **0.055835**.
- 9,680 dense handoff frames: maximum change **0.042823**.
- CPU motion tests: peak normalized travel speed **0.4363** per second and acceleration **0.2760** per second squared across 60/120/144 Hz; exact corner arrivals; zero path difference under unexpected drop events.
- Twenty simulated song chapters / 160 cues: **21 scenes** and **156 distinct directed pairings**. This measures deterministic routing variety, not classifier accuracy on recorded songs.
- Control protocol, audio, motion, section prediction, production, club-loop, fullscreen, mirror and Console cue-grid checks passed.

GPU timing used an **NVIDIA GeForce RTX 4070 SUPER**, native **3840×2160**, with 960 timed samples per individual scene and 121 per handoff after warmup. The user's v0.17 and v0.18 Stage renderers remained active throughout. These measurements include competing GPU workloads.

| Measured case | Mean | P99 | Maximum |
|---|---:|---:|---:|
| Slowest individual scene: Kinetic Canopy | 6.7101 ms | 9.7045 ms | 10.5206 ms |
| Slowest of 29 tested handoffs: Chroma Blades → Kinetic Canopy | 10.5898 ms | 12.4989 ms | 12.8348 ms |

All 29 expanded handoff measurements are included in the package's validation CSV. **Native 4K/120 is not certified.** The 120 fps budget is 8.333 ms, and heavier measured handoffs exceed it. GPU timing covers scene rendering, material transport, bloom and composition. Audio, CPU work, Spout, mirror copies and physical presentation add overhead. An isolated hardware/output qualification remains open.

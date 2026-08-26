# RareStage stage-visual research, 2023–2026

This document records the production research behind RareStage 0.10.0. It extracts reusable show-design and real-time-system principles; it does not reproduce an artist's artwork, stage identity, logos, or signature animation.

## Recent show sample

| Production | Year | Production evidence | RareStage lesson |
|---|---:|---|---|
| Morgan Wallen, *One Night at a Time* | 2023–2024 | Raw Cereal and Disguise synchronized 4,000 LED tiles, multiple video surfaces, 21 cameras, 12K real-time Notch content, and frame-accurate multi-server output. [Disguise case study](https://www.disguise.one/en/insights/case-study/morgan-wallens-one-night-time-tour-pushes-visual-boundaries-disguise) | Treat the complete display as one stage system. Patterns must preserve hierarchy across every surface and every frame. |
| Linkin Park, *From Zero World Tour* | 2024 | STURDY. and All of it Now adapted abstraction to a 5K unwrapped cube and exposed selected real-time parameters to performance control. [Notch case study](https://www.notch.one/madewithnotch/linkin-park-from-zero) | Author a coherent system first, then expose a small number of musically meaningful controls. Do not map raw audio to every position. |
| Billie Eilish, *Hit Me Hard and Soft: The Tour* | 2024 | Moment Factory designed a continuously transforming 360-degree environment using a video floor, towers, moving overhead screens, lasers, a cube, fluid content, and deliberately contrasting glitch moments. [Moment Factory project](https://momentfactory.com/products/billie-eilish-hit-me-hard-and-soft-the-tour) | A show develops as a sequence of worlds and states. Fluid motion needs contrast and negative space, not constant maximal activity. |
| Anyma, *The End of Genesys at Sphere* | 2024–2025 | Sphere presented the show as a meticulously curated audio/visual work using its high-resolution LED canvas and concert audio system. [Sphere announcement](https://www.sphereentertainmentco.com/afterlife-presents-anyma-the-end-of-genesys/) | Build stage-scale compositions with readable depth and chapters. A strong world is more valuable than a collection of unrelated reactive effects. |
| deadmau5, *Retro5pective* | 2024 | Cube v3, a wide upstage wall, internal pixels, strobes behind blow-through video, note-level light hits, blackouts, and energy control were designed as one video-light composition. [CHAUVET Professional production story](https://chauvetprofessional.com/news/color-strike-m-fires-imagination-at-deadmau5-retro5pective-with-erik-mahowald-and-bending-lite/) | Preserve base motion and scene identity while using kicks, notes, and hats for short lighting punctuation. Darkness is an authored state, not a failure. |
| Linkin Park, *From Zero* launch and arenas | 2024–2025 | The same content language was remapped from transparent/ceiling LED to non-standard cube canvases while keeping live parameter control. [Notch case study](https://www.notch.one/madewithnotch/linkin-park-from-zero) | World logic should be resolution- and surface-independent. Procedural detail should remain coherent at HD, 4K, and non-standard aspect ratios. |
| Radiohead, 2025 tour | 2025 | The production generated the entire show live in Notch, drove twelve moving LED panels, adapted content to panel geometry, accepted multiple real-time control streams, and retained recallable track states. [Disguise case study](https://www.disguise.one/en/insights/case-study/radioheads-record-breaking-tour-put-visuals-round-disguise-and-notch) | Keep the engine live, but organize it into recallable authored states. Geometry, content, and lighting should transform together. |

## Design synthesis

### 1. Continuous system, discrete cues

Professional real-time visuals keep an autonomous visual clock running. Music does not start and stop animation. Instead:

- Continuous time drives flow, drift, atmosphere, slow rotation, depth, and idle choreography.
- Tempo phase selects authored pattern timing and phrase boundaries.
- Kick events create short pressure, depth, and luminance accents.
- Hi-hat and percussion events illuminate small edges, pixels, and specular details.
- Section changes alter the current world's layout, density, and palette before changing worlds.

### 2. Beat following without visual discontinuity

RareStage needs causal, live analysis; whole-track estimators are unsuitable. Essentia explicitly notes that its statistical whole-track rhythm extractor is not designed for real time. [Essentia rhythm documentation](https://essentia.upf.edu/tutorial_rhythm_beatdetection.html)

Recent online beat-tracking work demonstrates that low-latency beat and downbeat inference depends on both local evidence and retained context. BEAST reports a streaming architecture under 50 ms that carries musical context between blocks. [BEAST paper](https://arxiv.org/abs/2312.17156)

RareStage 0.10.0 therefore uses a lightweight causal design appropriate to the existing native renderer:

- Adaptive, multi-band spectral-flux evidence rather than one fixed threshold.
- Separate kick and high-percussion envelopes.
- A median of recent valid beat intervals for tempo stability.
- Confidence derived from interval consistency and accumulated evidence.
- A monotonic phase-slew loop. Beat evidence changes clock velocity briefly but never teleports the visual phase backward or forward.

This is deliberately smaller than a neural beat tracker, adds no runtime model dependency, and preserves the engine's high-refresh target.

### 3. Stagewall grammar

Stagewall is treated as an articulated facade, not a screen effect. Its cue vocabulary is:

1. Column chase
2. Diagonal chase
3. Center-out radial pressure
4. Checker drive
5. Organic sparse field
6. Helix flow
7. Split gate
8. Rare mosaic

Patterns hold long enough to read, then cross-morph over a controlled phrase window. Panels retain autonomous drift and turn between cues. Each panel also owns a synchronized micro-pixel matrix, allowing high-frequency percussion to add detail without shaking the complete wall.

### 4. Logo choreography

The Rare mark is not a mandatory overlay. It may be:

- A panel-state instruction distributed across a wall.
- Negative space cut into a light array.
- A gobo or shadow revealed by a beam.
- A temporary attractor that organizes moving pixels.
- Absent when the world is stronger without branding.

No 0.10.0 scene uses a repeated watermark stack.

### 5. High-refresh rendering budget

- Worlds remain branch-local in the single HDR scene shader.
- Procedural detail is resolution-independent and uses bounded loops only.
- Beat analysis remains CPU-light and causal.
- The half-resolution bloom and filmic composite remain unchanged.
- Immediate audio response changes depth and light; it does not trigger expensive scene rebuilds.

## 0.10.0 implementation map

| Research principle | Engine change |
|---|---|
| Continuous system, discrete cues | Independent wall/particle clocks plus tempo-phrased pattern states |
| Fast hits, stable motion | Separate kick/hat envelopes and monotonic phase slewing |
| Physical LED architecture | Expanded Stagewall plus Kinetic Array and Lightwell |
| Pixel choreography | Rotating panel sub-pixels and Pixel Loom group rotations |
| Restrained branding | Mosaic, negative-space, gobo, and attractor logo roles |
| Show-scale visual chapters | Smart Director affinity tables expanded to twelve worlds and five acts |

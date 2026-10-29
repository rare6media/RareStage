# RareStage v0.21.1 - Non-wall ensembles

This revision follows the direction that Stagewall is the movement-quality benchmark while the other worlds need different geometry, fuller compositions and their own recognizable motion. Scene count stays at 24; existing keys, saved selections, Console protocol v16 and network packet v4 are compatible.

## Visual changes

**Prism Cascade (7)** replaces vertical ribbons with three staggered streams of faceted hexagonal prisms. The center stream moves against the outer streams. Shared wave and tilt gestures coordinate the groups. Inverse row lookup carries the streams continuously beyond the screen; neighboring candidates cover the maximum rotated footprint.

**Chroma Blades (9)** uses twelve broad blades in two answering banks. Six continuous phrase poses form scissors, interlocking zippers, fans and travelling sweeps. Contrasting banks and illuminated inlays give the pattern a readable structure.

**Matrix Vault (R)** becomes a linked mechanism of nine different-size square frames with a separate inner square. The frames counter-rotate and vary their relative alignment over the phrase. Broad beveled faces remain visible between beats.

**Kinetic Canopy (W)** replaces eleven top-anchored bars with eighteen tapered, faceted fins in two opposing banks. Six authored pose programs create travelling waves, mirrored arches, braids, alternating sweeps and opening fans. Each program occupies 16 beats: the first half develops its pose; the second half interpolates to the next pose with a quintic curve. The global musical clock continues throughout. Four arrangements adjust the formation; no particle simulation or random hit selection is used.

**Halo Engine (X)** replaces thin torus outlines with three coordinated segmented rotors, each containing three broad faceted tiers. The central rotor and supporting wings counter-rotate. A shared phrase envelope changes their opening and sweep. Supporting rotors spread horizontally on landscape displays and vertically on portrait displays. Sector signals are periodic around the full circle to avoid a discontinuity at the angular seam.

**Ribbon Foundry (Z), Ribbon Racetrack (V) and Kinetic Monument (N)** have broader folded faces, an offset spine, stronger separation between palette colors and overlapping travelling waves. A recessed moving field supports the main geometry. Monument spreads its formation with display aspect ratio instead of remaining a small central cluster. Native ribbon bridges retain the gather, turn and release sequence; pose, arrangement and old/new gesture programs interpolate through the same geometry.

These are analytic 2D/2.5D surfaces, not a claim of new mesh-based 3D rendering. Supporting light fields share the foreground clock. Sustained pressure adds a small lighting contribution, and percussion accents edges. Neither changes the new ensembles' structural position, scale or rotation.

Scene density estimates were updated in the shared catalog for future route scoring. Matrix Vault now declares rotational movement, so the Director and material handoff use orbital flow rather than the previous depth-travel motion. Its operator category is preserved. This revision does not introduce new song detection, live image inspection or predictive AI.

## Validation design

The new `--ensemble-test` renders the production shaders at 320x180, 180x320 and 480x180. It samples each of eight revised scenes across four arrangements and 96 musical beats, totaling 9,216 compositions. Foreground occupation is measured above luma 0.23, so the dim supporting field cannot satisfy the check by itself. Each composition must cover more than 25% of the frame and more than 10% of every quadrant. Mean luminance must remain below 0.80. These checks apply to these eight intentionally full compositions, not to all scenes regardless of artistic intent.

The first pass caught Kinetic Monument covering only 20.2% of a wide frame. The final aspect-aware formation corrects that behavior. Numeric coverage does not establish artistic quality, so native captures and a moving preview are included for review.

Existing tests cover all 552 directed scene pairs, native ribbon/wall handoffs across screen shapes and themes, identity entrances/exits, continuous silent motion, restrained transient response, rendering artifacts and native GPU timings. Prism Cascade, Chroma Blades, Canopy and Matrix Vault are included in the long silent-motion check. Chroma Blades, Matrix Vault and Halo join the eight-style identity sequence check. Thresholds in the existing suites are unchanged.

## Review preview

The 96-second preview contains Prism Cascade, Chroma Blades, Kinetic Canopy, Matrix Vault, Halo Engine, Ribbon Foundry, Ribbon Racetrack and Kinetic Monument, ending in a handoff to Stagewall. It is rendered at 960x540 and 60 fps, with synthetic 120 BPM accents and no audio track. It demonstrates geometry and continuity, not real-song interpretation.

## Results

The final source passed the six local rendering checks, protocol tests, seven CPU suites and Console grid capture. Both executables report product version 0.21.1. The package includes the detailed CSVs and validation status records.

| Check | Coverage | Result |
|---|---|---|
| Full-field composition | 9,216 samples across eight scenes, four arrangements, three aspects and 96 beats | Minimum foreground occupation 34.28%; least-occupied quadrant 16.67% |
| Scene/program rendering | 1,536 renders | Minimum mean luma 0.086499 |
| Identity entrances/exits | 192 sequences, 46,272 frames | Maximum adjacent-frame mean RGB change 0.056329; held-style difference zero |
| All directed scene pairs | 552 pairs, 66,792 frames | Maximum change 0.050787; endpoint error 0.000000363 |
| Native bridges across themes/aspects | 126 combinations, 15,246 frames | Maximum change 0.042394 |
| Dense handoff regression | 13,068 frames | Maximum change 0.042823 |
| Sustained motion | Eleven scenes, 21,120 frames | Continuous silent motion and restrained hit response passed |
| Artifact/identity/instrument regression | 29,184 samples | Existing gates passed |

The denser ribbon surfaces initially made the Monument-to-Foundry release too quick in one stress fixture. Gathering and release now use allotted handoff time directly, and the opening extent is bounded by the visible display. The corrected source passes both the dense handoff suite and all ordered pairs without increasing the thresholds.

Native GPU timing is recorded separately for 24 single scenes and 36 handoffs in the package's validation folders. Those draws include scene, bloom and composite work; they exclude presentation pacing, live audio, mirrors and Spout transfer. The RTX 4070 SUPER system had older v0.17 and v0.21 renderers active at measurement start. These are competing-workload observations, not isolated comparisons or a hardware qualification. **4K/120 remains unqualified.**

The preview demonstrates the final rendered choreography with synthetic timing. Real-song detection accuracy and subjective artistic quality require separate evaluation.

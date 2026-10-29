# RareStage v0.23.0 — Kinetic Playgrounds

This update adds eight scenes, each with ten named programs, to a 32-scene catalog. It develops visual actions that can be followed through a phrase: exchanging rows and columns, completing a relay, opening and folding a bloom, tipping a chain, weaving banks and docking pieces. Musical hits illuminate the motion rather than displacing it.

## Reference study and design decisions

The review used creator and production case studies, not a claim of watching complete concert sets or measuring audience preference:

- [Cursor Video / UKF Future Vision](https://www.notch.one/madewithnotch/future-vision-ukf-live-stream): the production describes sustained geometric optical motifs and scene direction led by the structure of the music. The design takeaway here is a readable visual rule that develops over time.
- [STUDIO/ / Clean Bandit at the London Palladium](https://www.notch.one/madewithnotch/clean-bandit-london-palladium): the designers built a recognizable train journey and used perspective and transformation to develop it. The takeaway is an action with a setting and payoff rather than undirected shape variation.
- [Muzencab / Cuboid2](https://resolume.com/footage/Cuboid2): the creator describes adjacent symmetry that lets animation continue across cube faces. The takeaway is compatible motion and edge behavior across surfaces and scene changes.

The programs and renderer in this update are original procedural work. No third-party footage or audio is included. These references motivate design choices; they do not validate RareStage's musical accuracy or artistic quality.

## Scenes and programs

### Axis Puzzle

Row Then Column, Opposing Rows, Column Pairs, Checker Exchange, Center Zipper, Three Lane Lock, Staircase Shift, Cross Shuttle, Outer Ring Shift, Solve And Release.

### Sliding Shutters

Horizontal Banks, Vertical Banks, Opposing Shutters, Interlock, Travelling Slats, Center Open, Ladder Exchange, Cross Weave, Diagonal Steps, Full Frame Lock.

### Orbit Relay

Orbit Handoff, Figure Eight, Counter Rings, Rounded Circuit, Double Relay, Slingshot, Rendezvous, Orbit Exchange, Spiral Catch, Unison Lap.

### Iris Bloom

Petal Open, Double Bloom, Alternating Petals, Spiral Rosette, Breathing Wheel, Opposing Fans, Petal Zipper, Four Point Bloom, Inverted Iris, Bloom And Rest.

### Domino Circuit

Travelling Tip, Zigzag Chain, Circle Fall, Staircase, Opposing Racks, Wave Answer, Crossover, Grid Release, Fan Collapse, Chain Reset.

### Elastic Loom

Woven Lanes, Vertical Shuttle, Herringbone, Sine Braid, Diamond Fan, Rolling Stack, Crossing Shuttle, Travelling Pleats, Open Wings, Fold And Return.

### Pendulum Choir

Phase Wave, Unison Swing, Counter Swing, Travelling Release, Cross Pendulums, Double Choir, Arc Ladder, Mirrored Swell, Circle Swing, Settle And Answer.

### Magnetic Assembly

Radial Dock, Split Arch, Square Lock, Diamond Lock, Ring Assembly, Spiral Gather, Lane Dock, Four Bank Dock, Orbit Clusters, Assemble And Release.

## Continuous motion and handoffs

Axis Puzzle and Sliding Shutters use reversible eight-beat strokes with zero velocity and acceleration at each stroke endpoint. The grid is inverse-mapped before selecting cells. Some programs stagger neighboring strokes to create zipper and exchange behavior; these are sliding formations, not a simulation of legal Rubik's Cube moves. Existing Stagewall and Panel Cascade movement functions are retained.

The six object scenes share twenty persistent pieces. Their centers, sizes, angles, corner shapes and facet treatments interpolate through a native bridge. Positions are calculated once per scene pass on the CPU using the current/outgoing program samples and the continuing musical clock. A 640-byte pose array extends the production constant buffer from 288 to 928 bytes. The GPU shades and composites the pieces. The new programs do not accumulate angle turns that could cause multiple rapid rotations during a late-set program change.

Pixel Loom has four explicit fallback directions to/from Axis Puzzle and Sliding Shutters. Direct lattice resizing was too abrupt in the two-second stress fixture; these pairs open through continuous material surfaces instead.

Native coverage is now 106 ordered scene pairs: 68 within the nine-wall rig, six between ribbon worlds, two between Blades and Canopy, and 30 between the six object scenes. This adds 56 directed native bridges. The remaining 886 pairs retain material transport. Its displacement now approaches the texture boundary smoothly instead of sampling outside the frame and repeating edge texels. Native bridges and material transport remain different mechanisms.

Automatic changes involving the new scenes receive eight beats. Other non-native handoffs receive six beats during active sections; quiet sections and the existing Blades/Canopy bridge retain eight. Manual transition duration still follows the operator's setting. The conductor preserves the outgoing program sample through the handoff and defers new decisions until a safe boundary.

Identity alternation and spacing are retained. In the six object scenes the pieces gather into surfaces that carry the mark/name, then return to their moving formation. This is a material-mask formation, not a universal letter-shaped mesh system. Pixel Bank retains its smooth corner departure behavior.

## Compatibility and scope

Use the matching v0.23 Stage and Console. Console shared memory is v18; UDP packets are v6 and remain 160 bytes. The Console lays out all 32 scene buttons; new scenes have no dedicated letter shortcut and can also be reached with Previous/Next. A searchable browser, direct per-program operator selection and the broader 100+ scene catalog are future work.

Ten-program coverage applies to these eight new scenes. The eight v0.22 ensembles retain six named programs; other existing scenes retain their established systems. There is no new musical classifier, prediction of unheard audio, live image evaluator or token-consuming runtime.

## Validation

The new production-render suite passed 240 scene/program/aspect sequences (115,200 frames), 720 ordered within-scene program changes (87,120 frames), and all 56 newly introduced native edges at nine source/target combinations (60,984 frames). It checks ongoing movement, nonblank output, temporal deltas and exact/near handoff endpoints. These numerical gates do not establish audience appeal or prove every localized edge artifact absent.

The largest mean adjacent-frame RGB change was 0.0293116 for steady programs, 0.0517311 during program changes and 0.0507967 for the new native handoffs, all below the unchanged 0.06 gate. Maximum native endpoint error was 0.000000610836; the near-endpoint maximum was 0.000117077.

Four Panel Cascade reference captures are byte-identical to the prior reference. Each of four Stagewall captures differs in only one color channel by one 8-bit code value among 691,200 channel samples. The reference compositions and movement functions are preserved; this comparison covers the eight recorded arrangements, not every possible frame.

All eight local rendering suites passed. The existing all-pair sweep passed 992 ordered scene pairs with a maximum mean temporal delta of 0.0507872. Existing coverage also includes 2,048 arrangements, 192 identity sequences, 13,068 dense handoff frames, 45,568 artifact/identity/instrument samples, 9,216 ensemble compositions, the earlier phrase programs and stage-lighting/output contracts. The artifact suite selects 24 scenes for its specialized checks; the all-pair and arrangement sweeps include all 32.

Protocol and conductor tests and seven CPU suites passed. New conductor coverage visits all ten programs in each new scene across twenty simulated songs. Pose checks cover finite values, bounded speed, late-set angles and actual geometric travel in every object program with no audio or lighting input. Four Point Bloom, Diamond Fan and Split Arch include continuous formation movement rather than relying on light changes. Console layout/control validation passed. Full musical-session listening review and 4K/120 output qualification remain open.


Console-owned Stage startup, control and shutdown passed. The portable preview is 120 seconds at 960×540/60 fps, with 7,200 decoded frames, synthetic 120 BPM accents and no soundtrack. It is a scripted rendering fixture, not a live Director or real-track accuracy test. Final scene stills and representative handoff/identity sequences were visually inspected.

The portable build includes `validation/performance-summary.json` with native 4K scene and handoff timings, sample completeness and the running-process/hardware context. Those GPU timings exclude CPU pose preparation, audio, presentation, mirrors and Spout; they do not qualify the complete output path for 4K/120.

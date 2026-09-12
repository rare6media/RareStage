# RareStage v0.23.1 — Pattern Materials

Stagewall and Morph Wall supply the motion vocabulary for six existing object scenes. The shared source contains the sixteen wall signals, authored motif order, row gestures, column gestures and motif interpolation. It compiles into the wall HLSL and the CPU object-pose code. The existing wall renderers keep their geometry and surface behavior.

## How the patterns travel

Each object retains a stable logical row and column address as its formation develops. The same beat and seed feed the shared wall pattern field. Each scene adapts its displacement to its own structure:

| Scene | Geometry response | Stylized surface |
|---|---|---|
| Orbit Relay | Row strokes turn circulating groups; column patterns raise and lower them | Satin discs with curved shading and broad reflections |
| Iris Bloom | Pattern waves turn petals, open their radius and extend their length | Folded facets with restrained hue shifts |
| Domino Circuit | Complete rows translate together; column waves and bands articulate the cards | Enamel with broad gloss and bevels |
| Elastic Loom | Ribbon banks slide, exchange and extend across one another | Directional metal with broad reflected bands |
| Pendulum Choir | Shared strokes move the swinging groups horizontally and vertically | Rounded ceramic surfaces |
| Magnetic Assembly | Row and column strokes translate and dock the plates | Brushed metal with a restrained directional grain |

The ten base formation programs in each scene remain. Pattern changes retain the wall's eight-beat clock and smooth joins. Hits illuminate the surfaces; they do not supply these displacements. This reuses authored patterns across different topologies and does not increase the scene count or claim additional separately authored base programs.

## Handoffs and identity

The six scenes retain twenty persistent pieces and all thirty directed native object pairings. Current and outgoing pattern seeds are evaluated separately before position, size, angle and signal are interpolated. Curvature, facet treatment, grain strength and hue-shift weights blend with the same handoff phase. The Rare mark/name assembly settles the pieces and pattern signal before releasing them back into the continuing movement.

The whole catalog retains 106 directed native handoffs and 886 material-transport fallbacks. This update does not turn every scene pairing into a geometry morph. The automatic duration rules are unchanged.

## Runtime and compatibility

The pose preparation runs once per scene pass on the CPU. Twenty pattern signals add 80 bytes to the frame constants, for 1,008 bytes total; existing phrase fields retain their offsets. The pixel shader evaluates broad material features within the visible piece bounds. These are procedural stylized surfaces, not physically simulated glass or ray-traced metal.

Console shared memory stays at v18; UDP remains v6 with the same 160-byte packet. Local display mirrors copy the completed frame. The matching v0.23.1 bundle provides consistent visual content. Rendering validation now explicitly isolates both playground and material test modes from saved settings, audio capture and the live control endpoint.

## Validation scope

The new rendering fixture covers all sixteen inherited banks in six scenes at landscape, portrait and wide aspect ratios: 288 sequences and 69,120 frames. Separate CPU contracts check the unchanged authored order, smooth field joins, coordinated whole-row displacement and bounded object movement without audio or lighting input. Existing program-change and native-handoff fixtures continue to exercise the production path; object handoffs now also use different outgoing and incoming pattern seeds.

Recorded results, favorite-image comparisons, benchmark environment and the exact coverage counts are shipped in the portable build's `validation` directory. They establish the stated checks, not audience preference or complete musical accuracy. Full-song listening and complete 4K/120 multi-output qualification remain open.

Local results: all nine rendering suites, both protocol/conductor tests, seven CPU suites and Console control/launch checks passed. The inherited-pattern maximum mean adjacent-frame RGB change was 0.0113839 across 69,120 frames. All 992 ordered scene pairs, 720 within-scene program changes and 504 sampled native handoffs passed; the largest sampled native endpoint error was 0.000000203612. The twelve recorded Morph Wall, Stagewall and Panel Cascade arrangements are pixel-identical to v0.23.0. These are the specific recorded arrangements, not a proof about every possible frame.

The portable preview is a scripted 96-second, 960×540/60 fps reel with synthetic 120 BPM accents and no soundtrack. GPU timings and environment details are provided in `validation/performance-summary.json`; complete output qualification remains separate.

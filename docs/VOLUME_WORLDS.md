# Volume Worlds and Auto Color (v0.28)

## Philosophy

Earlier scenes build a composition and let motion happen to it. Rare Volume starts from the
choreography: a program decides where the Rare mark, the twelve panels and the camera are on every
beat, and everything else (light, colour, flat graphics) is arranged around that. The tempo clock
drives every pose, audio only lights the result, and every move is an eased step that arrives, holds
and leaves, so the stage never snaps.

## Rare Volume (scene 42, Depth family)

One ray is traced per pixel, with two rotated-grid samples for clean edges. There is no mesh or
per-object allocation.

| Element | How it is made |
| --- | --- |
| The mark | The logo (or the DJ name during a name formation) is extruded to a slab. The ray marches the shape mask through the slab, refines the hit by bisection, and derives bevel and wall normals from the mask gradient. The front face keeps the artwork, re-tinted by the current palette. |
| Panels | Twelve oriented boxes tested analytically. Each face carries a flat 2D pattern (chevrons, rings, dots, bars) that scrolls on the beat. |
| Backdrop | Flat contour arcs, a radial fan and dust at different depths. Camera yaw shifts each layer by a different amount, which is what makes them read as volume. |
| Floor | A perspective grid with a tempo-travelling light band and a pool under the mark. |
| Light | Surfaces stay near black. A sharp band travels with the tempo, a dim key keeps forms readable, kick accents sit on bevelled edges only. |

### Programs

Each program places all twelve panels and also poses the mark. A phrase blend interpolates both
(angles take the short way round), so a change of program is continuous.

| # | Program | Motion |
| --- | --- | --- |
| 0 | Turntable Ring | A tilted ring turns in 30 degree steps; the mark swings and takes a full turn every 64 beats |
| 1 | Pendulum Halo | An upright ring swings like a pendulum |
| 2 | Twin Helix | Two strands climb around the mark and fade at the ends |
| 3 | Crown Fan | A fan opens over the mark and closes |
| 4 | Diagonal Cascade | Staggered panels tumble through the frame |
| 5 | Counter Orbits | Two tilted rings turn against each other and swell on the beat |
| 6 | Spiral Gather | Panels spiral in to a halo and release |
| 7 | Wave Wall | A grid behind the mark turns over in a travelling wave |
| 8 | Figure Eight | Panels follow one lemniscate through depth |
| 9 | Halo Settle | Everything answers into one calm ring |

The Director treats them as turning, counterflow, travelling, fanning, assembling and folding
gestures, so motif carry-over works as it does for the other scenes.

## The other volume scenes

All four share the engine in `volume_worlds.h` (rotation helpers, box tracer, lighting, backdrop,
floor) and add their own cast in `volume_worlds2.h`.

| Scene | Cast | What the programs choreograph |
| --- | --- | --- |
| 43 Card Match | 18 cards, 9 matching pairs, 6 x 3 | Which cards turn, in what order, and where they travel. Faces are symbols or tinted Rare marks; Logo Chorus turns every card to a mark. |
| 44 Rarebot | A robot of 17 parts floating on four expanding exhaust rings | Shoulder, elbow, head and body angles per beat. The mark sits on the chest; the face is a dot-matrix of lamps (blinking, changing moods, nine-bar mouth meter). Kick nods the head and snare flicks the arms with no easing; heavy music adds random eighth-note twitches and stop-motion stutters. |
| 46 Rarebot Squad | Six copies of Rarebot | Which copies are on stage and where. Each copy pops in on a beat-grid draw that heavy music makes likelier, and flashes in on its own drum (kick, snare or mids). Lineup Pop, Kick Clones, Snare Answer, Mirror Pair, Canon, Teleport, Crowd Surge, Totem, Orbit Crew, Finale Collapse. |
| 45 Pillar Choir | 7 x 7 pillars | The height of every pillar. Logo Formation reads the logo's alpha at each pillar, so the field draws the mark. |

Handovers between programs stagger the pieces by index and route each one out and back (outward and
away from the camera), so pieces never cross the hero in a straight line. Angles take the short way
round. Pose tables are compiled once through a two-pass loop, which keeps shader compile time down.

## Auto Color

The palette is a ring of 24 stops in hue order (`src/palette_ring.inc`, the single source for the
shader, the Console swatches and the Director). Every stop defines primary, accent and spectral
hues. The shader interpolates hue (the short way round), not RGB, so a glide stays saturated.

`ColorDirector` (`src/color_director.h`) owns the colour:

* It retargets on act changes and every 32 beats. Scene changes never recolour.
* The hop size depends on the act: a drop jumps furthest and prefers warm stops, a breakdown moves
  slowly and prefers cool ones.
* A new stop must be at least 2.5 stops away, is penalised for being near any of the last seven
  colours, and is penalised by how often it was used, so a long set covers the wheel evenly.
* A critically damped spring travels to the target. Retargeting mid-glide keeps the velocity, so
  there is no reversal. A held colour breathes by about 5 degrees on the tempo.

Manual choice (swatch, **P**, HUD) holds a colour and turns auto off; **G** or AUTO COLOR turns it on.
The leader sends the target in 1/16 stop units, so followers glide to the same colour.

## Validation

* `RareStageColorTests` simulates ten minutes at 60 fps: worst per-frame hue step, acceleration,
  repeats, short hops, coverage of all 24 stops and dominance are all checked.
* `RareStage.exe --volume-test` renders every program of all four scenes at four points of its phrase, checks that a
  lit composition is visible, then runs every program and every handover for 32 beats at eighth-beat
  steps and flags any frame whose change is far above its neighbours. It also renders handovers to and from other families, DJ-name
  extrusion stills and per-frame cost at 1080p and 4K to `RareStage-volume-validation/`.
* The Console `--screen-ui-test` walks every Show page and fails if a button leaves the frame or a
  scene cannot be reached.

## Limits

The first uncached shader compile takes several minutes on a development machine (about 4 minutes
with these scenes; the optimizer dominates). Release builds ship pre-compiled bytecode
(`--prepare-shaders`). Panels are not shadowed and the floor does not reflect the mark. Two samples per pixel clean the
edges but are not a full antialiasing pass. Actual-song listening review and complete multi-screen
4K/120 qualification remain open.

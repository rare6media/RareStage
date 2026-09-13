# Motion Worlds — v0.26.0

Four new filled 2D scenes add forty named geometric programs. Optical Weave
interlocks broad bands; Iris Assembly opens curved blades; Shape Exchange
organizes discs, diamonds and capsules; Orbit Press moves ordered banks of arcs.
Select a scene and its program in the Catalog. Apply Pattern holds that program;
Resume Director returns to automatic development.

Optical Weave and Shape Exchange share sixteen persistent pieces. Iris Assembly
and Orbit Press share sixteen curved pieces. These four directed scene routes
interpolate each piece's position, orientation and dimensions through the musical
handover. Both endpoints reproduce the standalone scene exactly. Other graphic
routes retain the spatial fold; this release does not claim geometric
correspondence between every scene.

All programs use continuous beat time. Percussion changes light rather than
nudging positions. CPU pose preparation runs once per frame and occupies the
existing fixed-size pose buffer. A dedicated graphic pixel shader avoids carrying
the legacy world's shader dispatch through graphic-only draws. Costs still need
measurement; this is not a complete 4K/120 qualification.

The catalog contains 42 scenes, 35 eligible for automatic rotation, 228 named
programs and 16 continuous-choreography entries (244 previews). The seven scenes
previously excluded from automatic selection remain manual-only. Existing IDs,
favorites and the original Rare artwork are preserved. Stage and Console use
protocol 21; network protocol 9 rejects older peers with a different catalog.

`RareStageMotionTests` checks distinct geometry, long-clock continuity and exact
correspondence endpoints. `--graphic-test --motion-only` captures every new
program, selected handovers and both identity kinds. `--play-test --motion-only`
checks temporal continuity, all within-scene program pairs, aspect ratios and
the authored native routes. Add `--long-clock` to repeat after 8,192 beats.
`--graphic-test --motion-only --preview-only` renders a two-minute synthetic
120 BPM reel. Native images must be inspected alongside numeric checks.

The next release adds action-aware scheduling and bounded offscreen rehearsals.
The 100+ scene / ten-program-per-scene objective remains open. No actual-track
listening or complete multi-output 4K/120 qualification is claimed here.

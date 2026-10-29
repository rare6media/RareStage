# Generative Director (0.31)

How RareStage 0.31 builds, checks and performs worlds that are not in the scene catalog. This is the
architecture as it exists in the code; the last section lists what is **not** done or not verified.

## The pieces

| Layer | Files | What it does |
| --- | --- | --- |
| Audio lanes | `main.cpp` (`AudioEngine`) | Seven bands (sub, kick body, bass, low-mid, mid, presence, air) with per-band onsets, smoothed by a *Reactivity* control and scaled by per-lane operator gains. Reach shaders as `bandsA` / `bandsB`. |
| Modulation matrix | `modulation.h` | 18 sources routed through a typed patch bay into a pool of 32 semantic parameters (`float4 g_Param[8]`). Routes have gain, offset, shaping, attack, release, clamp, combine mode (add, multiply, maximum, replace) and a section mask. Two patch banks crossfade; envelopes of routes shared by both patches carry over. Evaluation is allocation free and deterministic. |
| Procedural palette | `procedural_palette.h`, `palette_shader.h` | Cosine palettes `a + b·cos(2π(c·t + d))`. The Director's ring colour supplies the hue, the section supplies the harmony (analogous, split, complementary, triadic). A hue change inside a harmony slides the short way round the wheel; a change of harmony bridges between *evaluated* colours. `rarePalette()` in the shader is the single choke point, so every scene follows it. |
| Motion clocks | `generative_clock.h` | Integrated clocks (travel, pulse, drift, warp) and slow followers (energy, build, drop). Speed is integrated on the CPU so a change in the music bends the motion instead of making the picture jump. Written into pool slots 20 to 26, which are reserved for this and must not be routed. |
| World description | `world_spec.h` | `WorldSpec`: reproducible DNA (id, seed, family, domain chain, primitives, material, lighting, camera, motion, modulation weights, budget, field parameters, schema and grammar version, shader hash). `Generate(seed, family, quality)` is deterministic. `Validate` rejects bad ranges, non-finite values, illegal module combinations and anything over the hard complexity limit. Specs save and load as plain text. |
| Shader grammar | `world_grammar.h`, `world_fields.h` | Turns a validated spec into a self-contained pixel shader (`psGen`) over the shared `Frame` cbuffer. Whitelisted modules only, constant loop bounds, epsilon-safe maths, finite-value guard at the output, source size limit. Same spec and grammar version give the same source and hash. |
| Compile service | `world_compiler.h` | One `std::jthread`. Bounded pending (8) and ready (4) queues, generation numbers to reject obsolete work, a disk cache keyed by source hash (pruned to 256 files or 64 MB), no lock held while compiling, device-lost flag, ordered shutdown. The worker only calls `CreatePixelShader`; the immediate context stays with the renderer. |
| Qualification | `world_qualifier.h`, `world_qualifier_gpu.h` | A compiled candidate is rendered offscreen at 320x180 for five frames, timed with `D3D11_QUERY_TIMESTAMP` and its disjoint query, scaled to the output pixel count and compared with a budget for the display refresh rate (5 ms at 60 Hz, 3 ms at 90, 2 ms at 120, interpolated between). Results are collected by polling with `DONOTFLUSH` and a no-wait map; nothing blocks. Verdicts: accepted, accepted (heavy, above 70% of budget), over budget, renders black, renders blown out, non-finite output, no timing. Two rejections blacklist a shader hash. An over-budget world gets one cheaper sibling (one quality tier down). |
| Stage guest policy | `generative_director.h` | When a qualified world takes the stage and when it leaves. See below. |
| Launch scene | `world_intro.h` | The intro and standby scene, one shader driven by pool slots 20 and 21. |

## Scene families

Four families are generated for the stage. Each is a screen-space field that reads the musical clock directly,
so its choreography does not depend on the Director switching anything.

* **Cosmic Space**: domain-warped nebula with filaments, parallax stars that stretch into a hyperspace run on
  builds, planet with atmosphere and rings, spiral galaxy, drop shockwave.
* **Circuit Board**: routed nets (shared edge bits keep traces continuous, 45 degree chamfers, vias), two layers,
  a lit relief with a moving key light and a reflected horizon, dark chips with a lit die, pulses on the nets, ambient
  motes and a burst of sparks on every beat.
* **Virus Spread**: Voronoi cells with an infection front that grows through the phrase, surges on the drop and heals
  before the next phrase, spores drifting over the field.
* **Error Horizon**: a glowing horizon and sun corrupted by tears, macroblock shifts, pixel-sort streaks, channel
  splits, bit flips and static. Damage follows the section; errors land on sixteenth-note slots, with a clean breath
  on each bar except when a drop lands.

The eight older structural families (panel cascade, monolith, cathedral, conduit, fractal chamber, orbital
structures, abstract fields, nested architecture) are still in the grammar as ray-marched SDF worlds but are no longer
generated for the stage. Their only use today is a small monolith or ring accent inside the scene families.

Patterns change on phrase boundaries (8, 16 or 32 beats per spec) and morph smoothly: the pattern is a parameter
vector that is interpolated, never switched.

## The stage guest

A generated world is a guest over the Director's show. The Director's own scene logic keeps running underneath, so a
release always lands on a coherent picture.

* It arrives at the onset of a drop (about a 0.3 s fade) or on a 16-beat phrase boundary during a build or a sustained
  drive (a 2.5 s fade). Never mid-phrase, and never without a qualified candidate: a drop with nothing ready is missed.
* It stays about 96 beats and leaves on a phrase boundary. A cooldown of 64 beats and 40 seconds follows.
* Any operator action ends it at once: picking a scene, previous or next, freeze, blackout, manual mode, clean open,
  fallback, loading a preset, holding a program, forcing a section. Nothing starts while the operator holds the show.
* It steps back by itself if the GPU frame time stays above 30 ms for three seconds.
* Fully on stage it is drawn alone; during a fade it is blended over the Director's picture with a blend factor.
* It is not used in a sync group (the takeover is local).

Operator controls (Director tab): **TAKE NEXT QUALIFIED WORLD NOW**, **RELEASE GENERATED WORLD**, and a preview that
plays one chosen family and seed on the output (unqualified, with the same GPU safeguard).

## Launch sequence

The renderer opens on a launch scene of about six seconds (nebula, hyperspace run, arrival flash and shockwave, a
constellation that spells the Rare mark, the mark with a light sweep), then settles into **standby**: the mark, a ring
that follows the level and seven live lane meters, so standby is also a test pattern. Standby ends when the operator
acts. Picking a scene starts that scene with the Director on auto; AUTO starts the Director. A click or key during the
intro skips to standby. `--no-intro` disables the sequence; validation and self-test modes never run it. The console has
its own launch animation (`console_splash.h`); `--no-splash` disables it. **REPLAY LAUNCH SEQUENCE** returns the
renderer to standby.

## Threading and ownership

* The render thread owns the D3D11 immediate context, all render targets and the qualifier.
* The compile worker owns nothing but its queues; it receives specs by value and returns finished packages.
* The audio thread publishes complete frames; the modulation snapshot is built once per frame from one copy.
* `WorldCompiler` must be destroyed (or `stop()`ped) before the device. `~App` does this first.

## Settings and protocol

Settings keys: `Reactivity`, `BandGain0` to `BandGain6`, `Modulation`, `ProceduralPalette`, `Generative`. All default
to off or neutral, so the show is unchanged until a feature is enabled.

Shared-memory protocol version 30 (mapping `Local\RareStage.Control.v30`). The console and renderer must come from the
same build. Commands added since 0.30: `SetReactivityPercent`, `SetBandGainPercent`, `SetModulation`,
`SetProceduralPalette`, `SetGenerative`, `SetPreview`, `GenerativeTake`, `GenerativeRelease`, `EnterStandby`.

## Tests

* `RareStageGenerativeTests`: modulation matrix, procedural palette, motion clocks.
* `RareStageWorldSpecTests`: generation, validation, determinism, persistence, shader safety, the compile service
  (bounded queues, cache, stale results, shutdown), the qualifier on a real GPU, every family compiling and drawing,
  choreography changing with the music, and the launch scene.
* `RareStageGenDirectorTests`: the stage guest policy, including a simulated hour.
* The existing suites and `--performance-self-test`, `--audio-self-test`, `--director-render-test` still pass.

## Not done, or not verified

* The generated worlds have not been heard against real music in this repository's tests; choreography was checked
  with synthetic section, energy and band values.
* Drop and phrase arrivals are covered by the policy test, and take, release and operator interruption were run live,
  but the cross-fade between a guest and the Director's picture was not watched live.
* GPU costs are estimates from a reduced-resolution offscreen render scaled to the output size. Offline they came out
  between 0.16 and 0.5 ms at 1080p, but the same pass measured lower live; the cause is unconfirmed.
* Qualification history and favourite worlds are not saved to disk. There is no full-resolution qualification and no
  runtime quality reduction other than the cheaper sibling.
* Device removal is handled in code (the service flags it and is rebuilt) but has not been exercised.
* No long-running soak test or leak check of repeated generation cycles has been done.
* The structural families are not yet used as real accents inside the scene families.
* The first launch after a shader or constant-buffer change recompiles the main shader (several minutes). The release
  package ships prepared shader bytecode to avoid this.

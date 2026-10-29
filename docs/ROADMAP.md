# RareStage roadmap: status at 0.31.0

Status as of October 29, 2026. This replaces the status sections of [ROADMAP_V0_30.md](ROADMAP_V0_30.md), which is kept for its
original goals and traceability (the 100+ scene catalog target and its research and performance exit criteria are
**still open**). "Completed" means built, integrated and covered by the checks named in the entry. Anything that was only
checked with synthetic values, or not watched live, is marked as such. See
[RELEASE_0_31.md](RELEASE_0_31.md) and [GENERATIVE_DIRECTOR.md](GENERATIVE_DIRECTOR.md) for the details.

## Completed in 0.31.0

**Catalog and audio**
- Eleven scenes retired from rotation, hidden rather than deleted so scene ids hard-wired into shaders are unchanged
  (Signal Rail, Void Gates, Vector Speedway, Kinetic Monument, Domino Circuit, Pendulum Choir, Magnetic Assembly,
  Gearworks, Origami Parade, Radial Fans, Optical Weave). 36 active scenes, 202 named programs. Checked by the phrase
  conductor and performance tests.
- Seven independent audio lanes with onsets, a Reactivity control and per-lane gains, in the Audio Lab and in shaders.
  Checked by the audio self-test and a live run with generated audio.

**Console**
- New Dear ImGui console with Live, Mix, Audio Lab, Director, Output and Production tabs; the old console remains as
  `RareStageConsoleClassic.exe`.
- Console launch animation in the RareClip style (swarm, lasers, mark assembly, wordmark scan, version decode); checked
  from captured frames. `--no-splash` skips it.

**Generative engine**
- Modulation matrix (18 sources, 32 parameters, four combine modes, section masks, two crossfading patch banks) and
  procedural colour harmony (cosine palettes, hue slide, evaluated-colour bridge). Both off by default.
  Checked by `RareStageGenerativeTests`.
- Integrated motion clocks, so a change in the music bends motion instead of making the picture jump. Checked by unit
  tests; not yet judged against real music.
- Reproducible world specs, validation, text save and load, and a deterministic shader grammar. Checked by
  `RareStageWorldSpecTests` over thousands of generated specs.
- Four stage scene families: Cosmic Space, Circuit Board, Virus Spread, Error Horizon. Each compiles, draws, moves and
  changes with the section on a real GPU in the world spec test.
- Background compile service with bounded queues, disk cache, stale-result rejection and ordered shutdown.
- GPU qualification with timestamp queries, refresh-rate budgets, verdicts, blacklist and a cheaper sibling. Checked
  with real and hostile shaders on a real device and live.
- Stage guest policy: drop and phrase arrivals, debut, lifetime, cooldown, operator authority, GPU step-back. Policy
  checked by `RareStageGenDirectorTests`; take, release and operator interruption checked live.
- Operator preview of a chosen family and seed; operator take and release.

**Launch and release**
- Renderer launch scene and standby scene (mark, level ring, seven lane meters); scene click or AUTO starts the show.
  Checked live from the renderer's saved frames.
- Version 0.31.0, README, release notes, architecture document, `package-release.ps1`, and a `RareStage-0.31` package
  with prepared shader bytecode that launched and started the launch scene.

## In progress

- **Qualification gaps.** History and favourite worlds are not saved to disk, there is no full-resolution qualification,
  no runtime quality reduction other than the cheaper sibling, and no separate last-known-good fallback.
- **Visual quality of the new families.** Circuit chips and Cosmic Space were improved but are not finished; Virus
  Spread and Error Horizon are the strongest. Only one pattern set per family. The structural families appear only as a
  small monolith or ring accent.
- **Live validation with real music.** Choreography and pacing were checked with synthetic values and captured frames.
  The cross-fade of a guest over the Director's picture has not been watched live.
- **Cost figures.** GPU cost is estimated from a reduced-resolution render; offline and live measurements disagree and
  the cause is unconfirmed.
- **Release hygiene.** The README's older sections still describe retired scenes. The GitHub Actions workflow was edited
  for the new targets and version but has never been run.

## Not started

- **Hardening (Phase H):** long-running soak tests, a resource-leak check across repeated generation cycles, exercising
  device removal and recreation, and measured 60, 90 and 120 FPS benchmarks. No FPS claim is made until these exist.
- **Console parity:** the Rehearsal tab, scene favourites and per-scene program selection are not ported, so the classic
  console cannot be retired yet.
- **Director self-judgment:** light operator feedback (thumbs up or down) feeding the Director's scoring.
- **RareStudio Window Capture:** the window is not picked up in Window Capture mode (Screen Capture works). Not
  diagnosed; needs to know whether RareStage is missing from the window list or captures black.
- **Retrofit of existing scenes** to the seven lanes and the modulation matrix; today only the composite pass consumes
  the matrix.
- **Patch editor:** only two built-in modulation patches exist and there is no console editor for them.
- **World library:** saved favourites, mutation and inheritance of specs, showing structural families as real accents.
- **Optional research:** a slow, non-real-time "producer" loop with a small local model to help tune audio response;
  GPU particle systems, compute-driven synthesis, multi-layer compositions, external show control.
- **Original 0.30 goals:** the 100+ scene catalog with ten programs each, held-out musical evaluation, blinded aesthetic
  comparisons and physical output qualification.

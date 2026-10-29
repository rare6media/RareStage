# v0.24 — Choreography Sequences

The catalog now contains 35 scenes. Gearworks, Switchyard and Origami Parade add 30 named programs. Eleven playground scenes have ten programs each; eight earlier ensemble scenes have six each. The other sixteen scenes use continuous authored choreography. This is 158 selectable named programs, not completion of the 100-scene / ten-pattern target.

## Connected direction

A show sequence keeps a travelling, counterflow, turning, fanning, assembling or folding motif across committed scene visits. It establishes, carries, develops and releases that motif. A confirmed drop can reserve a payoff, with a 96-beat cooldown; an uncertain event cannot spend it. A new chapter changes the motif after five directed visits. A manual scene choice reanchors the sequence.

The sequence influences scene route scores and the Phrase Conductor's program choices. Program-use memory still discourages accidental repetition. Existing scene dwell, section evidence, conditional branches, manual holds and eight-beat program interpolation remain active. Forecasts prepare routes; they do not authorize payoffs. No new music classifier, online learning or AI inference is introduced.

Directed changes carry the wall-pattern seed through a chapter. Between the nine persistent-object scenes, the renderer samples all ten incoming programs against twenty outgoing piece positions, sizes and velocities. It keeps the thematic choice unless another entry improves that geometric cost. This is a bounded CPU calculation at a scene change, not additional live GPU rendering. Shared native geometry performs the actual handoff; material transport remains the fallback for unrelated geometry.

Identity entrances and exits use two gesture-related choices per motif. Existing spacing, assembly/hold/release timing, mark/name alternation and the rule against revealing during a scene handoff remain. Pixel Bank's smooth corner departure remains unchanged.

## Three machine compositions

* **Gearworks:** counter-rotating shafts with solid drive links, opposing racks, vertical clutch, annular train, twin gearbox, tilting drive, cam wave, compression lock, spiral drive, clutch/release. These are stylized kinetic mechanisms, not a mechanical simulation.
* **Switchyard:** closed delivery circuits, counterflow, elevators, figure-eight sorter, lane exchange, diagonal junction, row shunt, column lift, roundabout, dock/dispatch. Closed paths avoid offscreen recycling seams.
* **Origami Parade:** travelling creases, opposing folds, rosette, wings, turning tapestry, accordion wave, double fan, pleated exchange, spiral flight and fold/seal. Faceted diamond silhouettes articulate continuously.

Percussion accents surface light. Object positions follow the continuous musical clock. Rotations remain bounded so a long-running scene cannot accumulate extra revolutions during interpolation. The three additions share the twenty-piece geometry bridge with Orbit Relay, Iris Bloom, Domino Circuit, Elastic Loom, Pendulum Choir and Magnetic Assembly.

## Catalog controls

Open **CATALOG**, or press **Ctrl+F**. Search matches scene and program names. Category and favorite filters affect browsing; they do not silently restrict the automatic show. Starred scene IDs are saved to `catalog-favorites.txt` beside Console. If that folder is read-only, favorites remain available for the session and Console reports the save failure.

Selecting a scene or program in the browser only changes the preview. Previews are production-rendered stills, not live output. **Cue Scene** changes the live scene. After its handoff and identity animation settle, **Apply Pattern** starts an eight-beat interpolation and holds that program. **Resume Director** releases the pattern hold. Scene cycling remains independently controlled by Smart Director / Hold Auto. A follower rejects local program selection.

The browser paginates the catalog and preserves stable IDs and existing scene shortcuts. `tools/build_catalog_previews.ps1` generates the 174 packaged preview images from the production renderer.

## Compatibility and validation

Use the matching Stage and Console executables. Local control is v19; the UDP packet is v7, still 160 bytes. Older network versions are rejected because they cannot interpret the expanded catalog. Local mirrors still copy the same completed frame.

There are 148 directed native scene pairs and 1,042 material-transport pairs, totaling 1,190 ordered pairs. Native coverage is not claimed for every pair. Tests cover all 110 playground programs at three aspect ratios, all 990 directed within-scene program changes, and 882 native handoffs across 98 playground-related edges and nine representative program combinations. The older six material scenes retain their separate 288-sequence shared-pattern check.

CPU contracts cover sequence development, uncertain-event rejection, payoff cooldown, manual reanchoring, entry-cost reduction, distinct new program motion, program holds and catalog filtering. These fixtures do not establish musical taste or long-set listening quality. Full-song listening review and integrated 4K/120 presentation/mirror qualification remain open.

### Local results — 12 September 2026

All nine render suites, two CTest targets, seven CPU suites, Console layout/preview checks and Console-owned Stage launch/control/shutdown passed. Additional production command tests verify pattern application, hold/resume, invalid IDs, in-flight changes, identity protection and follower rejection.

The 330 program/aspect sequences cover 158,400 frames. Their largest mean adjacent-frame RGB change was 0.0293116. The 990 program-change fixtures peaked at 0.0517311; 882 sampled native handoffs peaked at 0.0507967, with maximum endpoint error 0.000000407224. All 1,190 ordered scene pairs passed. The specialized artifact fixture covered 51,712 frames across 27 selected scenes. These are sampled render checks, not exhaustive perceptual or musical qualification.

Twelve recorded Morph Wall, Stagewall and Panel Cascade arrangements remain pixel-identical to v0.23.0. Entry matching improved its analytic position/velocity/size cost in 181 of 216 fixtures, with a mean reduction of 0.398566 normalized cost units. That result measures the matcher’s continuity objective rather than audience preference.

Native 3840×2160 GPU timing completed with 960 valid samples per scene and 121 per handoff, covering 35 scenes and 47 selected handoffs without incomplete-scene retries. On the RTX 4070 SUPER, Gearworks averaged 1.53105 ms, Switchyard 1.24835 ms and Origami Parade 1.27257 ms. The heaviest scene averaged 3.88632 ms (P99 5.00509 ms); the heaviest handoff averaged 5.163 ms. The largest handoff P99 was 6.71197 ms. Another RareStage instance remained running, so these measurements are contended and do not establish a speedup. They include scene/bloom/composite GPU passes and exclude CPU choreography, audio, presentation, mirrors and Spout. Complete 4K/120 output remains unqualified.

The packaged preview is a fully decoded 120-second, 960×540, 60 fps scripted reel with synthetic 120 BPM accents and no soundtrack. Hosted CI status is recorded separately in the package provenance and pull request.

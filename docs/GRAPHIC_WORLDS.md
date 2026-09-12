# Graphic Worlds — v0.25.0

Three filled 2D compositions develop the approved motion studies without using the twenty-object renderer. They add thirty named programs, with continuous tempo-based motion and restrained percussion lighting. Select **Contour Currents**, **Ribbon Braid** or **Radial Fans** from the scene controls or Catalog. In the Catalog, cue the scene, select a program and use Apply Pattern to hold it; Resume Director restores automatic development.

## Programs

| # | Contour Currents (35) | Ribbon Braid (36) | Radial Fans (37) |
|---|---|---|---|
| 0 | Traveling Bands | Over Under | Counter Rotation |
| 1 | Central Compression | Braided Opening | Align Open |
| 2 | Opposing Waves | Split Return | Opposing Fans |
| 3 | Diagonal Sweep | Parallel Current | Spiral Petals |
| 4 | Chevron Roll | Dual Braids | Outer Zipper |
| 5 | Double Lens | Herringbone Flow | Double Iris |
| 6 | Hourglass Exchange | Cross Weave | Pinwheel Steps |
| 7 | Concentric Field | Standing Knot | Quarter Cycle |
| 8 | Diamond Contours | Open Wings | Concentric Press |
| 9 | Resolve Release | Unison Release | Wide Aperture |

Contours fill the frame with ordered bands, compressions, chevrons and nested fields. Ribbons use four continuous curves with explicit front/back ordering and opaque surfaces. Fans use broad sectors, separated inner and outer rings, opposing rotation and eased arrivals. The eight existing palettes supply coordinated color groups. Each program changes geometry; a palette change is not counted as another program.

The Phrase Conductor holds recognizable formations and selects related answers, development or resolution on the musical clock. The Show Sequence maps each new program to travelling, counterflow, turning, fanning, gathering or folding gestures. Scene routes, program memory and automatic dwell controls continue to operate. These are authored deterministic rules, not model inference or learned musical understanding.

## Cohesion and identity

Within a scene, contour coordinates, ribbon poses and fan dimensions interpolate over the existing eight-beat program blend. Positions do not reset or receive kick-driven nudges. Percussion changes edge illumination.

The six directed joins between the three graphic scenes render in one scene pass. A travelling curved fold bends outgoing and incoming material coordinates around a narrow moving boundary. Joins between a graphic scene and an older scene use that same fold in the two-scene material pass. This is spatial material transport, **not universal geometry correspondence**. Older scene pairs retain their existing handoffs. The cumulative catalog has 154 directed one-pass native handoffs and 1,252 material handoffs, excluding self-pairs.

Initial distance-field intersections produced broken fragments and were discarded after viewing the native intermediate frames. Full-frame opacity blending also obscured both compositions, so it is not used for the new graphic handovers. The fold preserves the shapes on either side; its middle is intentionally a handover between two visual languages rather than a claim that a ribbon literally becomes every fan blade.

Rare and DJ-name reveals inherit moving band colors while the surrounding composition continues at reduced brightness. Three entrance/exit families gather through curved rows, rotate into alignment, or expand from a smaller formation. These use the existing identity mask and global identity timing. The original Rare artwork is retained. They do not claim mesh-based construction of individual letter strokes.

## Catalog and compatibility

The seven rejected scenes, IDs 28–34, remain selectable manually but are excluded from automatic next, following, alternative and timeline choices. This applies even when one of those scenes is currently selected or previously preferred. Their existing IDs and favorites remain valid.

There are now 38 catalog scenes, of which 31 are eligible for automatic selection, and 188 named programs plus sixteen continuous-choreography scenes. Seventy named programs belong to the seven manual-only scenes. The catalog has 204 rendered previews. These counts describe implementation, not artistic approval of every older scene. The 100+ scene / ten-pattern target remains open.

Use the matching v0.25 executables: Console protocol 20 and network protocol 8. The network packet remains 160 bytes and the frame constants remain 1,008 bytes. Fullscreen controls, display selection and synchronized local mirrors are preserved.

## Validation and limits

`--graphic-test` captures 120 temporal pattern samples, 162 directed handoff samples and 162 Rare/name formation samples through the production HDR and composite passes. `--graphic-test --preview-only` renders a two-minute 960×540, 60 fps reel with synthetic 120 BPM accents, wall handoffs and identity reveals. It contains no soundtrack and does not establish musical-classification accuracy.

The playground regression also exercises the thirty new programs at three aspect ratios, every ordered within-scene program change, and new native handoff endpoints. Full-catalog pair tests cover 1,406 ordered pairs. CPU contracts verify manual availability, program metadata, curated route selection and a twenty-song synthetic route sequence. The existing wall reference fixtures are compared pixel-for-pixel.

Native GPU timings exclude audio/CPU processing, presentation, mirrors and Spout. An independently running v0.24 renderer was left active during measurements. Complete 4K/120 output qualification and full-song listening review remain open. See the packaged validation records for exact measured results and build provenance.

# Full-frame 2D choreography

September 12, 2026. The user clarified the visual target: more visually striking 2D effects with the quality of Stagewall and Morph Wall, while avoiding a show defined by panels. This supersedes the Domino-first prototype order in the initial visual audit.

Implementation follow-up: the user approved all three motion directions. [v0.25 Graphic Worlds](GRAPHIC_WORLDS.md) implements three native families with ten programs each. The sketch and prototype-order notes below preserve the original design brief; they are not the current runtime status.

## What carries forward

Carry coordinated groups, readable movement patterns, confident color, controlled acceleration, opposing answers and satisfying arrivals. These qualities can drive curves, ribbons, radial sectors, silhouettes and negative space. A scene does not need panels, literal machinery, a grid of small objects or a 3D camera to communicate them.

The whole frame should form one composition. Broad forms do the principal movement; smaller detail supports that movement. Color follows groups and actions rather than giving each object unrelated decoration. A restrained phase should still have a readable form and ongoing motion.

## First three visual directions

### Contour currents

Broad filled bands flow across the entire frame. Their boundaries compress, bend and part to create graphic openings. They can become opposing waves, nested contours, chevrons and expanding rings while preserving clear ordering. The effect is graphic choreography, not a literal water simulation.

The principal action is **travel → compress → open → redirect**. The recognizable event is a whole field arriving in a distinct formation. Avoid thin wireframe terrain, excessive frequency, crossings between adjacent bands and a permanently small center motif.

### Ribbon braid

A few large continuous ribbons weave over and under each other. Crossing order, connected edges and consistent thickness must remain visible. The weave can spread across the frame, gather into a central knot, split into opposing streams and exchange direction.

The principal action is **weave → separate → exchange → rejoin**. A ribbon should remain traceable through the composition. Avoid assembling a ribbon from disconnected sticks, uncontrolled overlaps and uniformly translucent glass shading.

### Radial fans

Large filled sectors develop into counter-rotating fans and broad apertures. Different rings and opposing groups answer each other, briefly align, and open. Supporting arcs may continue past the screen edge to establish scale, with the dominant central structure remaining readable.

The principal action is **counter-rotate → align → open → unfold**. Use purposeful acceleration and arrivals within the musical clock. Avoid tiny cog icons, bead rings, perpetual undifferentiated spinning and reliance on bloom for presence. This direction may rebuild an existing radial scene instead of adding another near-duplicate catalog entry.

## Motion study delivered in conversation

The conversation contains three selectable motion sketches of these directions, each developing through three arrangements on a simulated 120 BPM clock. Contours interpolate their ordered boundaries, ribbons use explicit crossing depth, and radial forms use eased rotations with arrivals. They demonstrate composition and movement intent; they are not production shader implementations or finished sets of ten programs.

The sketches do not take microphone input, classify music, reproduce the full Director, preview production handoffs, or establish the native renderer's performance. Selecting a direction switches the comparison view; it is not a demonstration of the final scene transition. Existing RareStage code, scene IDs, saved settings and executable behavior are unchanged by this design pass.

## Native implementation order

1. Develop Contour currents and Ribbon braid through the actual HDR renderer, using two materially different full phrases. Keep Radial fans as the next comparison direction or an existing-scene rebuild.
2. Give each action a destination, a readable completion and safe continuations. Apply beat evidence to planned action timing and lighting accents without resetting its position.
3. Author meaningful handoffs: the spacing of wall rows becomes the spacing of contour bands; a contour band lengthens into a ribbon; a ribbon curls around an opening. Preserve direction and clear gaps throughout the middle of each join. Do not assume every pair shares geometry.
4. Review complete motion with Stagewall/Morph Wall as quality references. Expand retained directions into at least ten genuinely different programs only after the first phrases are compelling.
5. Measure native frame cost, continuity, silhouette, aspect-ratio behavior and identity readability separately from artistic judgment. Keep full-song musical validation open until actual tracks are available.

The seven rejected scenes remain unfinished. The 100+ scene objective remains, but a large catalog should contain different visual languages with depth in each—not more entries sharing one small-object renderer.

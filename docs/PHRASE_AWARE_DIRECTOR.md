# Phrase-Aware Director — v0.27.0

Automatic scene changes now respect establishment, program blending and a
payoff interval before taking a preferred beat-aligned departure. Movement
gestures supply the timing contracts. A pending departure stops the Phrase
Conductor from starting another program. Uncertain musical evidence allows one
longer continuation; an armed deadline remains fixed. Holding a pattern also
holds automatic scene changes until the operator resumes the Director. Pixel
Bank retains its dedicated corner-arrival choreography.

The Director can rehearse two candidate handovers before the final route lock.
Each receives five samples through the same HDR, morph, bloom and composite
passes as live output, using separate targets no larger than 192 pixels per
dimension. At most one sample is outstanding and submissions are at least
250 milliseconds apart. GPU queries and staging reads are polled without
flushing or waiting in the live loop. The output texture and its timestamps
remain separate from the rehearsal targets and timestamps.

Rehearsal requires known spare rendering capacity, a settled program, no active
identity reveal and sufficient time before the cue. Pressure skips submissions.
An expensive sample triggers a backoff before another capacity probe. Manual
holds, changed scene/program/music context and expired results invalidate the
batch. Cancelled work already submitted to the GPU may finish; its results cannot
enter a replacement batch. Resource or query failures leave analytical planning
available.

Image measurements describe luminance, occupied area, focal center and a coarse
spatial brightness map. Candidate penalties account for unexpected dark loss
and excess composition changes relative to the endpoints. Sparse compositions,
dark openings and purposeful brightness development are legitimate. These are
bounded heuristics, not a general aesthetic judge, a flicker detector operating
at full output frame rate, or musical understanding learned from tracks.

The image penalty supplements movement compatibility, recent-use memory,
musical role and measured route costs. Once both finalists have been checked,
immediate selection stays within that pair; branches for a different musical
section retain the full eligible catalog and discard these image penalties.
The final eight beats preserve the route
lock. A completed matching rehearsal can supply its entry program when the cue
lands at the rehearsed time. Changed context returns selection to the existing
analytical planner. No model inference, online service or runtime token use is
involved.

The Console reports the motion intention, the beats remaining to a departure and
completed handover checks. Shared telemetry also includes submitted frames,
discarded batches, skipped-budget count and rehearsal GPU time. Use matching
Stage and Console protocol 22. Network protocol 9 and all scene IDs remain
unchanged from v0.26. The catalog remains 42 scenes, 35 auto eligible, 228 named
programs and sixteen continuous-choreography scenes.

Validation includes CPU intent/budget/staleness fixtures and production GPU
rehearsals, output restoration, cancellation and operator precedence checks.
One hour of prescribed chapters produced 74 scene departures across all 35
automatic scenes, with no failed payoff, hold or progression contracts. This
accelerated simulation uses the App scheduler and conductor; it is not a
one-hour live-output soak or an audio-classification evaluation.
Synthetic inputs and controlled artifact fixtures are identified in the reports.
Actual-song comparison and complete multi-screen 4K/120 frame-pacing qualification
remain open; five low-resolution samples do not establish either.

Run `ctest --test-dir build -C Release --output-on-failure` and
`RareStage.exe --director-render-test`. The latter writes
`RareStage-director-validation/contracts.csv`, `rehearsals.csv` and native images.
It also verifies that the actual coordinator completes a batch and that the
committed scene uses its rehearsed entry. Resource-pressure inputs in these
fixtures are controlled, while all rehearsal samples use production GPU passes.

The four Motion Worlds now have explicit bass, mid, high and section affinities
in the existing musical-fit table. The new instrument tables also have a
compile-time catalog-size check to prevent silent zero-filled entries.

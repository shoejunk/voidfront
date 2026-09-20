# Independent presentation performance review - 2026-09-20

Scope: ordinary offline presentation instrumentation and retained evidence.
Reviewer owns this report and `tools/verify_presentation_profile.py`; no Git,
builds, packaged launches or timing experiments were performed by this reviewer.

## Findings before implementation

Source inspection identified unnecessary previous-unit index reconstruction each
rendered frame, linear selection membership in actor/HUD loops, and projection of
full-health unselected Scale128 actors whose bars are subsequently hidden. HUD
also resubmits static minimap obstacles every redraw. These are concrete repeated
operations, not proof of the dominant frame bottleneck. Early bar eligibility is
a narrow optimization; previous-snapshot caching must refresh on offline, network
and reset paths. Selection caches must cover direct test and production mutations.

The prior scale captures cannot diagnose a CPU/GPU cause: the same-run twelve-unit
control was also slow, and hidden launch/capture costs were not isolated. Asset
source reports 8,888 triangles and 15 joints per walker, but imported generated
LODs and actual visibility prevent multiplying those counts into a claimed draw
load. Engine counters and actual stage timings are needed.

## Instrumentation review

Reviewed `presentation_profile.gd`, the production main/HUD instrumentation and
`profile.ps1`. The opt-in profile preserves ordinary AI and camera setup, records
predeclared warmup separately, retains every sample, and performs optional image
readback after measurement. Bridge advancement and snapshot conversion are
separate; snapshot includes authoritative hashing. Main callback encloses bridge,
snapshot and presentation durations and must not be added to them. Neither
presentation nor HUD callback timing includes asynchronous renderer/GPU work.

Deferred sampling follows child process callbacks. HUD draw timing may describe
a prior submitted redraw; aggregate draw observations must not be asserted to
identify the same process frame. The profiler's notes now state this limit.
Engine monitors likewise have their own cadence. Focused/nonminimized status does
not prove an unoccluded foreground window, human interaction or input-to-photon
latency. Instrumentation overhead, including boundary snapshot copying, remains
inside raw intervals.

Invalid profile-option error handling initially allowed a launch to wait for the
watchdog; the implementation agents were notified. Console-launcher process
counters initially sampled the wrapper rather than the actual Godot game. Those
source-run host numbers must not be reported as game RSS/CPU. Helper corrections
and final launch behavior remain subject to final review.

## Independently verified baseline evidence

The independent Python verifier checks sequential frames; telescoping wall
intervals; warmup/measurement boundaries; finite nonnegative durations; one main,
presentation and HUD process callback per frame; bridge/snapshot count equality
with tick advancement and the production eight-tick catch-up cap; containment of
timed child stages in the main callback; hash/population/focus metadata; and every
nearest-rank summary. HUD draws have independent cadence but must be observed.
Ten deliberate corruptions reject per report, including mutations whose summaries
are recomputed so rejection checks underlying relationships rather than stale
percentiles alone. This is evidence-integrity validation, not a performance gate.

Both baselines passed:

- `artifacts/presentation-500-before-2026-09-20.json`, SHA256
  `cd0feabf716512ad7dc70bda3e50e6fcabb644beb580e01c6c646f97ca0a24e6`;
  independent result `artifacts/presentation-source500-integrity-2026-09-20.json`.
- `artifacts/presentation-package500-before-2026-09-20.json`, SHA256
  `e02ef1fec8113cab2e96a8f2b13528896f913f0f0b88ecdc96eb2ac93c25251d`;
  independent result
  `artifacts/presentation-package500-before-integrity-2026-09-20.json`.

Both retain 60 warmup and 360 measured frames, all measured frames focused and
nonminimized, camera size 27, and zero selected actors. Therefore they do not
measure a large-selection membership improvement. Ordinary source mode loads
the Debug extension; the source and package runs are not equivalent build modes.

| Baseline | Measured ticks | Wall p95 / p99 ms | Bridge-call p95 / p99 ms | Snapshot-call p95 / p99 ms | Present-call p95 / p99 ms | HUD-draw p95 / p99 ms |
| --- | --- | --- | --- | --- | --- | --- |
| Source 500 | 80 to 753 | 110.580 / 121.023 | 16.459 / 24.518 | 3.272 / 4.309 | 8.497 / 9.259 | 6.494 / 8.224 |
| Packaged 500 | 65 to 476 | 62.607 / 66.953 | 2.018 / 2.328 | 1.125 / 1.299 | 6.740 / 7.307 | 6.097 / 7.950 |

Packaged main callback p95/p99 is 12.287/12.851 ms, including its child stages.
Source main p95/p99 is 50.960/61.072 ms. The source run's 673 advances comprise
289 two-tick, 59 one-tick and 12 three-tick measured frames. Initial source scene
contains 2,093 MeshInstance3D nodes, 500 Skeleton3D nodes and 500 AnimationPlayers;
draw-call monitor median is 4,397, with 4,392..5,703 observed. These observations
justify further renderer/animation profiling but do not identify CPU versus GPU.

Actually opened both baseline PNGs at 1920x1080. The HUD and minimap are readable;
the formation is a bright repetitive mass on blockout terrain. The source image
shows contact/damage at the front and final populations 242/250; the earlier
packaged endpoint has 250/250 alive. Static images do not establish animation,
responsiveness, crowd progress or art acceptance.

Fixed rendered-frame counts cover different simulation durations when frame
speed changes. A future before/after run must disclose this and retain all raw
samples rather than claim a matched-state causal whole-frame improvement. No
reference hardware, complete battle, human responsiveness, CPU/GPU diagnosis or
shipping approval is established. Final optimization evidence remains pending.

## HUD optimization and packaged after evidence

The final narrow optimization is in HUD only: static minimap cells are prepared
once in a MultiMesh 2D quad batch; a selection set is rebuilt each redraw; hidden
healthy unselected Scale128 health bars skip projection. Actor presentation and
simulation are unchanged. Unit circles retain the same radius/colors/positions.
Unit quads centered at cell+0.5 match the old unit cell rectangles. Per-axis map
scale and map-rect translation preserve square and rectangular map placement;
the draw transform resets before circles, health bars and drag rectangles. Current
reset/network flows retain static terrain, so the ready-time batch is valid.
Future dynamic terrain requires rebuilding it. No source blocker was found.

`artifacts/presentation-package500-after-2026-09-20.json`, SHA256
`066ae5ad1c6a215fbc3b61aca7ab0ee74f5de86e50183963449c4f08c7600929`,
passes the independent verifier and all ten corruptions. Result:
`artifacts/presentation-package500-after-integrity-2026-09-20.json`.
All 360 measured frames are focused/nonminimized with zero selection and camera
size 27. Measured ticks are 61..460; final populations remain 250/250.

Observed before/after HUD-draw p95 is 6.097/4.573 ms and mean is 5.397/4.164 ms.
Wall p95 is 62.607/60.745 ms, p99 66.953/63.859 ms; this remains a severe frame
budget failure. Present-call p95 is effectively unchanged, 6.740/6.745 ms.
Median draw calls are 4,397/4,398, so **no draw-call reduction claim is supported**.
Median render objects are 5,343/4,628; primitives remain 2,438,432. The lower HUD
callback cost is evidence consistent with the removed script work; a single pair
with different tick spans does not quantify an isolated whole-frame speedup.
Remaining frame cost is not identified as CPU or GPU by this profile.

Actually opened the after PNG. Minimap border/ridge/dots, HUD and formation remain
visible without new observed clipping. Direct comparison of static minimap strips
(1718,872)-(1900,942) and (1718,992)-(1900,1055) found zero changed pixels across
24,206 pixels; moving dots elsewhere differ as expected at different tick times.
This is bounded static visual preservation, not an actual runtime resize test.
Resize mapping was checked in source only.

The after helper's sampled process 50412 matches the runtime report PID. Sampled
peak resident memory is 364,187,648 bytes; counters include startup/output and are
not per-frame memory samples. Independently matched current EXE/PCK/DLL,
HUD/profiler/helper/project/extension/toolchain files to their prelaunch hashes.
The retained before DLL is byte-identical to the after DLL. Main source changed
after this capture, so this timing belongs to its fingerprinted PCK rather than
an asserted future final package. The primary was notified to package/verify the
later error-handling change. Baseline source wrapper memory remains unusable.

Bounded verdict: the reviewed HUD optimization preserves the inspected static
map/UI and lowers observed HUD script duration, while the instrumentation makes
large presentation costs separately observable. No CPU/GPU diagnosis, actual
resize, selected-army timing, reference performance, responsiveness, complete
game, production visual quality or shipping approval is established.

## Final package and controls verification

Final package source includes early validation and nonzero exit for invalid
profile arguments or missing report output. The reviewer re-read this change.
The source drift noted above is resolved in the final packaged evidence below.

Ran the independent verifier once over all three final reports into
`artifacts/presentation-final-integrity-2026-09-20.json`. Each report passes raw
integrity/summary checks and all ten corruption tests. Independently verified all
ten prelaunch fingerprint entries in each host report against current files,
including main/HUD/profiler, EXE/PCK/DLL and toolchain; all 30 comparisons match.
Runtime and sampled host PIDs match for each final run. All measured frames were
focused and nonminimized; camera size stayed 27, selection stayed empty, and no
units died. Optional PNG capture occurs after measurement.

| Final ordinary package | Measured ticks | Measured duration s | Wall p95 / p99 ms | Present p95 ms | HUD draw p95 ms | Sampled peak resident bytes |
| --- | --- | --- | --- | --- | --- | --- |
| Foundry 12 | 7 to 23 | 0.801756 | 2.794 / 3.249 | 0.082 | 0.165 | 222,687,232 |
| Scale128 200 | 26 to 154 | 6.399793 | 19.787 / 20.400 | 1.615 | 1.948 | 255,201,280 |
| Scale128 500 | 62 to 462 | 20.038359 | 61.222 / 63.897 | 6.906 | 4.501 | 321,359,872 |

Each row contains 60 warmup plus 360 measured frames. The very short Foundry
window has only 16 measured simulation calls and must not be described as
sustained combat performance. The scale windows likewise remain ordinary starting
formations/AI movement with no complete battle. Engine reports vsync mode 1 and
max_fps 0; measured wall interval is not proof of monitor presentation cadence.
The 200/500 frame gates remain failed, and no reference-hardware gate passes.

Final report hashes:

- `presentation-package12-final-2026-09-20.json`:
  `125369ce6d9406aedad22d078e417b4e569d6e8686a7a75aca762416d60081ec`.
- `presentation-package200-final-2026-09-20.json`:
  `bfb012c7fd427cc4102fe3a78e486a2e7b7220b894bef7eb3191f895927b48ad`.
- `presentation-package500-final-2026-09-20.json`:
  `4e2a6276cc1519d1fa447271f05b9a93c48a4812c73322dcae0af4e0a6d23dfe`.

Actually opened all three final PNGs. Foundry retains its visible unit health
bars and rectangular minimap. Scale128 retains square minimap/ridge/team dots;
healthy unselected unit bars are hidden as intended. No new observed UI clipping.
The bright repeated unit bodies and blockout terrain remain production debt.

Also inspected both actual selected-army images from
`presentation-scale-controls-2026-09-20{,-wide}.png`. Selected health bars/rings
appear at camera sizes 27 and 96; full-health enemy bars remain hidden. Wide bars
are short individual marks. Dense choke bodies, bars and rings still overlap
visually; no production-readability acceptance is justified. Both minimaps retain
the ridge/gap and corresponding team dots. These camera zoom checks are not an
actual OS-window resize test.

The scale report has 23 passing checks, no errors, 701 consecutive tick hashes,
and 250 selected units. Inputs occur at event ticks 13 (Move), 28 (Stop), 39
(resume), applying at 14/29/40. Eleven stopped samples show no drift. Report
SHA256 `9bd237c4ed2ecf55c944c89b6f148cb55be3ff04329c9323ce293783b7402b02`
matches `artifacts/presentation-scale-replay-2026-09-20/summary.json`.
Independently matched current Debug/Release executable fingerprints, reconstructed
the exact canonical VFR bytes, and compared all eleven complete replay trace files
with the recorded 700-tick client stream. The retained verifier summary records
350,500 swept ledger rows and eight rejected corruptions; this reviewer did not
execute a second ledger audit. This preserves existing control/Stop/replay safety
evidence, not opposing-stream arrival, manual responsiveness or timing acceptance.

Final verdict remains bounded acceptance of ordinary stage instrumentation and
the narrow HUD optimization. No remaining blocker was found in this scope.
Large-frame performance, crowd progress, full RTS loop/content, human input,
physical networks, production art/animation/audio and all shipping gates remain
open. No COMPLETE.md or AAA acceptance is supported.

# Independent presentation/performance review - 2026-09-25

Scope: controlled pose diagnostics and static world batching, with actual retained
runtime evidence. Reviewer owns only this report; no Git, builds or Godot launches.
Read AGENTS, SPEC, PROGRESS, TESTING, roadmap and prior presentation review. STOP
was absent; the primary owned the active marker. This is not a shipping review.

## Findings and corrections

The prior wall-frame observations do not identify animation, skeleton CPU or GPU
cost. A controlled repeated-pose intervention is useful, but freezing a pose is
not representative animation playback or an acceptable product optimization.
Requested manual AnimationMixer callback mode, exact bone-pose hashing, one seek
per frame rather than seek plus an extra advance, suppression of skipped-time
beam/damage transients, and an overview camera covering the central ridge.
Inspected the resulting source: these concerns are addressed. Controlled input is
ignored, simulation and ordinary actor presentation are frozen, HUD callbacks
continue, and all timing rows retain focus/context/stage data. Actual serialized
bone transforms are hashed before/after, outside the measured interval. This
does not assert every intermediate bone transform was independently sampled.

The independent report verifier checks raw timing accounting, stage containment,
fixed controlled state/hash/camera and boundary snapshots/poses. The comparison
tool requires identical native payload, settings, expanded static geometry and
animation manifests. It correctly rejected the initial implementation pair due
to different DLL fingerprints. The earlier descriptive before/after numbers must
not be promoted to a strictly matched native-payload timing result. The corrected
ABBA sequence below resolves that comparison defect without weakening the tool.

## Static batching source review

The implementation batches repeated world boxes by exact mesh dimensions,
material and 16-unit center-position cells. MultiMesh instances use translation
only, retaining the original BoxMesh vertices, normals and UVs. Singleton boxes,
floor, unique borders, unit meshes, rigs, selection rings and dynamic beam/tween
creation retain their original paths. Material/shadow/layer/GI defaults agree.
Long grid strips still span the map; center-position grouping does not make every
batch's actual bounds 16 by 16. Shared bounds submit a few additional primitives.
This is a tradeoff in culling and submission, not fewer visible terrain polygons.

Actual expanded geometry inventories agree at 1,090 boxes and SHA256
`7da1ffb6c4f10273c8e1fbcd029a5e7894160b413869b707cd0516cfae94c44f`.
MeshInstance3D count falls from 2,093 to 1,010, plus 256 MultiMeshInstance3D nodes.
All 500 AnimationPlayers, 500 Skeleton3D nodes and 7,500 bones remain. No Blender,
skin, animation, simulation or network change is part of the optimization.

## Actual timing and image evidence

Independently ran the report verifier on the ordinary old-package baseline:
`artifacts/presentation-ordinary500-before-2026-09-25.json` passes 360 measured
frames, 526 measured ticks and ten corruption cases. Its frame p95/p99 is
97.679/106.838 ms, substantially worse than the preceding week's observations.
Runtime/host PID 30288 agrees; sampled resident peak is 322,383,872 bytes. The
loaded PCK is the previous package; edited source hashes in the host report are
checkout context only. Host workload was not isolated. The primary reported
another user automation active; this reviewer did not independently measure its
resource consumption. No causal claim about that workload is justified.

The initial same-package pose intervention preserves exact pose and geometry.
Refresh versus frozen p50 is 43.891 versus 17.188 ms, with 4,393 observed draw calls
in both. Manual seek CPU p95 is 2.076 ms in refresh. The much larger whole-frame
difference includes downstream engine/render effects and host variation. It does
not isolate skeleton CPU, GPU skinning or ordinary animation-playback cost.

Strict native-matched runs are
`artifacts/presentation-refresh500-matched-{a1,b1,b2,a2}-2026-09-25.json`, where A
uses the unbatched instrumented PCK and B the final batched PCK. Independently read
every report/host record, recomputed image differences and inspected raw counters.
All four use DLL SHA256
`B234E1FA1BC58A190951861D0FBA0B0A4920262C3F50B33FA28943E49AF148BE`, the same engine,
500-unit tick-40 snapshot, camera and pose digest
`d340d5fdea35190dc2e1bf6265c854e0eb972fca8479db5851933760dc94de2f`.

| ABBA sample | Mean frame ms | p50 ms | p95 ms | p99 ms | Observed draw calls |
| --- | ---: | ---: | ---: | ---: | ---: |
| A1 | 53.530 | 51.310 | 66.830 | 70.774 | 4,393 |
| B1 | 52.013 | 51.043 | 57.755 | 62.090 | 4,164 |
| B2 | 53.427 | 52.207 | 61.779 | 66.299 | 4,164 |
| A2 | 57.319 | 56.791 | 67.118 | 70.597 | 4,393 |

Baseline drift and two observations per implementation do not support a precise
general speedup estimate. Both batched observations have lower tail intervals in
this bounded sequence. The stable count reduction is 229 draw calls, with 1,080
additional submitted primitives (2,438,048 to 2,439,128). No performance budget
passes. Initial overview counters separately observe 10,779 to 8,715 draws and
1,620 extra primitives; that initial implementation timing pair has the native
payload mismatch and is only descriptive.

Actually opened standard before/after and overview before/after 1920x1080 PNGs.
The standard comparison changes 20 of 2,073,600 pixels, maximum channel change
18, within bounding rectangle (679,319)-(1040,718) in the walker region. The
strict ABBA images repeat exactly within A and within B; both A/B comparisons
have those same 20 pixels. Expanded geometry and bone digests agree; the small
raster difference is observed, not asserted to be bit-identical rendering.
The initial overview pair has zero changed pixels across the full image, covering
ridge/gap, terrain strips, borders, both formations and HUD. No new missing or
clipped terrain is visible. Static screenshots do not test moving-camera culling,
continuous animation/transition quality, responsiveness or crowd movement.

Bright tightly packed walkers and largely empty blockout terrain remain visibly
far from production quality. Team colors are small at overview distance. These
findings predate the batching and are not resolved by draw-call reductions.

## Final ordinary observations and verdict

Read final ordinary reports `presentation-ordinary{12,200,500}-final-2026-09-25`
and their host files. Each retains 360 measured frames, focused flags and matching
runtime/sampled PID; hidden launches still do not establish unoccluded human play.

| Units | Measured ticks | Frame p95 / p99 ms | Sampled resident peak bytes |
| --- | --- | --- | ---: |
| 12 | 24..144 | 17.128 / 17.336 | 233,840,640 |
| 200 | 29..188 | 25.696 / 27.531 | 244,510,720 |
| 500 | 115..821 | 130.709 / 137.834 | 313,151,488 |

The 500-unit run reaches deaths (237/247 remain), while earlier ordinary timing
windows covered different simulation intervals. It cannot quantify a causal
regression or improvement. The delivered ordinary frame target remains severely
unmet. Memory observations are process peaks on this development host, not the
reference machine or a complete battle campaign. There is no new movie, manual
input-to-photon measurement or animation-quality acceptance in this review.

Recommend landing the bounded diagnostic capability and scene/draw submission
reduction after the primary's required regressions. No remaining source or static
visual blocker was found in this scope; no general timing-speedup claim is
accepted. Profile actual advancing animation/renderer work next with engine-side
instrumentation and representative ordinary play. Dense crowd progress, full RTS
economy/AI/content, physical networking, reference hardware and production art/
audio/VFX gates remain open. No AAA, completion or shipping approval.

The highest-priority simulation continuation remains purposeful backward
decompression with a retained passing side against the unchanged dense opposing
stream fixture, with Stop/Hold/firing eligibility and full swept safety/progress
checks. Do not repeat undirected lateral fallback or treat these presentation
diagnostics as satisfying the movement milestone.

## Packaged regression evidence inspected

Read `artifacts/presentation-scale-controls-2026-09-25.json`: 23 fixture checks,
no errors, tick 700, 14 stopped samples and no drift. The report records successful
actual selection/group/Move/Stop/resume/restart InputEvent paths. Independently
matched its SHA256 to `artifacts/presentation-scale-replay-2026-09-25/summary.json`
and both current headless executable digests. That retained verifier reports all
700 hashes matching Debug and ten Release playbacks, 350,500 swept unit rows and
eight rejected corruptions. This reviewer inspected the summary and fingerprints,
not a second full swept ledger execution. This is one-sided scripted movement,
not the failed opposing-stream arrival gate or a human response test.

Actually opened both selected-army choke/wide PNGs and the final ordinary Foundry
PNG. Ridge geometry, borders, square/rectangular minimaps, selection bars/rings
and shortcut text remain visible with no new observed clipping. Dense selected
choke silhouettes remain obscured by overlapping bright bodies, bars and rings.
The default `presentation-offline-final-2026-09-25.json` reports passing controls,
combat and restart at 400 ticks, winner 1 and unchanged hash `1b35fbb0222cbc7b`.
Imported/requested clip names do not prove continuous animation quality.

Final `artifacts/presentation-network-final-2026-09-25/summary.json` records six
passing rendered separate-process cases: clean, 80/160 ms RTT with jitter/loss,
withheld frame, incompatible input delay and disconnect. Four successful pairs
reach 240 ticks; mismatch rejects at zero; disconnect preserves 52-tick prefixes.
Reviewer independently checked all five current package/replayer fingerprints
against the summary and all ten clean repeat trace files are byte-identical.
The suite's recorded opposite-build replays remain bounded loopback regression
evidence, not physical endpoints, full matches, sustained pacing or latency
acceptance. The run spans 2026-09-26 00:35:54..00:37:28 UTC.

Read the retained `artifacts/weekly-build-2026-09-25.log`: Debug and Release each
report 7/7 CTest, malformed replay checks and 2,000 identical tick hashes. These
are compilation/regression evidence. The primary confirms no sim/net/bridge/art
or pin changes in the integrated diff. Required bounded regressions are now
recorded; final verdict is acceptance of this narrow increment with every
performance, crowd-progress, production and shipping limitation above retained.

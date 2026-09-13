# Independent scale client review - 2026-09-13

Scope: bounded presentation/adapter access to the existing authoritative Scale128
map. Reviewer owns this report only; no simulation implementation or build edits.
No complete-game, crowd-progress, responsiveness or shipping acceptance is implied.

## Source review

Reviewed main.gd map setup, geometry, camera, order bounds and reset; hud.gd
minimap mapping; extension bridge scale-reset/input/snapshot interfaces;
bridge_scale_test.gd; scale_smoke.gd; verify_scale_capture.py and the reused
independent scale ledger auditor.

The bridge owns the real standalone 128x128 simulation and requested population;
presentation dimensions come from its snapshot. Map configuration precedes terrain
construction. Floor/grid/borders, cell geometry, command bounds and camera clamps
use the active map dimensions. Square-map minimap is 153x153 at the existing
bottom margin, with independent x/z scale factors. Scale/network combinations
reject; ordinary reset remains Foundry and scale R retains Scale128/count.

The packaged fixture uses actual InputEvents through production F2, group save /
recall, right-click movement beyond both former map bounds, S and R. Selection
clearing before recall is presentation-only setup. Stop positions are saved from
the event-tick snapshot, preceding the first application tick. Every authoritative
tick is captured from the production loop, so several ticks in one frame cannot
silently disappear from the replay trace. The independent verifier reconstructs
canonical inputs, compares full traces across both builds/repeats, and audits
unit/terrain interpolation intervals with the existing independent ledger oracle.

## Evidence finding sent before captures

The first verifier allowed Stop and resume at the same event tick and trusted
report.ok; its reused ledger auditor then consumed both commands without a stopped
interval. Require at least nine ticks between Stop and resume, bind application
and stop-tick fields to event_tick+1, and reject a collapsed-stop mutation. The
actual fixture already waits nine samples, but the independent verification
must enforce the advertised span.

R is tested while completed=true, keeping the new state at exact tick zero.
This establishes reset state/map/count/groups, but does not by itself establish
that a restarted ordinary match subsequently advances and accepts fresh orders.
The bridge contract separately checks post-reset command/replay progression;
that is adapter evidence rather than a restarted rendered input playtest.

## Pending evidence

Packaged 200/500-unit captures, current binary/report fingerprints, independent
trace/ledger results, raw timing distributions and actual screenshots pending.
Frame intervals include rendering/capture work; bridge.advance timing excludes
snapshot conversion and rendering. Headless scale timings cannot be substituted
for rendered large-map performance. The scripted one-sided Move/Stop/resume case
cannot pass the unchanged opposing-stream crowd-completion gate.

## Verified packaged evidence and final bounded verdict

The Stop-span finding is fixed in the final verifier: canonical resume is at
least nine ticks after Stop; first-application identities must match event+1;
collapsed-stop and wrong-stop-tick mutations reject. Both current reports pass.

Actually inspected all four current images:
`artifacts/scale-client-{200,500}-2026-09-13{-wide,}.png`, 1920x1080.
The large ridge and central opening, separated starting armies, 128x128 HUD
population labels and square minimap are visible. Choke close-ups show the
one-sided friendly stream on both sides of the ridge. No HUD/minimap clipping
was found. These still images do not establish full-map simultaneous visibility,
correct animation over time, input responsiveness or crowd completion.

The wide view exposes severe readability debt: fixed-width health bars merge
into cyan/orange horizontal stripes over the dense formations. Choke views show
bright cream meshes and overlapping bars/rings obscuring silhouettes and exact
spacing. The current geometry remains blockout terrain. This is useful diagnostic
access, not production RTS readability approval.

Independently inspected both JSONs: each has 23 passing fixture checks, no errors,
701 consecutive tick hashes, 700 bridge-advance timing samples and three canonical
Move/Stop/resume inputs. Actual Stop spans are eleven ticks in both cases:
200 units event26/app27 through resume event37; 500 units event29/app30 through
resume event40. First-tick baselines and nine-tick minimum are therefore present.

Both `artifacts/scale-client-{200,500}-replay-2026-09-13/summary.json` reports finish
successfully with one Debug and ten Release replays each, all eight deliberate
evidence corruptions rejected, and independent swept ledgers of 140,200 / 350,500
rows. This reviewer independently recomputed both report SHA256s, matched current
Debug/Release executable SHA256s, reconstructed and compared exact canonical VFR
bytes, and compared all 22 full replay trace files against each packaged trace.
No second full ledger execution was needed or claimed.

Raw timing arrays were independently checked for finite nonnegative values and
nearest-rank percentiles recomputed:

| Total units | Frame samples | Frame p95 / p99 / max ms | Bridge p95 / p99 / max ms | Sampled peak resident bytes |
| --- | --- | --- | --- | --- |
| 200 | 689 | 77.606 / 102.389 / 431.139 | 0.278 / 0.353 / 0.482 | 281,518,080 |
| 500 | 304 | 149.874 / 162.589 / 467.608 | 0.980 / 1.030 / 1.232 | 368,816,128 |

The normal captures expose a severe rendered-frame budget failure at both sizes,
while bridge stepping is relatively inexpensive. Frame intervals include all
rendering and capture overhead and are not CPU-only frame profiles. Both renders
used the development host, not the SPEC reference hardware. No frame-budget,
manual latency or shipping gate passes. No measurement filtering or target
weakening is warranted. Snapshot conversion, actor/skeleton update, draw calls
and overlays need separate profiling before selecting an optimization.

Verdict: accept only the bounded existing-Scale128 client/adapter access and its
verified canonical input, Stop, reset-state, replay and safety evidence. No
remaining blocking source/evidence finding in that narrow scope was found.
Opposing-stream recovery remains unchanged and failed; this one-sided fixture
cannot replace it. Restarted rendered ticking after R, manual play, movie-based
animation inspection, production readability and representative frame budgets
remain open. No complete-game or shipping approval.

## Final HUD correction and replacement captures - 2026-09-13

The prior figures above are the initial 2026-09-13 capture baseline and remain
retained. Final accepted package captures are now explicitly:
`artifacts/scale-client-final-{200,500}-2026-09-13.json`, matching `.png`,
`-wide.png`, and `-host.json`. Replay evidence is under
`artifacts/scale-client-final-{200,500}-replay-2026-09-13/`.

Final HUD source scales selected/damaged unit health bars with zoom in Scale128
only, while preserving the legacy Foundry bar behavior. Actually inspected all
four replacement 1920x1080 images. Wide selected bars are now individual short
marks rather than formation-wide horizontal stripes; full-health unselected enemy
bars are hidden. This resolves the specific overview overlay defect. Dense choke
units still visually merge through bright bodies, overlapping bars and selection
rings; production silhouette/readability remains unaccepted. Map geometry, HUD
labels and square minimap remain visible without observed layout clipping.

Final reports again pass all 23 fixture checks, no errors, 701 tick hashes and
700 bridge samples each. Actual Stop spans are nine ticks (200: event26/app27,
resume35) and eleven ticks (500: event27/app28, resume38). Independent verification
again recomputed report/current Debug+Release SHA256s, reconstructed exact VFR
bytes and compared all 22 complete replay trace files. Both final summaries pass
all eight corruptions and report independently audited ledgers of 140,200 and
350,500 rows. The earlier fixture traces differ in event timing because inputs
were dispatched in separate rendered runs; no identical-stream or causal timing
optimization comparison is claimed.

Raw final nearest-rank observations, recomputed by this reviewer:

| Total units | Frame samples | Frame p95 / p99 / max ms | Bridge p95 / p99 / max ms | Sampled peak resident bytes |
| --- | --- | --- | --- | --- |
| 200 | 717 | 77.408 / 101.006 / 368.545 | 0.264 / 0.345 / 0.483 | 282,304,512 |
| 500 | 303 | 152.062 / 168.804 / 446.913 | 0.973 / 1.020 / 1.200 | 369,451,008 |

The rendered-frame failure remains severe. The overlay correction is a verified
visual improvement, not a performance fix. Current verdict remains bounded
Scale128 client/adapter/input/Stop/reset-state/replay/safety acceptance only, with
no blocking source/evidence finding in that scope. Dense opposing-stream progress,
rendering budgets, full game, manual input responsiveness, production art and
shipping remain unaccepted. The primary is separately verifying final default
controls/offline/network regressions; this addendum does not claim their results
before those runs finish.

### Final measurement-attribution correction

The current 12-unit Foundry control also reports poor frame intervals:
`artifacts/scale-client-offline-regression-2026-09-13.json` passes at tick400 with
frame p95/p99 68.984/72.206 ms and bridge p95/p99 0.046/0.055 ms. This reviewer
read that report and separately matched all five files in
`artifacts/scale-client-final-fingerprints-2026-09-13.json`, including the final
package EXE/PCK/bridge and both replayers.

Consequently the scale frame observations above must not be attributed solely
to army size or product rendering cost. The capture helper launches with
WindowStyle Hidden; hidden-window scheduling, capture/fixture overhead, and
CPU/GPU workload are not isolated. The current results fail to demonstrate the
frame budget under the captured conditions. They do not prove a CPU or GPU
bottleneck, nor reproduce foreground human-play performance. Next performance
work needs a controlled foreground run and separately measured frame stages,
with the same no-filtering and unchanged-budget requirements. The primary reports
no older game process was present; only its current owned network pair existed.
No unsupported process-contention explanation is adopted.

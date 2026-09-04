# Independent simulation and smoke review — 2026-09-03

Reviewer: determinism_critic, separate agent. Scope: AGENTS.md, SPEC.md, TESTING.md, PROGRESS.md, roadmap, sim/**, extension/bridge.cpp, client/main.gd, and artifacts/debug-smoke.json. No implementation or Git changes. STOP absent at review start and final report update. This report is the sole owned file.

## Findings resolved in this run

1. P2, Sim::submit formerly rejected referenced units with hp <= 0 at receipt, making future-command acceptance depend on packet arrival timing. The fix removes receipt-time health validation while retaining ownership/existence checks and apply-time dead-unit no-op. New late_receipt_after_death regression submits an identical tick-1000 command early and at tick 950 after its subject dies, then compares buffered/executed hashes. Independently inspected the change and regression; integrated Debug and Release tests pass.

2. P2, client smoke formerly ignored combat_damage, accepted merely an AnimationPlayer, and treated hash advancement as useful gameplay evidence. Stages advanced without acceptance checks. The fix requires accepted Move/Stop/AttackMove, unchanged positions during ticks 47–54 following Stop, actual damage, all five clips on every actor, and requests for walk/attack playback. Inspected code and Debug smoke JSON. The artifact records accepted_orders [1,0,2], stop_without_drift true, combat_damage true, all five animations, moved_samples 2322, tick 400, winner 1, and ok true. This resolves the original acceptance false positives for the documented programmatic skirmish smoke.

Residual evidence caveat: played_clips / observed_clips is populated immediately after AnimationPlayer.play(). It proves existing clips were requested, not that playback time advanced or bones visibly deformed. Video/capture inspection or explicit playback/bone telemetry must establish actual animation. Root notified. No additional blocking P1/P2 implementation defect was found in the fixes.

## Deterministic architecture assessment

Authoritative code has no Godot dependency or floating arithmetic/wall-clock reads. Coordinates, fixed movement increments, BFS neighbor order, entity order, command order, serialization and RNG use explicit integer operations. Damage accumulation permits mutual kills. Bridge converts bounded integers to canonical commands and reads snapshots; it does not use Godot physics/navigation. These are source-level foundation findings, not proof of network determinism.

Pending command bytes participate in Sim::hash. Identical executed world states with different future receive buffers can hash differently. Current whole-state hash requires identical closed command horizons; future network synchronization should compare an executed-state checksum or guarantee identical buffered horizons. sim/README.md now explicitly documents the limitation.

Combat acquisition observes earlier entity movement and later entities' old positions in the same tick. This is deterministic but side-dependent; sim/README already discloses the limitation. Balance and responsiveness need runtime evidence.

## Actual checks and limitations

Initial review independently ran existing build/sim-agent-clean Debug and Release test executables. After fixes, independently ran build/windows/sim/Debug/voidfront_sim_tests.exe and build/windows/sim/Release/voidfront_sim_tests.exe. Both integrated executables returned exit 0 with replay_hash=9497079902275560851 and winner=1. Root owns compilation; reviewer executed the resulting binaries without modifying build directories.

Tests exercise malformed/truncated command bytes, incompatible protocol byte, duplicate IDs/commands, invalid ownership, future bounds, reordered arrival, a single-unit ridge route, immediate Stop, twelve-unit AI combat/spacing, two in-process replay instances and late receipt after death. A twelve-unit spacing assertion does not establish robust crowd routing. No evidence for opposing streams through narrow chokes, dynamic construction blockers, unreachable crowd goals, Hold separately, 128x128 battle budgets, loss/jitter/latency, actual process-to-process transport, full match economy/AI or saved replay diagnostics exists in the reviewed tests. These are explicit roadmap gaps, not newly discovered regressions.

The reviewed Debug runtime artifact identifies RTX 2070 SUPER and reports frame p95 16.7003 ms / p99 18.265 ms, bridge-advance p95 0.262 ms / p99 0.415 ms, and Godot static-memory peak 46,347,471 bytes. These small-skirmish, Debug, aggregate frame-delta measurements are not representative production CPU frame-time, resident-memory or 200/500-unit performance evidence. The stated p99 16.67 ms frame target is not demonstrated by this capture.

No screenshots/video, packaged smoke or human playtest were directly inspected by this reviewer. Root is producing package evidence and assigning visual review separately. No visual, responsiveness, performance or shipping approval is granted. Project remains a foundation prototype and must not create COMPLETE.md.

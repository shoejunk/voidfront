# Verification ledger

## 2026-09-11 spatial broadphase preservation

`./tools/verify.ps1` passes integrated MSVC Debug/Release and 5/5 CTest per
configuration, 15 malformed replay and seven output-alias cases/configuration,
2,000-tick cross-configuration equality and ten repeat playbacks. Pinned engine,
templates and godot-cpp checks pass. Log: `artifacts/broadphase-build-2026-09-12.log`.
After adding exact boundary cases, the Debug spatial test was rebuilt/retested
(`artifacts/broadphase-boundary-debug-2026-09-12.log`); Release included these.
Spatial regressions exhaustively compare candidate coverage over 100 seeded
500-unit layouts, including current target positions and conservative sweeps.
Explicit cases cover map/query extremes, reversed insertion IDs, output clearing,
collision offsets +/-191/192 and acquisition offsets +/-1568.

Before editing, ran the Release headless executable for 2,000 ticks with
`--units-per-team 100` and `250`, retaining full traces/logs/source in
`artifacts/crowd-baseline-2026-09-12`. After builds/tests ended, repeated both
commands into `artifacts/broadphase-2026-09-12`. `tools/compare_traces.py` passes
all 2,000 hashes in each comparison. Baseline versus post-change p95/p99:
200 units 2.440/2.933 -> 0.965/1.532 ms; 500 units 15.259/16.373 -> 3.898/5.240 ms.
These are whole-harness active ticks on the 32x24 prototype map/development host,
including AI/command work and excluding trace hash/write. Both scenarios remain
ongoing at tick 2,000. No representative 128x128, crowd-progress, new packaged
runtime or reference-hardware acceptance follows from this isolated optimization.

Build and test instructions and measured results are added as working increments land. All commands run from C:\dev\voidfront in PowerShell. A compilation result proves only compilation. Headless simulation tests do not prove networking or visual quality. A screenshot does not prove input response or stable frame time.

Required validation: MSVC Debug/Release sim regressions and cross-config traces; Godot version/import check; C++ extension in packaged build; Blender skeleton/clips and actual running animation; screenshots inspected at game distance; independent deterministic and visual/playability critics. Future milestones add separate-process network impairment, complete AI matches and representative battle profiling.

Evidence goes in ignored artifacts/; durable results and commands go here and in PROGRESS.md. No shipping gates have passed yet.

## 2026-09-03 verified foundation

Reproduce from the project root:

```powershell
./tools/bootstrap.ps1
./tools/export_assets.ps1
./tools/verify.ps1
./tools/package.ps1
./tools/capture.ps1 -Packaged -Name packaged-smoke -Ticks 400
./tools/capture.ps1 -Packaged -Movie -Name packaged-animation -Ticks 400
```

`build.ps1` verifies pinned Godot version, executable/template hashes and godot-cpp revision before configuring CMake and MSVC. It normalizes duplicate PATH/Path keys from the desktop host and uses one MSBuild node, with compiler parallelism inside godot-cpp. Godot automation uses artifacts/godot-profile to preserve the user's editor settings; Windows templates are copied and hash-checked. Import/export exit codes AND script/resource errors are fatal. One precisely recognized sandbox diagnostic remains: `ERROR: Failed to read the root certificate store.` This run does not use runtime TLS; all other runtime errors are fatal to capture verification.

Godot observed: 4.7.2.stable.official.ed1daf0bf. Windows release template independently reports the same version. Compiler: MSVC 19.38.33145.0, VS2022 Professional tool directory 14.38.33130; SDK 10.0.26100.0. Blender 5.1.2 ec6e62d40fa9. Dependency rationale: official godot-cpp explicitly supports its 4.7 API target; a narrow RefCounted/OS profile avoids generating the entire engine API. See https://github.com/godotengine/godot-cpp/tree/05057de73de4b99f114d36c40d84ca46926c0e25 and https://docs.godotengine.org/en/4.7/tutorials/scripting/cpp/index.html.

### Determinism and replay

Integrated MSVC Debug and Release CTest: 1/1 executable passes in each configuration. Coverage includes canonical serialization/truncation/version bounds, invalid ownership/IDs/duplicates, command sequence/tick ordering, ridges and blocked goals, immediate stop, twelve-unit spacing, combat/AI and repeated replay. Independent critic's arrival-time dead-unit command admission defect was fixed and regressed.

`verify.ps1` also invokes distinct recording and playback processes in each configuration; replay playback consumes recorded canonical inputs and generates no AI. Both 2,000-tick traces match, plus ten repeated Release playback traces. Trace SHA256: B5FBB540C91632A5FDF71952A3E8C6B315526AC3C500FE5F7E171C2ACECFD1B1. Fifteen malformed saved replay fixtures are rejected per configuration, and a valid empty-input replay remains idle. `tools/compare_traces.py` reports the first differing trace line/tick. Evidence: artifacts/verification.log and artifacts/determinism/{Debug,Release}/record.vfr, record.trace, replay.trace. These are separate file-playback processes, **not communicating network peers**.

### Packaged runtime and art

artifacts/package/Voidfront.exe loads its adjacent PCK and Release C++ bridge. The final normal capture passes all assertions: click selects one, drag selects six, Move/Stop/AttackMove accepted, no stop drift, actual combat damage, five imported clips with idle/walk/attack/hit/death requests, and restart recreates initial state. 400-tick terminal hash b559cd93ea152fc0; scripted player loses. The movie run reaches the same hash and passes. This is programmatic input, not a manual playtest.

Artifacts: packaged-smoke.json, packaged-smoke-host.json, packaged-smoke-engine.log, packaged-smoke.png, packaged-smoke-battle.png. Screenshots are 1920x1080. packaged-animation.avi and MP4 have 1,202 frames at 60 FPS, 20.033 seconds, **1600x900**: Godot's movie writer uses the project's base viewport despite the requested window size. Do not call the movie native 1080p. animation-overview.png, walk-contact.png and combat-contact.png are extracted sequences used for actual visual inspection. Movie encoding took longer than real time; its frame timings are excluded from performance claims.

Blender validation: 15 joints, normalized skin weights, UV/normal attributes, five clips, 8,888 triangles and five surfaces. Source: art/cairn_walker.blend; reproducible generator/validator plus GLB. Imported animation and moving limbs/corpse poses are visible in recorded sequences. This does not establish production deformation, foot contact or transitions. Lighting, corpse visibility and outcome panel were improved after screenshot criticism. Final restart no longer logs GLES3 null-material errors after sharing cached presentation materials. No 2D raster assets were needed in this milestone; HUD is native controls/drawing.

### Measurements and limits

Development machine: Ryzen 7 3700X, RTX 2070 SUPER, 68,644,352,000 bytes physical RAM visible to Windows; not the reference target. Normal 12-unit package, requested 1920x1080 window: p95/p99 wall-clock frame intervals **17.075/17.503 ms**, bridge advance **0.046/0.084 ms**, peak resident working set **231,268,352 bytes**, sampled via Windows Process.PeakWorkingSet64. Frame intervals include vsync/scheduling; advance timings exclude snapshot extraction. Godot static allocator monitor returns zero in Release and is unavailable, not zero memory use. The frame p99 exceeds 16.67 ms; target is unchanged. CPU frame-time percentiles and input-to-response latency have not been measured.

Standalone active-tick preliminary Release results: 200 units p95/p99 314/638 us over 939 active ticks; 500 units 1132/1694 us over 1992 active ticks. Both use the prototype 32x24 map and may include concurrent-build noise. They do not meet the required 128x128 benchmark or establish large-battle rendering/crowd quality.

Independent reports: .voidfront-agent/review-simulation-2026-09-03.md and review-visual-2026-09-03.md. No independent shipping approval. Official SC2 screenshots could not be retrieved for direct side-by-side comparison; that evidence remains missing.

Not executed/implemented: real network exchange, latency/jitter/loss, complete economic AI or 1v1 matches, production-map performance, manual input responsiveness, dynamic-blocker/opposing-flow crowd suite, complete content, audio or production animation polish. No COMPLETE.md is justified.

## 2026-09-06 recovered network checkpoint

The earlier ledger above describes the foundation at its date. This run resumes saved dev commit 96e56a9 and preserves its implementation. Godot 4.7.2.stable.official.ed1daf0bf, matching template hashes and pinned godot-cpp revision passed the existing build guards.

### Replay evidence preservation

`./tools/verify.ps1` passes integrated MSVC Debug/Release builds and 2/2 CTest executables per configuration, fifteen malformed replay fixtures per configuration, 2,000-tick cross-configuration trace equality, and ten repeat playbacks. It now also runs `tools/verify_output_aliases.py`: seven Windows cases per configuration require nonzero exit, the expected alias diagnostic and preservation of original bytes (exact path, hard link and case variant for replay/record, plus case aliases of nonexistent output). A disposable pre-fix fixture demonstrated that replay/hardlink output was accepted. Fixed in 24b4842. Logs: `artifacts/output-aliases-before.log`, `artifacts/replay-alias-verification-2026-09-06.log`.

### Packaged local skirmish regression

`./tools/package.ps1` and `./tools/capture.ps1 -Packaged -Name packaged-network-regression -Ticks 400` pass. Current package remains `artifacts/package/Voidfront.exe`. Programmatic click/drag selection, Move/Stop/AttackMove, combat damage, no stop drift and restart pass; terminal hash remains b559cd93ea152fc0. Five clips are imported/requested. This is local offline gameplay, despite the capture filename; Godot networking is not attached. No new animation movie or manual playtest was run.

Primary and independent runtime critic actually inspected both current 1920x1080 screenshots. Overbright walker surfaces, crowded selection indicators, blockout terrain, incomplete animation evidence and ambiguous defeat wording remain open. No unexpected engine error was logged; only the recognized certificate-store diagnostic remains. Report: `.voidfront-agent/review-runtime-2026-09-06.md`. Logs: `artifacts/package-2026-09-06.log`, `artifacts/capture-2026-09-06.log`, `artifacts/packaged-network-regression{,-host}.json`, matching engine log and screenshots.

Observed normal twelve-unit frame interval p95/p99 3.036/3.611 ms, bridge advance 0.036/0.056 ms and sampled resident peak 235,040,768 bytes over 21.774 seconds. Unchanged client code and unnormalized presentation/scheduling conditions do not establish an optimization relative to the previous ~17 ms run. These are neither CPU-frame/input-latency measurements nor representative 128x128, 200/500-unit battle results; SPEC budgets remain unchanged.

### Separate-process UDP lockstep

Final command: `python tools/verify_network.py --out artifacts/network-verified-2026-09-06`. All eleven cases pass with distinct Windows peer processes and real UDP relaying. Current evidence: `artifacts/network-verified-2026-09-06/summary.json`, initial `run.json`, per-case reports/logs, canonical VFR2 records and traces; console log `artifacts/network-verified-2026-09-06.log`. UTC run interval is 2026-09-06 17:29:21 through 17:33:51. Binary SHA256 fingerprints are recorded for both configurations of peers/replayers.

| Case | Executed ticks per peer | Result |
| --- | ---: | --- |
| Debug/Debug and Release/Release clean | 1,000 | All hashes agree; 49.08/49.03 seconds including confirmation linger and replay work |
| Debug/Release, 80 ms RTT, 20 ms one-way jitter, 1% loss | 1,000 | 82/73 actual random drops by direction; all hashes agree; 143.52 seconds |
| Release/Debug, 160 ms RTT, 20 ms one-way jitter, 1% loss | 1,000 | 106/101 actual random drops; all hashes agree; 220.83 seconds |
| Forced terminal loss | 100 | Twelve Finish/FinishAck packets dropped across both directions; recovery succeeds |
| Withheld frame | 100 | Eleven copies of player 1 tick 25 withheld for 0.5 seconds; no packet indicating advancement beyond held tick |
| Lost tick ACK | 100 | Three tick-25 ACKs dropped; overtaking frame/retry recovery succeeds |
| Incompatible content and protocol | 0 | Both peers fail before advancement with incompatibility diagnostics |
| Injected desync and disconnect | 25 | Both peers stop with diagnostic; applied failure prefixes replay identically |

Every successful pair's recording replays in the opposite configuration. The four 1,000-tick configurations/impairment traces agree with the clean baseline and ten repeated Release playbacks; trace SHA256 `3cda78f81a85a1a9f45175d5566a5090db3216c1b7394b70ecb892fcbc68823e`. Three malformed VFR2 cases reject changed content, truncated content identity and trailing data. Full suite includes seeded random duplication/reordering and records actual relay delays; this is controlled loopback impairment, not a physical network benchmark.

The interrupted implementation initially failed this run because the Python relay treated Windows UDP `ConnectionResetError`/10054 notifications as fatal. The C++ peer already handled the corresponding notification. A fresh fifty-tick probe reproduced the harness failure; after the narrow relay repair all eleven probe and full cases pass. Reset handling is actually exercised (clean cases one notification; full negative cases up to 43 per direction) while authoritative progress timeout still rejects disconnects. Other socket errors remain fatal. Per-case failures now print immediately. Default output is a new timestamped directory under `artifacts/network/`; explicit `--out` must not exist. A rejected reuse preserved the prior summary hash (`artifacts/network-reuse-rejected-2026-09-06.log`). Pre-repair evidence in `artifacts/network/` and `artifacts/network-probe-2026-09-06` must not be substituted for the fresh final suite.

Independent network source and executed-evidence review: `.voidfront-agent/review-network-2026-09-06.md`. This accepts only the bounded transport/replay foundation. `stall_count` counts completed turns and `stall_ms` sums whole elapsed turns, including handling; neither is pure network-stall or input latency. The stop-and-wait harness has no real-time pacing and cannot sustain 20 Hz at 80/160 ms RTT (roughly 7/4.5 ticks per second including harness overhead). Next: scheduled input delay, a bounded pipeline, tick-tagged lagged checksums, measured 20 Hz/response behavior, and then two packaged clients. Complete economic games, 30-minute network soak, LAN/Internet transport, representative performance and all production/shipping gates remain open.

## 2026-09-06 scheduled pipeline and timer correction

Protocol 2 closes future commands without a future state hash. Peers sample AI from deterministic source state t and schedule its commands for t+2 in the verification suite; initial frames are empty. Input receipt acknowledgements and tick-tagged executed checksums are separate. Execution can proceed without an input ACK, but at most 16 states may remain beyond contiguous mutual checksum confirmation. All executed checksums must be confirmed before successful terminal exchange. Both negative and successful sessions retain only applied commands in VFR2 recordings. No transport clock enters sim/.

`./tools/verify.ps1` passed integrated MSVC Debug/Release builds and 2/2 CTest each, fifteen malformed legacy replay fixtures/configuration, seven output-alias preservation cases/configuration, 2,000-tick cross-configuration equality and ten repeated replays. Log: `artifacts/pipeline-build-fixed-2026-09-06.log`. Added simulation tests schedule AI at delays 1, 2 and 16, round-trip wire data, deliver reversed/duplicate frames, and compare every executed hash with an independent canonical-input Sim. The first new test failed because its reference used default seed 1 while Lockstep used 42; explicit seed 42 corrected the fixture. Subsequent peer-only Debug/Release builds and both CTest suites passed after final CLI/comment changes.

The first 50-tick, 14-case process probe passed protocol checks but exposed a pacing defect: approximately 16.00 Hz, clean tick p95 63.54 ms and 80 ms RTT generated-command p95 188.56/187.53 ms. Preserved evidence: `artifacts/pipeline-probe-2026-09-06/summary.json`. Added balanced process-scoped timeBeginPeriod(1)/timeEndPeriod(1) and absolute 50 ms scheduling; genuine missing-data stalls reset the schedule. Ordinary scheduler lateness may produce a shorter phase-correction interval; missed full slots do not cause unbounded catch-up. Timing remains outside the simulation. The corrected short probe (`artifacts/pipeline-probe-fixed-2026-09-06/summary.json`) passes all 14 cases and raw timing recomputation, but its 80 ms RTT first-command sample still reaches 152.00 ms. It is not final acceptance evidence.

Peer reports preserve raw tick deadlines, execution times, lateness, intervals and local command source/execution ticks, sequences and generation/application times. The verifier recomputes interval and command percentiles, scheduled tick mapping, elapsed rate, backlog bounds and confirmation count. Strict timing-target booleans are distinct from protocol success. Sparse tick-aligned AI commands omit human polling phase, client dispatch and rendering; the first command's measured delay includes initial frame warmup. None of these samples proves mouse-to-screen responsiveness.

### Packaged local regression

`./tools/package.ps1` and `./tools/capture.ps1 -Packaged -Name packaged-pipeline-regression -Ticks 400` pass. Current package is `artifacts/package/Voidfront.exe`. Selection, Move/Stop/AttackMove, no stop drift, combat, imported/requested five clips and restart pass with unchanged terminal hash `b559cd93ea152fc0`. Both 1920x1080 screenshots were inspected by primary; independent review is recorded separately. Existing overbright walkers, dense selection cues, blockout terrain and ambiguous Signal Lost defeat text remain. No new movie, animation transition validation or manual playtest occurred; the Godot client still runs offline only.

Current twelve-unit frame interval p95/p99: 17.006/17.412 ms; bridge advance p95/p99: 0.048/0.070 ms; sampled peak resident memory: 242,659,328 bytes. Frame p99 misses 16.67 ms. These are scheduling/vsync intervals, not CPU-frame or human input measurements and not representative battle performance. Evidence: `artifacts/packaged-pipeline-regression{,-host}.json`, screenshots and engine log, plus `artifacts/pipeline-{package,capture}-2026-09-06.log`. PCK SHA256 remains `4F0C729801EE6D86ABD9B1C52A56DDF6F6D9AEEEDC0058EB730778E633E17379`; packaged bridge SHA256 is `BA0B56C92F0C0BDF94BD97A9642F8ECFF3B7268D6B14C161CCA9F6140BBE5D9F`.

### Initial buffer readiness correction

The first complete isolated pipeline suite at `artifacts/pipeline-verified-2026-09-06/summary.json` passed all 15 protocol cases but measured 80 ms RTT command p95 163.7463/131.9515 ms. All twelve local command samples were retained: source tick 0 was the slow sample; the other player-0 samples were approximately 99-102 ms. Checkpoint `8e17ab9` preserves that functional increment and its explicit timing miss.

The peer now enters an explicit initial readiness phase: receive every remote initial empty frame and actual receipt ACKs for every local initial frame before sampling source-zero canonical input or starting its first pacing deadline. It keeps sending/retrying during setup and retains a progress timeout. Nonempty initial frames reject. Setup duration is separately visible as session_ready_ms/startup_duration_ms; command timestamps are neither reset nor filtered. The new verifier checks all command samples occur after readiness, requires source-zero/sequence-one input still present, and adds a withheld initial ACK case to prove the transition. Both final peer configurations build and their 2/2 CTest suites pass. This changes only the headless transport harness; the packaged local runtime evidence above remains applicable.

### Final isolated process evidence

`python tools/verify_network.py --ticks 1000 --jobs 1 --out artifacts/pipeline-ready-verified-2026-09-06` passes all **16 cases**. Final evidence is `artifacts/pipeline-ready-verified-2026-09-06/summary.json`, with per-case PIDs, actual relay impairments, reports, raw timing, applied VFR2 inputs and traces; console log is `artifacts/pipeline-ready-verified-2026-09-06.log`. Run interval: 2026-09-06 19:08:33 through 19:13:08 UTC. Network cases ran one at a time after builds and packaged capture ended. This is still controlled loopback, not physical-network/reference-hardware profiling.

| Case | Executed ticks each | Paced Hz, player 0/1 | Command p95 ms, player 0/1 | Actual random drops, direction 0/1 |
|---|---:|---|---|---|
| Debug clean | 1,000 | 19.99937 / 19.99941 | 102.082 / 102.443 | 0 / 0 |
| Release clean | 1,000 | 19.99986 / 19.99988 | 101.709 / 102.188 | 0 / 0 |
| Mixed, 80 ms RTT, +/-20 ms one-way jitter, 1% loss | 1,000 | 19.99472 / 19.99979 | 102.558 / 101.880 | 116 / 106 |
| Mixed, 160 ms RTT, +/-20 ms one-way jitter, 1% loss | 1,000 | 19.75528 / 19.76406 | 101.414 / 130.312 | 197 / 202 |

Each peer supplies twelve sparse, tick-aligned canonical AI command samples, including source tick zero. At 80 ms RTT the measured generated-command p95 meets 150 ms; startup is separately 130.949/147.050 ms. At 160 ms, startup is 256.089/271.509 ms and missing-data stalls total 503/487 ms. The 160 ms rate remains below 20 Hz with a two-tick input delay. Strict at-least-20-Hz booleans remain false even in the nearly-20-Hz clean cases; no tolerance was silently substituted for the target. This accepts a bounded timing improvement, not the full human/network response gate. An 80 ms phase-correction interval reached 18.560 ms after scheduler lateness; no claim that every interval is 50 ms or that all response jitter is solved.

The four 1,000-tick traces and ten repeated playbacks agree, SHA256 `b4aac2b20ec6ec7ae028341a6488abbd00938e62a3e00b3d6f0c19561a8d0776`. Every positive peer recording replays in the opposite build configuration. Three malformed VFR2 fixtures still reject. Build fingerprints in the summary identify all four peer/replayer binaries.

Fault evidence: terminal loss drops twelve Finish/FinishAck packets; withheld frame drops thirteen copies and observes no executed state beyond 25 during the hold; lost frame ACK drops three copies and the relay observes state 26 before releasing the held ACK. Withheld initial ACK drops fourteen copies, delaying that peer's readiness beyond 500 ms without any gameplay frame/checksum from it before readiness. Withheld checksum drops twenty-seven copies and exercises the exact 16-tick lag limit with 312/311 ms stalls; lost checksum ACK drops three copies and recovers. Content/protocol/input-delay mismatches reject at zero. Injected desync stops at applied prefixes 25/27, final-state desync at 100/100, and disconnect at 27/25; all nonzero prefixes independently replay and common prefixes agree. Final-state desync cannot be reported as successful completion.

Independent reviews: `.voidfront-agent/review-pipeline-2026-09-06.md` and `.voidfront-agent/review-runtime-pipeline-2026-09-06.md`. These cover actual source/evidence and local screenshots, not shipping approval. Next: test a larger agreed delay for 160 ms RTT against an identically delayed clean baseline while retaining the 80 ms latency evidence; then a reusable nonblocking session and two packaged clients with explicit readiness/stall/disconnect/outcome UI and actual human-input measurements. Any-angle movement, complete economy/AI/matches, 30-minute networking, production content and representative performance remain mandatory. No COMPLETE.md.

Final incremental pin/build/test evidence: artifacts/pipeline-readiness-build-2026-09-06.log records the final Godot version guard, both peer targets and 2/2 CTest per configuration. This post-suite log capture made no source changes or recompilation; its binaries retain the final suite fingerprints.

## 2026-09-07 nonblocking session and matched delay study

`./tools/verify.ps1` passes integrated MSVC Debug/Release builds and **3/3 CTest**
per configuration, fifteen malformed legacy replays and seven output-alias cases
per configuration, 2,000-tick cross-configuration equality and ten repeated
playbacks. Evidence: `artifacts/session-build-2026-09-07.log`. Godot
4.7.2.stable.official.ed1daf0bf, matching template hashes and the recorded
godot-cpp SHA pass the existing guards. No simulation, client, art or tool pins
changed. Compilation is not gameplay evidence.

The new direct Session API CTest passed in 3.84 seconds Debug and 3.79 seconds
Release. It tests caller-provided empty/custom canonical input against a separately
authored Move/Hold schedule and serialized applied-frame replay in a plain Sim;
exactly-once eligible source sampling; at most one tick per overdue poll; rejected
ownership/player and provider exceptions without authoritative advancement;
exclusive socket binding and immediate reuse after cancel/error/completion or
destruction; terminal idempotence; and retained applied frames when a checksum
error follows advancement within the same poll. These are paired sessions in one
test process, not separate-process networking evidence or a poll-time benchmark.

The CLI now uses `vf::net::Session` for transport and retains its evidence files,
AI provider, scoped Windows timer resolution and Sleep(1) loop. The library
exposes read-only simulation/statistics and explicit lifecycle states. It drains
at most 256 datagrams and advances at most one tick per poll, without sleeping or
waiting for transport. Provider/simulation work and OS scheduling remain outside
any wall-time guarantee. The Godot client is still offline; no new packaged
playtest, screenshots, animation, input-to-display or frame-time capture was run.
Prior presentation evidence remains dated evidence, not a fresh verification.

### Complete command timing coverage

`verify_network.py` now matches every command timing event's execution tick and
sequence to every applied local command in its VFR2 recording, including failed
prefixes. This closes the independent reviewer's gap where internally consistent
timing arrays could omit a later command. `artifacts/command-coverage-regression-2026-09-07.log`
checks all 32 prior peer recordings and rejects deliberately omitted, duplicated,
reordered and wrong-sequence timing events. The twelve frozen baseline recordings
also pass (`artifacts/delay-baseline-coverage-2026-09-07.log`). Timing begins after
the command provider returns; original input queue residence, provider cost,
human polling phase and presentation are unmeasured.

### Pre-extraction control study

After the clean baseline build/test completed, its four peer/replayer executables
were copied to `artifacts/session-baseline-binaries-2026-09-07`. Then
`python tools/verify_network.py --suite delay-study --ticks 1000 --build artifacts/session-baseline-binaries-2026-09-07 --out artifacts/delay-baseline-2026-09-07`
ran six isolated profiles. All passed; no build/capture ran concurrently. Each
impaired case has a clean control with the same input delay and player/build
assignment. The suite checks every same-delay trace, opposite-build recording
playback and ten repeated replays per delay. Different delays deliberately have
different canonical streams and are not compared for hash equality.

| Profile | Input delay | Paced Hz, player 0/1 | Missing-data stall ms, player 0/1 | Generated-command p95 ms, player 0/1 |
|---|---:|---|---|---|
| Mixed clean, 80 ms control | 2 | 19.99935 / 19.99934 | 0 / 0 | 101.855 / 101.672 |
| Mixed 80 ms RTT | 2 | 19.99757 / 19.99673 | 4 / 2 | 101.685 / 101.437 |
| Mixed clean, 160 ms control | 2 | 19.99670 / 19.99755 | 0 / 0 | 101.394 / 112.573 |
| Mixed 160 ms RTT | 2 | 18.81376 / 18.81937 | 2293 / 2227 | 133.318 / 103.138 |
| Mixed clean, 160 ms control | 4 | 19.99924 / 19.99971 | 0 / 0 | 201.739 / 201.173 |
| Mixed 160 ms RTT | 4 | 19.99957 / 19.99962 | 0 / 0 | 201.323 / 201.801 |

Impaired cases use +/-20 ms one-way jitter and 1% loss, with 231/405/403 total
random drops in the 80 ms/two-tick 160 ms/four-tick 160 ms cases. Four-tick startup
at 160 ms is 287.163/273.252 ms, separately retained. Four ticks improved observed
pacing at the cost of about 100 ms more scheduled response. This is an explicit
configuration experiment; defaults remain two ticks, both peers must agree, and
there is no automatic latency-based negotiation. Strict at-least-20-Hz assessments
remain false; no target was weakened. Sparse tick-aligned twelve-unit skirmish
commands do not establish human response, physical-network performance, complete
matches or a 30-minute soak.

Baseline trace SHA256 by input delay: two ticks
`b4aac2b20ec6ec7ae028341a6488abbd00938e62a3e00b3d6f0c19561a8d0776`;
four ticks `db91bcbf2beb5bb497c2b4f21c0175bbf7a1e206da3619cb0a6697cd4d324734`.
Reports, raw timing, PIDs, actual relay impairments and executable fingerprints
are in `artifacts/delay-baseline-2026-09-07/summary.json` and its case folders.

### Final extracted-session process regression

`python tools/verify_network.py --ticks 1000 --jobs 1 --out artifacts/session-verified-2026-09-07`
passes all **22 cases** from 2026-09-07 15:04:59 through 15:13:16 UTC. Evidence:
`artifacts/session-verified-2026-09-07/summary.json`, its per-case reports/recordings/
traces and `artifacts/session-verified-2026-09-07.log`. All eight ordinary profiles
execute 1,000 ticks per peer; bounded fault fixtures run shorter prefixes. No
build, API test or packaged capture overlaps these isolated network cases.

| Profile | Input delay | Paced Hz, player 0/1 | Missing-data stall ms, player 0/1 | Generated-command p95 ms, player 0/1 |
|---|---:|---|---|---|
| Mixed clean, 80 ms control | 2 | 19.99983 / 19.99982 | 0 / 0 | 101.793 / 101.201 |
| Mixed 80 ms RTT | 2 | 19.99569 / 19.99488 | 7 / 8 | 101.490 / 101.364 |
| Mixed clean, 160 ms control | 2 | 19.99956 / 19.99966 | 0 / 0 | 101.709 / 102.151 |
| Mixed 160 ms RTT | 2 | 19.73730 / 19.73503 | 547 / 553 | 104.331 / 104.502 |
| Mixed clean, 160 ms control | 4 | 19.99938 / 19.99962 | 0 / 0 | 201.554 / 202.245 |
| Mixed 160 ms RTT | 4 | 19.99954 / 19.99935 | 0 / 0 | 201.292 / 201.416 |

Debug/Debug and Release/Release clean profiles also pass, with no stalls and
approximately 19.9995/19.9996 Hz. Every ordinary peer has twelve applied-command
timing samples, all matched to the recording. The three impaired profiles incur
231, 398 and 397 actual random drops respectively. Startup is 149.851/138.256 ms
at 80 ms and 271.734/279.101 ms at four-tick 160 ms. Complete raw metrics and
same-delay/build control deltas are retained. At 80 ms, generated-command p95 is
below 150 ms; every strict at-least-20-Hz assessment remains false. Four ticks
again remove observed missing-data stalls for this 160 ms run at the cost of
about 100 ms more command delay. Different OS/packet scheduling between runs
prevents attributing baseline-versus-extraction timing variation to an optimization.

Both delay trace SHA256 values exactly match the pre-extraction study above.
Every positive recording replays in the opposite configuration; ten repeat
playbacks pass for each delay, and three malformed VFR2 fixtures reject. The
verifier checks full command timing coverage on all 44 peer recordings, including
failed zero/nonzero prefixes. Current executable fingerprints are recorded in
the summary; no binaries were rebuilt after this suite started.

Faults preserve the existing behavior: terminal loss drops twelve Finish/FinishAck
packets; withheld input blocks execution beyond its tick; lost input ACK still
allows execution before its release; withheld startup ACK blocks that peer's
canonical input until readiness. Withheld checksums hit exactly the 16-state
verification cap at both delays (350/350 ms stalls at two ticks; 353/353 ms at
four ticks). Lost checksum ACKs recover. Protocol/content/input-delay mismatches
reject at zero; desync retains 25/27-tick applied prefixes, final-state desync
retains 100/100 and cannot report completion. Disconnect ends at 27/25 ticks with
delay two and 29/25 with delay four, respecting scheduled-input bounds. Every
nonzero failed prefix independently replays and common peer prefixes agree.

The reviewer independently audited all 44 reports/recordings, all 288 applied
local command events, raw timing calculations, repeats, failure prefixes, four
current executable fingerprints and both frozen-baseline hashes. The held-startup-ACK
case retains a 529.120 ms command p95 on the already-ready counterpart: readiness
is local buffer readiness, not a simultaneous start, and the fault sample is
neither filtered nor claimed to meet ordinary response targets.

Independent source/evidence review: `.voidfront-agent/review-session-2026-09-07.md`.
This is acceptance of the bounded session extraction and retained protocol/replay
behavior only. Godot networking, actual input-to-execution/display measurements,
strict pacing, LAN/Internet, complete economic matches, 30-minute soak, any-angle
pathfinding, production presentation and representative performance/shipping
remain open. No fresh runtime/visual approval or COMPLETE.md is justified.

## 2026-09-07 packaged loopback client integration

This entry supersedes the earlier offline-only client status. The bridge now owns
a nonblocking Session; the Godot client queues canonical commands for its selected
player and interpolates authoritative snapshots. Normal offline play keeps its
original command path. A network session has explicit handshake, buffer readiness,
running, stalled, final confirmation, complete and error states. Active shared
matches reject local restart; terminal R returns that window to offline play.

Reproduction:

```powershell
./tools/verify.ps1
. ./tools/common.ps1
Invoke-ProjectGodot @('--headless','--path',"$Repo/client",'--script','res://bridge_network_test.gd')
./tools/package.ps1
python tools/verify_client_network.py
./tools/capture.ps1 -Packaged -Name packaged-client-offline-regression -Ticks 400
```

Integrated MSVC Debug and Release each pass 3/3 CTest, legacy malformed replay
and output-alias preservation checks, cross-configuration 2,000-tick equality and
ten repeat playbacks. Log: `artifacts/client-network-build-2026-09-07.log`.
Pinned engine/templates/godot-cpp checks pass. Time is added to the narrow bridge
API profile to share Godot's exact microsecond clock with event ingress.

The direct Godot bridge regression passes with the Debug extension:
`artifacts/bridge-api-2026-09-07.log`. It rejects invalid player, pre-ready input,
enemy/unknown unit, invalid order/destination, queue slot 65 and unschedulable tail
input. All 64 accepted queue slots execute with consecutive identities; both
bridge states agree. Terminal rejection, socket reuse and offline reset/step
also pass. This is a same-process adapter test, distinct from packaged UDP proof.
Queue capacity is 64; replay evidence is capped at 100,000 commands/64 MiB.
Exceeding replay storage fails visibly with incomplete-evidence status. The client
keeps normal-play transition history bounded and only collects per-event/render
telemetry in network smoke mode.

The package harness launches two rendered Windows processes through a bounded
loopback UDP relay, retains PIDs/binary fingerprints/actual packet losses and
checks every tick hash plus every applied recording in both build configurations.
Every original accepted event is joined by identity to queue timing, canonical
recording, executed input and post-draw observation, including failure prefixes.
Ten repeated Release playbacks of the new clean recording also run. The harness
rejects reused evidence directories and unexpected runtime errors. Zero-tick
failure headers are validated but not replayed: the VFR2 reader intentionally
requires a nonzero applied prefix.

Synthetic F2, right-click Move, S Stop and A+click AttackMove run through
Input.parse_input_event. Timing starts at `_input`, before unhandled dispatch and
queuing; it excludes physical input and the OS-to-Godot event queue. `executed_usec`
is the first bridge observation after Session.poll returns, including poll and
provider cost. Display timing observes frame_post_draw after the first positive
snapshot interpolation; it is software rendering evidence, not monitor photons.
Feedback timing observes the order ring; Stop still places that ring at the map
origin, so useful unit-local stop acknowledgement is not established. Only three
orders per peer/profile are sampled. No sample is removed or retried to improve
latency. Human responsiveness, strict pacing, representative battle performance,
LAN/Internet, 30-minute soak and full RTS content remain unaccepted.

The first full harness attempt is preserved at
`artifacts/client-network-verified-2026-09-07`: four positive cases passed before
an overly strict mismatch assertion failed. Actual mismatch reports show one
peer rejecting incompatible input delay at tick zero and the other timing out
in handshake at zero after the rejecting peer closed. The corrected assertion
requires both zero-prefix errors and at least one exact incompatibility diagnostic;
it accepts that asymmetric handshake timeout without claiming both peers received
the mismatch. No simulation or transport rule was weakened.

### Final packaged evidence

`python tools/verify_client_network.py --out artifacts/client-network-final-2026-09-07`
passes **6/6 cases**, 2026-09-07 19:46:16 through 19:47:50 UTC, followed by ten
clean-recording repeats. All four positive pairs confirm 240 ticks; mismatch
fails at zero (both peers receive incompatibility in this final run), and the
disconnect retains 52/51-tick prefixes after the relay cuts communication.
Every common prefix agrees; twenty nonzero Debug/Release replay traces match.
All twelve recordings and 28 applied local commands have complete identity and
timing coverage. Evidence: that folder's `summary.json`, per-peer JSON/VFR/log/PNG,
per-case `verification.json` and `artifacts/client-network-final-2026-09-07.log`.

| Profile | Delay ticks | Event to observed execution p95 ms, player 0/1 | Event to first rendered state p95 ms, player 0/1 | Ring feedback p95 ms, player 0/1 |
| --- | ---: | --- | --- | --- |
| Clean | 2 | 133.957 / 133.871 | 166.652 / 166.736 | 16.555 / 16.577 |
| 80 ms RTT, 20 ms jitter, 1% loss | 2 | 133.749 / 133.874 | 166.673 / 166.654 | 16.604 / 16.599 |
| 160 ms RTT, 20 ms jitter, 1% loss | 4 | 250.795 / 234.510 | 283.503 / 266.842 | 16.591 / 16.541 |

Only three orders per peer/profile: these nearest-rank p95 values are sample
maxima, not stable population estimates. Actual random drops are 54/76 at
80/160 ms; withheld-frame fixture drops thirteen copies over 0.6 seconds and
exposes stalled state without advancing past the missing turn. Disconnect drops
1,206 packets. Paced rates at 80 ms are 19.873/19.928 Hz with 66/33 ms stalls;
strict 20 Hz is unmet. There is no matched clean four-tick client control, so this
run makes no comparative delay optimization claim. All samples remain retained.

Additional `--case clean --ticks 1000 --out artifacts/client-network-combat-2026-09-07`
passes: both packages confirm 1,000 ticks, both recordings replay in Debug and
Release, and ten repeats match. Both report winner 1; actual terminal screenshots
show defeat for player 0 and victory for player 1. This demonstrates a scripted
combat skirmish outcome, not an economic RTS match, balance or a 30-minute soak.
Session completion still waits for the configured tick cap/confirmation after
combat victory. Network terminal R was source-reviewed and bridge reset tested;
the packaged network-to-offline R input path has not been exercised.

Packaged offline capture passes all existing assertions and unchanged hash
`b559cd93ea152fc0`, including click/drag/move/stop/combat/restart and imported clip
requests. Logs: `artifacts/client-offline-capture-2026-09-07.log`,
`artifacts/packaged-client-offline-regression{,-host}.json` and matching PNGs.
Frame interval p95/p99 is 16.871/17.243 ms; bridge advance 0.044/0.065 ms; sampled
peak resident memory 242,016,256 bytes. Twelve units at 1920x1080 on the existing
development host; frame p99 still misses 16.67 ms. Network captures use 1280x720
with two rendered processes; neither capture establishes representative hardware
or large-battle budgets. No new movie or manual playtest was performed.

Primary and independent critic inspected actual final network active/error/outcome
and offline screenshots. Lifecycle/role/outcome text is readable. Overbright
walkers/rings, narrow queued formations, blockout terrain, ambiguous defeat wording
and production presentation debt remain. Independent review:
`.voidfront-agent/review-client-network-2026-09-07.md`. Same-poll post-advance
desync recording is covered in the underlying Session API but still lacks a
Godot adapter fault fixture. No shipping acceptance or COMPLETE.md is justified.

## 2026-09-07 bounded any-angle movement (bounded increment verified)

`./tools/verify.ps1` passed integrated MSVC Debug/Release **4/4 CTest** each,
15 malformed replay and seven output-alias cases/configuration, 2,000-tick
cross-configuration hashes and ten repeat playbacks. Evidence:
`artifacts/any-angle-build-fixed-2026-09-07.log`. Godot 4.7.2, template hashes
and godot-cpp pin remain verified. The initial build failed on a probe/test API
mismatch; retained in `artifacts/any-angle-build-2026-09-07.log`.

Independent review derived a valid tangent route that component truncation could
permanently stop: (3968,3920) to (4570,1835). The corrected integrator considers
neighboring integer samples with stable error ordering, bounded speed, strict
waypoint progress and full swept clearance. The simulation regression covers
this fixture and off-center arrival, direct non-axis/non-45-degree travel,
obstacle routes, instant Stop and live retarget. A natural two-unit combat fixture
also verifies attack-move resumes after firing: clearing a route now invalidates
its cache key. That additional test passes Debug/Release in
`artifacts/any-angle-resume-test-2026-09-07.log`. The previous unproven speed-loss
comment was removed. Candidate sampling over deltas [0,500]^2 observed a minimum
full-step length 30.610456 versus nominal32; this limited calculation is not a
global uniform-speed bound. Final short arrival steps and collision waits differ.

`python tools/verify_navigation.py --out artifacts/navigation-verified-2026-09-07`
passes **51 fixtures x10 runs in each configuration**, with byte-identical route
JSON and zero observed worst/mean route-length excess against the independent
Euclidean reference. Predeclared tolerance is 1% plus two coordinate units;
contract: `.voidfront-agent/navigation-contract.md`. The oracle uses a different
candidate superset and exact rational edge-event/midpoint collision, cross-checked
with slabs on 4,000 random segments plus fixture pairs. Analytic open, box,
long-detour, passage and unreachable cases validate it separately. Inputs,
per-fixture results, raw routes and binary hashes are retained in that directory.
This is bounded axis-aligned rectangle geometry: the angled fixture is a staircase,
the capacity fixture uses64 duplicate boxes, and the 1024 exposed-vertex cap is
not stress-tested. No dynamic/crowd, arbitrary polygon or performance acceptance.

Simulation compatibility version2 rejects older movement recordings. VFC wire1
is unchanged; navigation sources join the automatic VFR2/network content hash.
Old recorded hashes are intentionally not expected to match changed movement.

### Separate-process regression after movement changes

`python tools/verify_network.py --ticks 1000 --jobs 1 --out artifacts/any-angle-network-2026-09-07`
passes all 22 cases, 20:09:28-20:17:47 UTC. Eight ordinary pairs execute1,000 ticks;
all fault, content/protocol, input-delay, missing-frame/checksum/ACK, terminal,
desync and disconnect cases retain expected behavior. All44 VFR2 recordings,
436 applied local-command timing identities and38 nonzero replay traces were
independently reviewed. Common prefixes agree, ten repeats per delay match, and
four current peer/headless binary fingerprints match the summary. Delay2 trace
SHA256774f5fff200d480c75dffe38ad9866f372df995218be0623f71396bd0faa37fe;
delay 4 a923285a4ced2818b59321803405b4c5264644bbeb53197c8037f8d2fd2d3bc8.

At 80 ms RTT with 20 ms jitter/1% loss, generated command p95=101.468/100.993 ms,
paced rate19.97374/19.98119 Hz and231 random drops. At 160 ms delay 2,
p95=157.013/166.737 ms and19.06791/19.06941 Hz. Delay4 reaches
19.99952/19.99956 Hz with 201.091/201.406 ms p95 and400 random drops.
Matched controls remain in the same report; no pacing tolerance, sample filtering,
default-delay change or optimization claim was introduced. CLI timing excludes
original human input/queue/display and does not establish the full response gate.
Builds, package capture and benchmarks did not overlap this suite.

### Packaged exact movement and recording

Rebuilt pinned package with `./tools/package.ps1`. Normal command:
`./tools/capture.ps1 -Packaged -Movement -Ticks 400 -Name packaged-movement-2026-09-07`.
The new fixture uses actual InputEvents and the production order conversion,
disables only enemy AI for this fixture, and retains six canonical order attempts
plus all400 unit-position rows. Independent
`python tools/verify_movement_capture.py artifacts/packaged-movement-2026-09-07.json --out artifacts/packaged-movement-verified-2026-09-07.json`
checks complete data, every swept segment with the separate midpoint oracle,
maximum speed32,184 non-axis/non-45-degree steps, instant Stop and nine stationary
ticks, real movement before Stop/live retarget, actual ridge crossing and exact
arrival at all three completed legs. Actual integrated path-length excess is
0.018590% direct,0.013368% detour and0.013346% retarget. This includes stepping
variation, separately from the static graph's zero observed error. Arrival ticks
are76,170,219. This is one scripted walker, not crowd or human play acceptance.

Independent verifier review initially found weak assertions allowing omitted
coordinates and fake Stop/detour evidence. Corrected checks now reject all 25
adversarial cases; log `artifacts/movement-verifier-review-2026-09-07.log` records
the source/report hashes and results. Source review also caught the two simulation
regressions described above, both now covered by passing C++ tests.

`-Movie -Name packaged-movement-movie-2026-09-07` passes the same400-row verification.
Canonical event ticks differ by one from normal capture due frame/input dispatch;
spatial routes and their lengths agree. No identical-stream claim across these
separate input recordings. Converted movie:
`artifacts/packaged-movement-2026-09-07.mp4`,1600x900,60 fps,1201 frames,20.016667s.
Primary and independent reviewer inspected actual ridge/detour PNGs and the
12-frame,1 fps extracted contact `artifacts/movement-contact-2026-09-07.png`.
Oblique departure and ridge/retarget progression are visible, with no obvious
teleport in sampled frames. Detailed deformation, temporal smoothness and manual
responsiveness remain unverified; movie timings are encoding-distorted.

Normal movement frame p95/p99=16.997/17.369 ms, bridge advance0.027/0.031 ms,
resident peak238,157,824bytes. Existing offline400 tick capture
`artifacts/packaged-any-angle-offline.json` passes click/drag/groupMove/Stop/
AttackMove/combat/clip requests/restart; changed hash f2870e230c766422 and winner-1
at 400. Offline frame17.071/17.443 ms, bridge0.040/0.055 ms, resident235,126,784bytes.
Both normal captures use1920x1080 on the recorded development machine and twelve
walkers (only one moving in the dedicated fixture). Framep99 still misses16.67 ms;
no reference-hardware or representative-battle performance acceptance.

### Packaged networking and scale diagnostics

`python tools/verify_client_network.py --out artifacts/any-angle-client-network-2026-09-07`
passes6/6 cases: four positive240 tick pairs, zero-tick mismatched-delay rejection,
and replayable52/52 tick disconnect prefixes. All ten clean repeats pass. At 80 ms
RTT, event-to-observed-execution p95=133.781/133.861 ms and first-render
166.824/166.708 ms; at 160 ms withdelay 4,234.258/233.869 ms execution and
266.826/266.782 ms render. Only three synthetic inputs/player/profile; full human
latency and pacing gates remain open. A separate `--case clean --ticks 1000 --out
artifacts/any-angle-client-combat-2026-09-07` passes confirmed1,000 ticks and ten
repeats, with winner1 reported by both clients. Actual outcome screenshots were
inspected: player0 defeat, player1 victory, both complete/confirmed1000. Across
the two suites, all 14 recordings retain Debug/Release replay equality and common
prefix agreement; five current package/replayer fingerprints match their reports.
This is a scripted combat skirmish, not a complete economy/AI RTS match or soak.

After builds, capture, encoding and network sessions ended, ran the Release
headless executable for2,000 ticks with `--units-per-team 100` and250. Current
32x24-map diagnostics: **200 units p95/p99=2.465/2.704 ms;500 units=15.740/16.815 ms**.
Logs: `artifacts/any-angle-{200,500}unit-2026-09-07.log`; traces retained alongside.
Both runs remain winner-1 at2,000 ticks. These are whole-harness active-tick timings
including AI/command work, excluding trace hash/write, not isolated pathfinding
or representative128x128 measurements. The500 unit case already exceeds the4/8 ms
simulation budgets on this smaller map. Conservative waiting and repeated unit
collision scans are unfinished scale/crowd work; neither no-lock nor performance
acceptance is supported. Next: spatial broadphase and deterministically bounded
collision/replanning work with explicit crowd progress regressions, then genuine
128x128 group-order/replan benchmarks. Do not lower the budgets.

Independent final review: `.voidfront-agent/review-movement-2026-09-07.md` accepts
only the bounded any-angle/static-route and transport preservation increment.
Production art/audio/UI, true angled terrain, dynamic blockers/streams/chokes,
full economy/technology/fog/AI, LAN/Internet,30-minute matches, manual controls,
reference hardware, balance and shipping gates remain open. No COMPLETE.md.

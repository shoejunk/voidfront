# Independent session/network review - 2026-09-07

Reviewer: independent source and evidence critic. Scope: tools/verify_network.py, net/session.hpp, net/session.cpp, net/peer.cpp, net/session_tests.cpp and actual build/process artifacts supplied by the primary. No Git operations, builds, peers or helpers were started by this reviewer. This is not a gameplay, art, performance-budget or shipping approval.

## Source review

The delay study uses matched tick count, player/build assignment and input delay for each clean/impaired comparison. Trace baselines are partitioned by input delay, appropriately allowing canonical AI streams to differ between delays. Strict >=20 Hz and <=150 ms at 80 ms RTT assessments remain separate from protocol success. Startup/readiness and source-zero command timing are retained; the measurement still begins after the provider returns and excludes original input queue residence, provider cost, human polling and presentation.

An initial evidence gap was reported: internal timing-array consistency did not prove every applied local command was timed. The primary added verify_command_coverage, which reads the local command tick/sequence list from each applied VFR2 recording and compares exact event order/count. The reviewed artifacts/command-coverage-regression-2026-09-07.log records validation of 32 earlier recordings, including failed prefixes, plus rejection of omitted, duplicated, reordered and wrong-sequence samples. This closes that reported gap.

The extracted Session keeps clocks and Windows UDP outside sim/. Its provider is explicit, receives const authoritative state, closes empty turns and has canonical/player/ownership admission before execution. There is no hidden default AI. The socket is nonblocking; each poll drains at most 256 packets and advances at most one tick. Provider/simulation work has no wall-time bound, so this is transport-nonblocking, not a frame-time guarantee. Readiness, future input scheduling, independent tick-tagged checksums, the 16-state verification cap, retries, final confirmation and linger remain visible in source.

poll() retains applied_frames when a checksum failure follows a successful advance in the same call. peer.cpp consumes those frames before throwing and finalizes the replay to the executed prefix. Completion, failure, cancellation and destruction release Socket/WinSock lifetime; terminal repeated polls do not advance. Cancellation is represented as a terminal Error with a diagnostic and does not notify the remote peer, which times out. Current transport remains Windows loopback only.

The direct API test source checks custom and empty providers against independent plain-Sim schedules/replays; exactly-once sampling; malformed provider ownership/player and exceptions without tick-zero mutation; overdue polls advancing at most one tick; active-port exclusivity and reuse after cancellation/error/completion/destruction; terminal idempotence; and post-advance desync retaining replayable frames. These tests are paired sessions in one process, not separate-process transport proof.

No blocking source defect was found in the reviewed implementation. Final executed evidence was independently audited as follows.

## Executed evidence review - 2026-09-07 08:15 America/Los_Angeles

Read artifacts/session-build-2026-09-07.log: the pinned Godot 4.7.2 and godot-cpp SHA/API guards pass, integrated MSVC Debug and Release builds complete, and all 3/3 CTest tests pass in each configuration. The log also records legacy malformed replay/output-alias rejection, 2,000-tick cross-configuration equality and ten repeated playbacks. The direct session API output is present in build/windows/Testing/Temporary/LastTest.log. The reviewer inspected these executed logs; the reviewer did not run builds or tests.

The frozen pre-extraction delay study at artifacts/delay-baseline-2026-09-07/summary.json passes six isolated 1,000-tick profiles. Its same-delay hashes match the extracted session's final results. On that earlier run, four ticks at 160 ms RTT reduced observed stalls from 2293/2227 ms to 0/0 ms, with generated-command p95 rising from 133.318/103.138 ms to 201.323/201.801 ms. This is an observed buffer/response tradeoff, not a claim that extraction optimized throughput; operating-system/relay variation is visible between runs.

Final evidence: artifacts/session-verified-2026-09-07/summary.json, 2026-09-07 08:04:59 through 08:13:16 America/Los_Angeles. All 22 separate-process cases pass. Independently recomputed with read-only PowerShell from final raw files: all four current executable SHA256 fingerprints match the summary; 44 peer reports/VFR2 recordings contain exactly 288 applied local command timing events; every reported command identity and order agrees with its recording; source-plus-delay mapping, readiness, execution timestamp, latency sample, nearest-rank command/tick percentiles, raw deadlines/lateness, intervals and elapsed pacing agree. All successful and nonzero failure-prefix replay trace hashes agree, as do every pair's common applied prefix. Both delays' ten repeated traces and all ordinary case traces match their frozen same-delay baselines. No command sample was discarded by this audit.

| Final profile | Paced Hz, players 0/1 | Generated-command p95 ms | Aggregate stalls ms | Startup ms | Random drops |
|---|---|---|---|---|---|
| 80 ms RTT, delay 2 | 19.995692 / 19.994882 | 101.490 / 101.364 | 7 / 8 | 149.851 / 138.256 | 119 / 112 |
| 160 ms RTT, delay 2 | 19.737300 / 19.735028 | 104.331 / 104.502 | 547 / 553 | 271.685 / 265.524 | 196 / 202 |
| Matched clean, delay 4 | 19.999381 / 19.999621 | 201.554 / 202.245 | 0 / 0 | 20.892 / 14.882 | 0 / 0 |
| 160 ms RTT, delay 4 | 19.999538 / 19.999351 | 201.292 / 201.416 | 0 / 0 | 271.734 / 279.102 | 196 / 201 |

Same-delay 1,000-tick trace SHA256:
- Delay 2: b4aac2b20ec6ec7ae028341a6488abbd00938e62a3e00b3d6f0c19561a8d0776.
- Delay 4: db91bcbf2beb5bb497c2b4f21c0175bbf7a1e206da3619cb0a6697cd4d324734.

Fault evidence retains substantive behavior: 12 forced terminal drops recover; delayed frame holds execution; lost input/checksum ACKs recover; withheld startup ACK postpones the affected peer's readiness to 545.915 ms. The other peer's resulting 529.120 ms p95 remains visible, demonstrating that readiness is local buffer readiness, not synchronized simultaneous input acceptance. Both delay-2 and delay-4 withheld-checksum cases reach the exact 16-state cap, observing no state later than 40 while checksum 25 is held. Content/protocol/delay mismatch rejects at zero. Injected desync retains 25/27 tick prefixes, final-state desync fails at 100/100 with only 99 confirmed, and disconnect prefixes are 27/25 (delay 2) and 29/25 (delay 4). These common prefixes and replays independently agree; mismatch diagnostics identify state 25 or 100 on the detecting peer, while its partner times out.

## Verdict and remaining gates

Accept this bounded session extraction and matched-delay experiment as source/build/API/loopback regression evidence. No unresolved blocking defect was found within that scope. The reported strict >=20 Hz boolean is false for every ordinary profile, including clean runs; no tolerance has been substituted. Four-tick input delay brings measured canonical response to about 201 ms and must not silently replace the two-tick 80 ms profile or waive the 150 ms target. Startup and impairment outliers remain visible.

There is still no attached Godot multiplayer path, packaged networked-client match, human input-to-execution/display measurement, 30-minute complete match, physical LAN/Internet evidence, reference-hardware performance proof or bounded wall-time poll budget. Memory allocation is match-bounded rather than a fixed-size production ring. Full RTS content/AI/economy, any-angle pathfinding, production art/audio/UI and independent shipping gates remain open. No gameplay/AAA/shipping approval is given. Next: integrate two packaged clients with readiness/stall/disconnect UI and original human enqueue timestamps, retaining these protocol and replay checks.

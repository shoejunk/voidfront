# Independent pipelined lockstep review - 2026-09-06

Reviewer: independent agent `network_critic`. Primary owns Git, integration, builds and harness executions; reviewer owns only this report. Read AGENTS.md, checked STOP, and read SPEC.md, PROGRESS.md, TESTING.md and the continuing roadmap before review. Scope is the headless UDP pipeline and its evidence, not client networking, visual quality, complete matches or shipping.

## Verdict

Accept this bounded headless pipeline checkpoint. Final source and the fresh 16-case readiness suite have been inspected: separate processes exchange future canonical input, verify bounded lagged executed hashes, recover the tested losses, reject tested incompatible/faulted sessions and preserve replayable applied prefixes. No blocking correctness finding remains in this reviewed scope.

The measured generated-command response target passes for this 80 ms RTT fixture. **The strict 20 Hz and full human input-response gates remain open.** Observed rates are slightly below 20 Hz at 80 ms and around 19.76 Hz at 160 ms; phase correction includes an 18.56 ms interval. Twelve tick-aligned local AI command samples per peer do not establish human input latency, complete matches or client networking readiness.

## Final executed evidence

Final evidence is exclusively `artifacts/pipeline-ready-verified-2026-09-06/summary.json`, associated run metadata, per-peer reports/logs/records/traces, case results and `artifacts/pipeline-ready-verified-2026-09-06.log`. Run interval: **2026-09-06 19:08:33.168037 through 19:13:08.812521 UTC**, with `--ticks 1000 --jobs 1`. Earlier directories document failures and intermediate versions rather than final acceptance.

Independently matched all four current peer/headless executable SHA256 values to final metadata. Independently hashed all **26** full-length trace files: four peer/opposite-configuration replay files in each of four 1,000-tick cases, plus ten repeated replays. All equal `b4aac2b20ec6ec7ae028341a6488abbd00938e62a3e00b3d6f0c19561a8d0776`. The verifier additionally replays successful 100-tick fault-recovery cases and each nonzero failed prefix, and rejects changed-content, truncated-content and trailing-byte VFR2 files.

Final checkpoint log `artifacts/pipeline-readiness-build-2026-09-06.log` was independently inspected: the pinned Godot version guard passes, both MSVC peer targets build, and both CTest executables pass in Debug and Release. The incremental check did not change the four suite binary hashes, which were rechecked afterward. The earlier integrated verification log additionally covers the client extension and full replay/alias regression suite.

Values separated by slashes refer to players/directions 0 and 1. Timings are directly measured per-peer intervals and canonical generation-to-execution events; process startup, final confirmation linger and replay work are excluded from the pacing calculation.

| Case | Distinct process IDs | Measured rate, Hz | Local command p95, ms | Startup, ms | Random drops |
|---|---|---|---|---|---|
| Debug clean | 40452 / 6664 | 19.999375 / 19.999413 | 102.0818 / 102.4429 | 17.4715 / 14.8688 | 0 / 0 |
| Release clean | 30476 / 33740 | 19.999857 / 19.999878 | 101.7089 / 102.1882 | 17.9729 / 13.9240 | 0 / 0 |
| Mixed, 80 ms RTT | 47556 / 25656 | 19.994718 / 19.999785 | 102.5575 / 101.8803 | 130.9485 / 147.0495 | 116 / 106 |
| Mixed, 160 ms RTT | 48612 / 42356 | 19.755280 / 19.764061 | 101.4140 / 130.3118 | 256.0893 / 271.5093 | 197 / 202 |

The impaired profiles retain +/-20 ms one-way jitter, 1% random loss and seeded duplication. Each peer has **12 local command samples** in these cases; p95 is therefore the maximum under the documented nearest-rank calculation. Independently recomputed 80/160 ms p95 from raw generated/executed timestamps, confirmed source-zero samples remain present, and found no generated input before recorded readiness. At 80 ms, the first inputs were actually created at 130.9817/147.0579 ms, after readiness at 130.9485/147.0495 ms. No filtering or timestamp subtraction creates the passing result.

The strict `at_least_20hz` field is false for every full-length profile. The 80 ms minimum intervals are **40.4385/18.5597 ms**, exposing the actual phase correction; the 160 ms minimums are 46.5961/46.4628 ms. This review does not round those rates into a passed sustained-pacing gate or describe the loop as never producing short intervals.

All twelve additional cases pass their stated fault contracts:

- Terminal loss drops three Finish and three FinishAck packets per direction (12 total) and completes 100 ticks.
- Withheld frame 25 drops 13 transmissions; observed executed states during the hold are only 24 and 25. Both peers recover and complete.
- Lost frame ACK drops three tick-25 ACKs; the relay independently observes execution before release of the held ACK, corroborating the peer counter.
- Withheld startup ACK drops 14 initial ACK transmissions for 0.5 seconds. Player 0 produces neither active command frames nor executed checksums during the hold and becomes ready at **550.5017 ms**. Its source-zero command is genuinely created at 550.5249 ms. Player 1 becomes locally ready earlier and its first command waits **536.7807 ms**; the fixture retains this honest asymmetric-start result rather than claiming simultaneous readiness.
- Withheld checksum 25 drops 27 transmissions; observed states reach 40, and both peers reach the declared verification-lag bound of 16. They stall and recover. Lost checksum ACK separately drops three copies and recovers.
- Content, protocol and input-delay incompatibility reject before any advancement.
- Altered checksum 25 produces applied prefixes 25/27 and identifies tick 25. Altered final checksum fails both 100-tick peers: the detecting log reports tick 100, local hash 5451479770336477124 and peer hash 5451479770336477125; its counterpart times out waiting for all checksums/terminal confirmation. A final checksum cannot be bypassed by Finish.
- Disconnect produces prefixes 27/25; shared executed prefixes agree and each actual prefix replays independently.

Next: evaluate a larger scheduled input delay at 160 ms RTT against a matching-delay deterministic baseline, preserving explicit latency and pacing results. Then integrate a nonblocking session into two packaged clients and measure actual human input, feedback and complete networked play. Existing pathfinding, economy, content, performance and shipping requirements remain unchanged.

## Source observations and review feedback

- Protocol 2 removes the immediately preceding state hash from future input frames. The standalone lockstep still bounds decoding and future input, rejects ownership/conflicting frames, waits for both players and validates on a candidate state before committing. It uses no presentation clocks or Godot dependencies.
- The transport generates commands from deterministic source state, retags them by configured input delay, and preserves empty warmup frames. Receipt ACKs no longer gate execution. Executed hashes and their acknowledgments are separately tick-tagged; a future hash is not acknowledged until the local state is executed and equal. Contiguous bidirectional checksum confirmation bounds unverified execution to 16 ticks. Successful termination requires all hashes and terminal confirmation.
- Scheduled future inputs are excluded from failed-match recordings. Diagnostic prefix lengths can legitimately differ because input and checksum pipelines permit bounded progress after a fault; shared-prefix comparison and individual replay are required.
- Reviewer flagged that the old withheld-frame test treated outgoing future inputs as evidence of execution. Primary changed the assertion to observe executed-state checksum ticks. The checksum-hold case exercises the verification-lag bound rather than assuming every received future packet is advancement.
- Reviewer requested raw timing consistency checks, nonempty command samples, explicit timing-budget booleans distinct from protocol correctness, and evidence of advancing without a frame receipt ACK. These are present in the final verifier and exercised evidence.
- Initial pacing reset each deadline to actual execution plus 50 ms. The first probe demonstrated the resulting timer defect: roughly 62.5 ms intervals and 16 Hz even on clean loopback. The repair requests 1 ms Windows timer resolution using balanced RAII and retains an absolute 20 Hz phase, resetting after actual missing-data stalls or whole missed slots. This preserves ordinary phase correction rather than promising every interval is at least 50 ms. No tolerance or budget relaxation is approved by this review.

## Preserved intermediate failures and repairs

`artifacts/pipeline-probe-2026-09-06/summary.json`, individual peer reports and its console log record all 14 functional cases passing at 50 ticks. The 80 ms RTT command p95 was **188.5588/187.5334 ms**, and clean pacing roughly **16 Hz**. These are preserved evidence of the defect, not final acceptance. During the held frame, observed executed states were 24 and 25 only. With checksum 25 withheld, observed states reached 40 and both peers' verification lag reached 16. Desync prefixes were 25/27 ticks and disconnect prefixes 27/25, with replay/shared-prefix checks.

The corrected integrated log `artifacts/pipeline-build-fixed-2026-09-06.log` records **2/2 CTest executables passing in Debug and Release**, existing replay/alias checks and ten 2,000-tick replay comparisons. The new delayed-AI fixture initially used a different default seed for its reference than the lockstep simulation; explicitly matching seed 42 fixes that fixture. The earlier failed build log remains historical and is not substituted for the corrected verification.

`artifacts/pipeline-probe-fixed-2026-09-06/summary.json` and console log record all 14 cases passing after the timer repair. Clean rates are approximately **19.99 Hz**, with observed minimum tick intervals about **48 ms**. At 80 ms RTT, measured command p95 is **152.0037/135.0758 ms**, still a strict target miss for one peer; 160 ms RTT rates are **19.7608/19.6189 Hz**. The 50-tick probes have only three generated-command samples per peer, so their p95 is their maximum. Initial sampling before the empty-frame exchange finishes includes startup wait honestly; any future readiness barrier must precede accepting input, not remove or retimestamp measured samples.

The first isolated 1,000-tick suite, `artifacts/pipeline-verified-2026-09-06`, passed 15 functional cases, including an altered final executed checksum. Its 80 ms RTT p95 remained **163.7463/131.9515 ms**, with 12 samples per peer. This is a preserved failure of that version's command-response target, despite near-20 Hz pacing. The four run-metadata executable fingerprints were independently matched to the then-current binaries before the readiness edit.

The readiness repair has been inspected in source. It sends and retransmits the initial empty frames normally, then requires all initial remote frames and receipt acknowledgments of all local initial frames before setting the first active tick deadline or sampling source-zero commands. It rejects nonempty initial frames. Startup duration and readiness time are recorded separately. Input creation timestamps and every command sample remain intact; this changes when the session begins accepting input rather than subtracting an observed delay. It does not guarantee simultaneous peer readiness under arbitrary loss. The final fresh suite above exercises this revised behavior.

## Evidence limits to preserve

Command timing begins at generated canonical input, after deterministic AI sampling, and ends after local authoritative execution. The present bot emits tick-aligned commands at source ticks divisible by 20 and stops after victory. This excludes human input polling phase, presentation feedback and nonblocking client integration. Sparse local generated-command samples are useful transport evidence but cannot satisfy the full human input-response gate.

The mismatch field identifies the first mismatch observed by the transport. A fixture with only one altered checksum can identify its exact tick; arbitrary reordered multiple divergences require trace analysis before claiming the globally earliest divergence.

No independent shipping approval, production networking security, physical-network behavior, 30-minute complete match, client multiplayer, reference hardware performance or completed RTS scope is established by this increment. Keep SPEC budgets and the user's any-angle pathfinding requirement unchanged.

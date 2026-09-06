# Independent network review - 2026-09-06

Reviewer: independent agent `network_review`. Primary owns implementation, executions and Git integration; reviewer owns only this report. Read AGENTS.md, STOP state and project handoff before review. Scope: closed tick frames, executed-state hashing, loopback UDP peer, VFR2 replay loading, verification scripts and actual current-run artifacts. No visual, gameplay, shipping or AAA approval.

## Verdict

Accept this bounded headless transport foundation checkpoint: separate Windows processes exchange canonical turns under measured impairments, detect tested faults, and produce matching executed-state replay traces. The real-time multiplayer gate remains open. Stop-and-wait does not sustain the specified 20 Hz under the tested latency, and Godot network play and complete matches are absent.

## Findings and disposition

1. **Replay output aliases could overwrite the replay input.** Source inspection found `headless.cpp` compared canonical path strings without existing-file identity or Windows case normalization. Primary reproduced acceptance of a hardlinked trace/replay pair (`artifacts/output-aliases-before.log`). Current source checks equivalent files and case-normalized paths before opening streams. Independently inspected the regression: disposable exact-path, hardlink and case aliases for replay and recording preserve original bytes; a not-yet-created case alias creates no output. `artifacts/replay-alias-verification-2026-09-06.log` records all seven cases passing in each MSVC configuration, alongside 2/2 CTest passes per configuration and the existing replay checks.
2. **Windows UDP reset notifications aborted the relay.** The failed fresh probe log ends at relay `recvfrom` with WinError 10054. Delayed sends to an exited/unbound UDP endpoint can generate this notification. Current relay counts `ConnectionResetError` and continues; other socket exceptions remain fatal, and peer progress timeouts still enforce disconnect detection. Both the 50-tick probe and full suite exercise this fix. Full-suite reset notifications total **187**, detailed below; disconnect still rejects at tick 25.
3. **Reused evidence paths could retain stale success reports.** During the failed rerun, old `result.json` and peer reports coexisted with new empty logs/traces. Current harness exclusively creates a new output directory, defaults to a timestamp/UUID directory, and saves run times and hashes of both peer and headless binaries. Independently matched all four current binaries to the final summary fingerprints. The failed old directories are not acceptance evidence.

An early reviewer message alleged missing buffered-frame desync details based on truncated output. Exact source inspection disproved it: the advancement error already includes tick, local state, local frame and peer hashes. That claim was retracted and is not a finding.

## Executed evidence inspected

Final evidence: `artifacts/network-verified-2026-09-06/summary.json`, run metadata, individual result/peer reports and diagnostic logs. Run: **2026-09-06 17:29:21.466012 to 17:33:51.940100 UTC** (10:29 to 10:33 America/Los_Angeles). Supporting log: `artifacts/network-verified-2026-09-06.log`. The fresh smaller probe is separate at `artifacts/network-probe-fixed-2026-09-06`.

Slash-separated counts below are direction/player 0 and 1. Elapsed values include process startup, terminal linger and replay verification; they are not input-latency measurements.

| Case | Separate PIDs | Applied ticks each | Seconds | Random drops | Forced drops | Reset notifications |
|---|---|---:|---:|---|---|---|
| Debug clean | 41100 / 26388 | 1000 | 49.08 | 0 / 0 | 0 / 0 | 0 / 1 |
| Release clean | 35300 / 40344 | 1000 | 49.03 | 0 / 0 | 0 / 0 | 0 / 1 |
| Debug/Release, 80 ms RTT | 11496 / 40860 | 1000 | 143.52 | 82 / 73 | 0 / 0 | 2 / 0 |
| Release/Debug, 160 ms RTT | 48144 / 29868 | 1000 | 220.83 | 106 / 101 | 0 / 0 | 5 / 0 |
| Terminal packet loss | 35308 / 15144 | 100 | 16.53 | 0 / 0 | 6 / 6 | 0 / 1 |
| Withheld tick frame | 39624 / 41572 | 100 | 7.42 | 0 / 0 | 0 / 11 | 0 / 1 |
| Lost tick ACK | 29552 / 6176 | 100 | 7.04 | 0 / 0 | 0 / 3 | 0 / 1 |
| Reject content | 26020 / 48896 | 0 | 2.08 | 0 / 0 | 0 / 0 | 43 / 1 |
| Reject protocol | 32696 / 33184 | 0 | 2.06 | 0 / 0 | 0 / 0 | 43 / 1 |
| Detect desync | 35360 / 31336 | 25 | 3.34 | 0 / 0 | 0 / 0 | 43 / 1 |
| Detect disconnect | 29768 / 47600 | 25 | 3.31 | 0 / 0 | 0 / 0 | 0 / 43 |

The 80/160 ms profiles configure +/-20 ms one-way jitter and 1% loss. Observed one-way delay medians were approximately 46.58/46.58 ms and 92.31/92.23 ms; scheduler overhead is present. Terminal fixture drops the first three Finish and FinishAck packets in both directions. Withheld-frame fixture prevents forwarding player 1 tick 25 for 0.5 seconds and rejects evidence of player 0 advancing past it during that interval. Lost-ACK fixture drops three ACKs for tick 25 and then verifies recovery.

All four 1000-tick cases end at executed-state hash **3888858813899053580**. Independently hashed the **16** peer/replayed trace files across those cases and all **10** repeated replay traces: every file SHA256 is `3cda78f81a85a1a9f45175d5566a5090db3216c1b7394b70ecb892fcbc68823e`. Harness also compares each peer's VFR2 recording replayed by the opposite build configuration, including successful 100-tick fault-recovery cases and both 25-tick failed-match prefixes. Final summary records rejection of incompatible-content, truncated-content and trailing-byte VFR2 fixtures.

Inspected desync log identifies tick 25, local hash `10526839968865815419` and peer hash `10526839968865815418`. Disconnect survivor reports timeout waiting for tick frames/ACK at tick 25. Content and protocol diagnostics identify incompatibility before advancement; the other endpoint times out in handshake. No claim that a zero-tick rejected join is a playable replay.

## Remaining gates and exact next step

`state_hash()` covers all current executed authoritative fields and sequence counters while excluding pending receipts. Closed-frame advancement validates both players against a copy before committing. Source uses fixed integer state and canonical explicit serialization; transport clocks remain outside authoritative simulation. Current tests cover bounded decoding, ownership, duplicate/conflicting frames, reordered future frames, missing-frame stalls and atomic rejection. Actual transport coverage is narrower than arbitrary malformed envelopes, adversarial floods, persistent partitions or reconnect behavior; those remain unproved.

The largest networking gap is **real-time throughput**. The measured 1000-tick 80/160 ms cases take about 143.5/220.8 seconds, far beyond the 50 seconds implied by 20 Hz. `stall_count` currently counts every advanced tick and `stall_ms` accumulates whole tick cycles, not exclusively excess missing-input delay. Neither is authoritative-response latency evidence.

Next: introduce scheduled input delay and pipelined canonical turns, with tick-tagged lagged state checksums decoupled from command frames' immediately previous state hash. Future commands cannot know the future preceding state at send time. Preserve deterministic missing-turn stalls and bounded buffering, then measure sustained 20 Hz and response budgets under the same impairments before claiming client networking readiness.

No 30-minute soak, Internet protocol validation, human networked Godot session, full economic 1v1/1vAI, reference-hardware performance, production crowd routing, balance, content or shipping review is established here. No completion marker is justified.

# Independent movement review - 2026-09-07

Verdict: the corrected bounded any-angle increment, static route fixtures and preserved small-skirmish network/runtime paths have sufficient evidence for a development checkpoint. This is not acceptance of the complete movement milestone, representative performance, production presentation or shipping. The 500-unit diagnostic already exceeds the simulation budgets on the small current map.

The reviewer owned this report only, performed no Git operations or builds, inspected actual source and retained evidence, and independently checked raw recording/timing/trace consistency. Applicable project instructions and handoff documents were read.

## Defects found and corrected

1. **Feasible tangent routes could stop forever.** The original normalized component truncation changed the ray near a tangent corner. An independent exact-rational calculation found the current-map segment from (3968,3920) to (4570,1835) clear, but after 51 advances the integrator reached (4407,2390), proposed (4416,2360), entered the expanded upper ridge and rejected the same step indefinitely. Both endpoints are legal. The primary replaced one-sided truncation with up to nine nearby lattice candidates, deterministic projection-error ordering, maximum-speed/strict-progress constraints and complete static/swept-unit checks. The supplied two-command regression is present and passes in Debug and Release.
2. **Attack-move could lose its cached route after combat.** The temporary goal==here branch cleared the path without invalidating route_goal. The primary now invalidates the cache key. The reviewer supplied a natural command-only regression: approach the enemy's original position, briefly move the enemy toward the attacker so the attacker survives combat, then verify that the survivor resumes and reaches its unchanged destination. The assertions and passing Debug/Release test runs were inspected.
3. **The original speed-loss bound was unsupported.** Remaining delta (17,34) produced step (13,27), losing 2.033352 coordinate units from nominal speed 32, exceeding the claimed sqrt(2) bound. The claim was removed. A limited independent enumeration of positive remaining deltas 1..256 for the corrected solver found worst sampled open-step deficit 1.389544 at delta (20,25), step (19,24). This is a diagnostic, not a global bound or consistent-speed acceptance. Heading variation and short final approaches still need systematic measurement.

## Static geometry and determinism

Read sim/navigation.{hpp,cpp}, sim/voidfront_sim.{hpp,cpp}, their tests/probe, sim/CMakeLists.txt, replay compatibility and the navigation contract. Geometry uses bounded integer rational comparisons. Axis-aligned square clearance and permitted tangency are explicit. Stable visibility vertices, integer edge costs and deterministic Dijkstra ties avoid runtime clocks and floating-point authority. Route waypoints and the cache goal participate in state hashes. Navigation source participates in the content fingerprint. Simulation version 2 intentionally rejects prior version 1 replay behavior while VFC command wire bytes remain version 1.

The independent reference uses a Cartesian candidate superset, floating Euclidean Dijkstra only offline, and exact edge-event midpoint classification cross-checked against rational slabs and analytic routes. Its shared visibility-graph mathematical model is explicitly disclosed. It does not reuse C++ collision code or integer edge costs.

Reviewed artifacts/navigation-verified-2026-09-07/summary.json and raw Debug-00.jsonl routes. All 51 fixtures pass ten identical route runs per configuration, within the predeclared 1% plus two-coordinate-unit tolerance. Observed worst and mean excess are zero. Current probe fingerprints match the recorded identities. The staircase fixture approximates an angled wall with rectangles; it does not fulfill true angled-wall support.

artifacts/any-angle-build-fixed-2026-09-07.log records integrated MSVC Debug and Release builds, 4/4 CTest each, malformed replay/output-alias regression checks, 2,000-tick cross-build equality and ten repeated replays. artifacts/any-angle-resume-test-2026-09-07.log records both configuration tests after the combat-resume regression was added. Source assertions were read directly. Compilation is not used as gameplay proof.

## Separate-process network preservation

artifacts/any-angle-network-2026-09-07/summary.json passes 22 actual separate-process UDP cases. Inspected each case's PIDs, status, dropped packets, confirmed prefixes and failure diagnostics. Independently parsed all 44 raw VFR2 recordings and peer reports: 436 local applied commands exactly match their timing identities, 38 nonzero traces match retained opposite-configuration replays, all pair common prefixes agree, and ten repeated trace files per delay match the recorded SHA256. All four current peer/headless executable fingerprints match the suite.

Eight positive profiles confirm 1,000 ticks. Fault fixtures have explicitly shorter prefixes, typically 100 ticks; incompatibilities stop at zero and injected disconnect/desync fixtures retain 25..29 ticks. Passing 22 cases does not mean 22 full 1,000-tick matches.

At 80 ms RTT, 231 actual random drops occurred, command p95 is 101.4676/100.9927 ms and pacing is 19.973737/19.981194 Hz. At 160 ms with default two-tick delay, pacing is 19.067905/19.069412 Hz and command p95 is 157.0133/166.737 ms. Four ticks give 19.999518/19.999563 Hz with zero observed stalls, at approximately 201 ms command p95. Defaults did not change; no between-run optimization claim is justified. These are sparse generated AI commands over loopback, not physical-network, human-input or 30-minute complete-match evidence.

## Packaged movement and recording inspection

Read client/main.gd's movement InputEvent fixture and tools/verify_movement_capture.py. Reviewed artifacts/packaged-movement-2026-09-07.json and its hash-bound packaged-movement-verified-2026-09-07.json: 400 consecutive authoritative positions, 184 arbitrary-heading steps, nine stationary Stop ticks, live retargeting and three exact arrivals. Integrated travel excess over the independent static reference is 0.018590% direct, 0.013368% ridge detour and 0.013346% retarget. Every reported step is at most 32 coordinate units; the verifier checks complete swept static clearance and retains actual production-converted command coordinates. Independent adversarial verifier evidence rejects illegal geometry, fake detours and drift on the first Stop response tick.

Actually inspected packaged-movement-2026-09-07-ridge.png and -detour.png at 1920x1080. The selected walker appears at the ridge end and subsequently east of it, with oblique orientation. The position ledger supplies collision/route evidence; screenshots alone do not.

Actually inspected artifacts/movement-contact-2026-09-07.png: twelve cropped movie frames sampled at 1 fps from the first 12 seconds. They show oblique departure, approach below the upper ridge, traversal around its end, east-side arrival and subsequent retarget progression. No obvious teleport or orthogonal staircase is visible in those samples. This is sampled recording inspection, not full-frame video playback, temporal-smoothness certification or detailed deformation review. Metadata records 1600x900, 60 fps, 1,201 frames and 20.016667 seconds. The separate movie ledger hash matches its verification artifact and repeats the normal route lengths, 184 oblique steps and nine Stop ticks. Recording timings are excluded from performance claims.

Normal movement capture frame p95/p99 is 16.997/17.369 ms, missing the 16.67 ms p99 target. Bridge step p95/p99 is 0.027/0.031 ms; sampled resident peak is 238,157,824 bytes. These are one-moving-walker/twelve-present-unit development-host observations. The retained logs show the known root-certificate-store diagnostic.

artifacts/packaged-any-angle-offline.json and its host report pass click/drag/move/stop/combat/restart and clip-request checks. Tick 400 hash is f2870e230c766422 and winner is -1: combat activity, not a completed skirmish at 400 ticks. Frame p99 is 17.443 ms; bridge p95/p99 is 0.040/0.055 ms; sampled resident peak is 235,126,784 bytes. This still does not meet the frame target.

## Final packaged network regression

Reviewed artifacts/any-angle-client-network-2026-09-07/summary.json: six cases pass, with four positive pairs confirmed through 240 ticks, mismatch rejected at zero, and disconnect retaining matching 52-tick prefixes (49 confirmed). Read raw reports and checked retained Debug/Release replay equality and common prefixes for every nonzero recording. All current package executable, PCK, bridge DLL and replay executable fingerprints match the saved suite.

At 80 ms RTT with 54 actual drops, three synthetic inputs per peer give event-to-observed-execution p95 133.781/133.861 ms, first-render p95 166.824/166.708 ms and marker feedback p95 16.636/16.630 ms. These p95 values are maxima of three samples, not robust human-response estimates. The 160 ms profile uses four ticks and observed 74 drops.

The additional artifacts/any-angle-client-combat-2026-09-07/summary.json confirms 1,000 ticks for both peers, matching winner 1, repeated replay verification and bound current package fingerprints. Independently checked both raw recordings against retained Debug/Release traces. Actually inspected clean/peer0.png and peer1.png: the first displays SIGNAL LOST, the second SECTOR SECURED, and both display COMPLETE at tick 1000 with confirmed 1000. The existing defeat wording is ambiguous with connection loss. This is a scripted combat-skirmish outcome, not the required economic RTS match.

## Performance failure and remaining acceptance gaps

Read the isolated current-map headless logs after other runtime checks ended:

- artifacts/any-angle-200unit-2026-09-07.log: 200 units, 2,000 active ticks, p95/p99 2.465/2.704 ms, winner -1.
- artifacts/any-angle-500unit-2026-09-07.log: 500 units, 2,000 active ticks, p95/p99 15.740/16.815 ms, winner -1.

The 500-unit case exceeds the 4/8 ms simulation targets even on the current 32x24 map. The 200-unit result is a small-map diagnostic and cannot establish the required 128x128 representative battle budget. Neither run concludes in 2,000 ticks; that alone does not prove permanent crowd lock, but it also supplies no full-match success evidence.

Dynamic collision uses conservative swept axis-aligned exclusion and stable sequential priority. Its bounded local candidate search is not a global dynamic-unit detour solver. Stationary blockers, opposing streams and crowded chokes can remain locked. Naive per-unit scans also impose quadratic collision work. Next priority should be reproducible crowd/blocker failure fixtures plus spatial broadphase and bounded collision/replanning work, with unchanged performance targets, before adding more expensive dynamic routing.

True angled terrain, dynamic construction, shared/repeated group orders, consistent heading speed, 128x128 representative profiling, broad input cadence and physical-network/30-minute complete matches remain open. Overbright walker surfaces/rings, stacked idle formations, blockout terrain and the Stop marker at map origin remain visible presentation debt. Full economy/AI, finished content, balance, human playability, detailed animation quality and independent shipping review are not satisfied. No COMPLETE.md or shipping approval is justified.

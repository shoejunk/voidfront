# Continuing roadmap

## 2026-09-26 economy checkpoint: production is next

The first packaged worker/salvage/construction increment is verified; see the
latest PROGRESS.md, TESTING.md and economy review. Continue the small-match
sequence below at step2: resource-funded unit production, queues and population
limits, with visible purchase/queue/spawn feedback and canonical replay tests.
Then deliver economic AI and anchor victory. The earlier instruction to deliver
the first economy increment has been fulfilled for its bounded scope. Existing
large-crowd/performance/production-quality failures remain required final gates.
This checkpoint preserves the user's gameplay-first sequencing change below.

1. **Foundation / playable skirmish (first increment verified):** independent deterministic sim, canonical commands/persistent replays/tests, pinned extension, camera/selection/move/combat, original animated Blender unit and packaged runtime evidence exist. Production art, crowd routing and manual responsiveness remain open.
2. **Real network lockstep (packaged loopback client path verified):** protocol 2 scheduled frames and independent executed checksums now drive two rendered Godot packages through the reusable Session. Players select their own army; readiness/stalls/errors/final confirmation and explicit input delay are visible. The six-case client suite covers clean/80/160 ms RTT, withheld input, mismatched delay and disconnect, with exact applied-prefix replay and original event-to-execution/render timing. Offline mode remains intact. This is loopback combat-skirmish evidence, not LAN/Internet or full RTS multiplayer acceptance. Only three synthetic commands per peer/profile were measured; strict pacing and frame targets remain unmet. Follow-up network gates: broad input phase/cadence and queued-during-stall/tail tests, packaged post-advance desync recording, physical endpoints, coordinated match setup/restart and complete-match/30-minute soak. Preserve all CLI/API/client regressions and tool pins.
3. **Complete small match (next development milestone):** gather/build/produce/tech/fog/victory/restart; command-driven economy AI; queued orders and enemy context targeting; finish the movement requirements below before production content expansion. **Current increment:** static visibility routes, arbitrary fixed-point headings, exact destinations, spatial broadphase, bounded local swept detours and control groups 0-9 are implemented. Seven repeated crowd fixtures, packaged InputEvent swap/chain/Stop/resume and control-group tests pass. An actual 128x128 Sim harness now covers eight 200/500-unit traffic/combat scenarios with 96 executions and 11,202,800 independently audited unit rows; all Release p95/p99 observations meet 4/8 ms on this host, but every noncombat crossing/repeated/Stop-resume case has zero arrivals. **Exact next step:** coordinated yielding or preventative avoidance for the retained dense opposing-stream fixture, following crowd-failure-2026-09-12.md and scale-contract.md. Preserve all replay/network, static-route and packaged tests. General crowd progress, dynamic construction, per-command response, representative production battles, reference performance and human responsiveness remain unaccepted.
4. **Content and production systems:** two asymmetric factions, three maps, complete art/animation pipeline, UI/audio/VFX, tutorials, accessibility, robust match UX.
5. **Quality iterations:** independent visual/RTS/determinism/performance critics, reference comparisons, large battles, balance, responsiveness and network soak on documented hardware.
6. **Shipping gate:** full regression, actual complete games, reproducible package, adversarial independent review against every SPEC requirement. COMPLETE.md only with passing evidence, then pause automation.

Reorder work when measured defects justify it, preserving architecture and all quality gates. No deadline-based relaxation.

## 2026-09-13 checkpoint refinement

The existing Scale128 scenario is now available in the actual offline package
with configurable 1..250 walkers per side, map-aware camera/orders/minimap and
restart preservation. Packaged 200/500-unit InputEvents and full recorded tick
hashes replay across builds/repeats; see TESTING.md. Large-map networking remains
separate. This is diagnostic access to the same simulation, not complete matches.

Early opposing-stream bias and bounded common lateral translations both failed
the unchanged 500-unit crossing case and were removed. The critic proved legal
lateral moves can merely oscillate and press neighbors against the ridge ends.
Next movement work: purposeful backward decompression and a retained passing side,
with explicit Stop/Hold/firing eligibility and the same full safety/progress gate.
Do not repeat undirected lateral fallback or merely increase component limits.

Large-map captures expose poor crowded readability and failed frame intervals.
The same-run twelve-unit capture also slows under hidden-window conditions, so
first profile ordinary foreground play with controlled presentation conditions;
separate capture/fixture overhead, snapshot conversion, skeletons, draw calls and
HUD cost before choosing an optimization. Preserve the original budgets and
report actual product responsiveness separately. Production/content expansion
still waits on the movement and complete-small-match requirements above.

## Required movement and pathfinding milestone

2026-09-20 evidence refinement: ordinary-play stage profiling is now available
in `tools/profile.ps1`, with raw focus/window/stage samples, payload fingerprints
and an independent verifier. A measured HUD optimization batches static minimap
cells and avoids hidden-bar projections; final 500-unit HUD p95 is 4.501 ms.
Whole-frame p99 remains 20.400/63.897 ms for 200/500 units on the development
host. Focus flags pass in hidden launches, but unoccluded human foreground play,
same-state engine animation/skeleton/render attribution and representative frame
acceptance remain open. Preserve the independent evidence in TESTING.md. This
does not change the failed crowd-progress gate or production prerequisites.

The bounded current-map implementation now has arbitrary authoritative headings and static clearance routes. Finished movement must additionally satisfy the dynamic crowd, route quality and performance gates below, with direct travel toward unobstructed destinations and efficient routes around obstacles. Neither four/eight-direction movement nor presentation-only smoothing satisfies this requirement.

The algorithm and navigation representation remain open: constrained Delaunay triangulation is not required. Choose the strategy using measured route quality, runtime cost, memory use and deterministic behavior. All authoritative routing, collision and avoidance remain in the standalone integer/fixed-point simulation, with stable ordering and identical Debug/Release replay results.

Acceptance requires:

- **Optimal or near-optimal paths:** measure route length/cost against an independently validated shortest feasible reference using the same unit clearance and terrain costs. Include open ground, angled walls, concave obstacles, narrow passages and long detours. Define and record a quantitative near-optimality tolerance before accepting the implementation; report worst-case as well as aggregate error. Visual smoothness alone is insufficient.
- **Fast, efficient execution:** profile initial group orders, shared destinations, repeated orders and replanning after blocker changes on the existing representative 200-unit and 500-unit scenarios. Record pathfinding time, simulation p95/p99, memory and command-response latency; meet the existing SPEC budgets without weakening them. Bound and deterministically schedule work to avoid frame/tick spikes.
- **Robust movement in actual play:** validate unit clearance, consistent speed across headings, responsive stop/retargeting, crowded chokes, opposing streams, moving blockers, unreachable goals and dynamic construction without clipping, permanent crowd locks or needless zigzags. Measure dynamic crowd behavior separately from static shortest-path quality so necessary collision avoidance is distinguished from poor global routing.
- **Evidence before acceptance:** retain deterministic regression fixtures, route-quality comparisons and performance captures; inspect packaged gameplay recordings and obtain independent pathfinding/playability review. Any-angle movement and efficient near-optimal routing are required functionality, not optional late polish.

Controlled diagnostic contract (2026-09-25): fixed seed/map/tick 40, fixed
camera and clip phase 0.25 seconds. Pose refresh repeatedly submits that pose;
pose frozen initializes the identical pose and disables automatic updates.
This measures manual resubmission plus downstream effects, not ordinary
animation, isolated skeleton/GPU time, or gameplay. Expanded static geometry
and exact bone-pose hashes must match for comparisons. A rebuilt native DLL
caused the first implementation comparison to reject; fresh matched-native
captures use a separately retained baseline PCK with the final EXE/DLL.

## 2026-09-26 movement experiment rejection

Six portal-lane variants failed the unchanged 500-unit crossing (25/0/12/49/84/72
exact arrivals). All authoritative changes were removed. The reviewed checkpoint
retains twelve small regression trajectories, exact relative-sweep checks and
final routing diagnostics. Read `review-crowd-recovery-2026-09-26.md` and the
rejected experiment archive before resuming. Fixed gate offsets and passing-half
restrictions did not coordinate same-stream merging, egress or goal fan-out.
Next implement bounded follower dependencies and cooperative entry/exit ordering,
with a geometrically valid progression objective and full collision checks;
retain exact goals and immutable Stop/Hold/firing eligibility. Passing the small
tests is preservation evidence only. Dense arrival, full matches, production
content, responsiveness, reference performance and shipping remain open.

## 2026-09-26 cooperative follower checkpoint

Bounded transactional following fixes the demonstrated tangent-convoy defect:
six units now advance together without artificial stops or lateral diversions.
Canonical goals, exact full-tick relative sweeps, immutable Stop/Hold/firing
participants, rollback and deterministic work caps are preserved. This does not
solve opposing-stream progression, and the full arrival contract remains intact.

The optional six-unit reversed-goal probe isolates the next smaller defect.
Five units arrive; unit 2 stops at (2448,2928), targeting (2304,2944), between
already-arrived neighbors at z=2816 and z=3072. Independent frozen-scene geometry
permits vertical alignment to z=2944 at x=2448, then horizontal travel into the
exact slot. Next implement bounded, collision-checked goal-axis or multi-blocker
clearance-boundary candidates. Prove this canonical fixture before combining it
with cooperative entry/exit ordering on the unchanged 500-unit opposing stream.
Do not displace stopped arrivals, change goals, substitute static feasibility for
executed progress, or claim transaction-cap/cycle exhaustion coverage that has
not been exercised. Preserve replay/network/package gates and original budgets.
# Continuing roadmap

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

The bounded current-map implementation now has arbitrary authoritative headings and static clearance routes. Finished movement must additionally satisfy the dynamic crowd, route quality and performance gates below, with direct travel toward unobstructed destinations and efficient routes around obstacles. Neither four/eight-direction movement nor presentation-only smoothing satisfies this requirement.

The algorithm and navigation representation remain open: constrained Delaunay triangulation is not required. Choose the strategy using measured route quality, runtime cost, memory use and deterministic behavior. All authoritative routing, collision and avoidance remain in the standalone integer/fixed-point simulation, with stable ordering and identical Debug/Release replay results.

Acceptance requires:

- **Optimal or near-optimal paths:** measure route length/cost against an independently validated shortest feasible reference using the same unit clearance and terrain costs. Include open ground, angled walls, concave obstacles, narrow passages and long detours. Define and record a quantitative near-optimality tolerance before accepting the implementation; report worst-case as well as aggregate error. Visual smoothness alone is insufficient.
- **Fast, efficient execution:** profile initial group orders, shared destinations, repeated orders and replanning after blocker changes on the existing representative 200-unit and 500-unit scenarios. Record pathfinding time, simulation p95/p99, memory and command-response latency; meet the existing SPEC budgets without weakening them. Bound and deterministically schedule work to avoid frame/tick spikes.
- **Robust movement in actual play:** validate unit clearance, consistent speed across headings, responsive stop/retargeting, crowded chokes, opposing streams, moving blockers, unreachable goals and dynamic construction without clipping, permanent crowd locks or needless zigzags. Measure dynamic crowd behavior separately from static shortest-path quality so necessary collision avoidance is distinguished from poor global routing.
- **Evidence before acceptance:** retain deterministic regression fixtures, route-quality comparisons and performance captures; inspect packaged gameplay recordings and obtain independent pathfinding/playability review. Any-angle movement and efficient near-optimal routing are required functionality, not optional late polish.

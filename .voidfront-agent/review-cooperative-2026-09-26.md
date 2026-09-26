# Independent cooperative movement review - 2026-09-26

Scope: independent adversarial source/design and root-produced evidence review.
This reviewer owns only this report; no Git operations, builds, simulator runs,
or implementation edits. STOP was absent and the primary agent owns the active
run marker for task 01a0de62-2496-7fa3-8121-488995b713df. Read AGENTS, SPEC,
PROGRESS, TESTING, roadmap, scale contract and the preceding crowd-recovery
review. This is not a shipping review.

## Baseline and design requirements

The preserved baseline has no cooperative follower transaction. Actual movement
checks each other unit's whole tick-start/current swept bounding rectangle.
That conservative check can reject legal simultaneous following into space the
leader vacates, even when the two interpolated centers preserve full clearance.
Local detours do not reserve future space. Their blocked_ticks counter resets
after failed searches and cannot establish elapsed no-progress time.

The unchanged dense gate remains 500 living units, exact assigned destinations,
the retained 128x128 terrain, seed 42, opposed canonical Move commands and 4,000
ticks. Previous experimental passage through the mouth did not solve downstream
ordering or final fan-out. Low cost during a stationary tail does not establish
movement acceptance. The baseline and previous probe counts are prior dated
evidence; fresh source/artifacts must support this run's claims.

The implementation proposal is a Move-only transaction after ordinary movement:
resolve bounded follower dependencies, try their own forward steps before
retained lateral yielding, preserve immutable participants and validate complete
relative sweeps before committing. Advice sent directly to the implementation
agent:

- All dependency recursion needs stable ordering, cycle detection, bounded work
  and complete rollback. A failed child must not leave a parent's assumed move
  or partial route/recovery state committed.
- Stop, Hold, exact-arrived units and units already moved this tick stay
  immutable. Move-only participation avoids changing firing eligibility, which
  must follow the actual earlier combat decision if eligibility later expands.
- Validate simultaneous relative segments for unequal participant displacement;
  endpoints or an equal-translation argument cannot prove general safety.
  Nonparticipants retain full tick-start/current sweeps. Speed applies from the
  tick start, not as an additional allowance after ordinary movement.
- A retained yield objective needs geometric clearance and a purposeful release
  condition. Repeated lateral motion or side switching is not leader progress.
  Cycle/component-limit rejection and geometric infeasibility are different
  diagnoses; a 16-participant cap does not establish dense-crowd liveness.
- Canonical goals stay fixed. Explicit orders clear persistent recovery state;
  every persistent field participates in authoritative hashes. Refresh terrain
  routes after recovery leaves an old visibility segment.
- A successful subset is unsafe when its clearance proof depended on rejected
  members moving. Commit dependency sets atomically or revalidate all rejected
  participants as stationary.

## Fresh baseline and first source inspection

Read `artifacts/cooperative-baseline-2026-09-26/crossing.json` directly:
500 living units, zero exact arrivals, last motion tick 530, maximum pending
idle 3,709 and peak resident memory 5,074,944 bytes. The unchanged dense failure
is reproduced in this run.

The baseline-linked new chain executable reports both tangent follower cases
failed while all twelve prior fixtures and the analytic sweep oracle pass.
Its complete retained CSV shows the actual unwanted behavior: after 16 ordered
movement ticks, forward unit 1 is at (784,2704), short of (640,2944), while the
unblocked front unit 6 reaches (640,3584). In the reverse case unit 6 diverts to
(496,2800), short of (640,2560), while unit 1 reaches (640,1920). These are six
canonically positioned walkers at 128-coordinate spacing, with a legal equal
32-coordinate translation each tick. Exact 16-tick co-motion is a deliberately
specific regression contract; it is not a universal RTS responsiveness claim.

Inspected the first route-forward transaction implementation. It defers
cooperation until all ordinary combat/movement decisions finish, accepts only
unmoved unfinished Move units, uses existing effective waypoints, and adds no
persistent state. Each recursive proposal is terrain-checked and compared with
other units using tick-start-to-proposed/current relative segments against the
open clearance square. The 192-coordinate broadphase covers 128 clearance plus
both possible 32-coordinate displacements. Failed descendants roll back to the
saved member count. A final whole-transaction revalidation checks sibling and
nonmember sweeps before committing any positions. Work is capped by participant,
start and candidate-attempt counts with stable tick-based starting identities.

No source-backed safety defect was found in that version's rollback or final
sweep checks. I flagged the older local search possibly installing a detour
after setting next_x/next_z, allowing cooperation to succeed toward the previous
waypoint but leave the newly unnecessary detour for the next tick. The agent
agreed this needs evidence before altering established detour semantics.
Global responsiveness and liveness do not follow from bounded starts alone.
Unproven retained-side yielding was deferred; that narrower scope is preferable
unless new evidence demonstrates a purposeful yielding benefit.

The final refinement snapshots a Move unit's active detour before ordinary
reconsideration and restores that snapshot only when the cooperative retry
succeeds toward the earlier waypoint. Failed transactions preserve the ordinary
result. Consumed detour fronts have already been removed before the snapshot.
No persistent state or untracked timer was introduced. Source review finds this
consistent with retaining the maneuver actually attempted and discarding only
an unnecessary replacement.

The primary identified the legacy replay compatibility boundary: default Foundry
records still emit VFR1, which carries kProtocolVersion but no content identity.
I confirmed the load/save branches in sim/headless.cpp. Bumping simulation/replay
version 4 to 5 is warranted to reject older authoritative behavior. Canonical
command format and lockstep packet format are unchanged; current network frames
also carry the rebuilt simulation source fingerprint. Old-replay rejection must
be verified rather than assuming that content hashing protects VFR1.

## First candidate evidence

`artifacts/cooperative-probe1-chains-2026-09-26.log` passes both new cases and
the original twelve. Independently checked every ordered coordinate row after
setup: all 192 walker samples across the two directions are exactly the intended
32-coordinate axial displacement for all sixteen ticks, with no sideways detour.
This is a concrete repair of the retained baseline follower defect.

Independently recomputed the complete raw timing arrays and endpoints for the
baseline and first dense candidate:

| Run | Exact arrivals | Last motion | Maximum pending idle | Step p95/p99/max ms |
| --- | ---: | ---: | ---: | --- |
| Fresh baseline | 0/500 | 530 | 3,709 | 1.6006 / 1.9193 / 2.1279 |
| First follower candidate | 0/500 | 587 | 3,711 | 2.7245 / 3.1433 / 4.1284 |

All 500 remain alive in both. Candidate peak resident memory is 5,128,192 bytes.
The additional later motion is not dense progress acceptance. Candidate cost is
materially higher despite remaining below the 4/8 ms p95/p99 numerical limits;
the full scenario matrix and final-source evidence must establish the acceptable
scope. These are exploratory single Release observations on the development host.

## Final-source regression evidence inspected so far

`artifacts/cooperative-final-build-2026-09-26.log` records pinned Godot
4.7.2 verification, integrated MSVC Debug/Release 7/7 CTest each, malformed
replay rejection and separate/cross-build/repeated 2,000-tick trace equality.
`artifacts/cooperative-legacy-rejection-2026-09-26.log` records incompatible
replay protocol for the frozen older VFR1 fixture.

`artifacts/cooperative-small-2026-09-26/summary.json` records seventeen
fixtures, 30,254 independently audited rows, ten runs per configuration and
22 deliberate evidence corruptions rejected. I checked both current crowd-test
executable SHA256 values against recorded runs and checked the first Debug and
last Release ledger files against the recorded common hash. These are the
original twelve cases plus two tangent directions, Stop/Hold blocked-convoy
prefixes and an interrupted convoy retarget with exact arrivals. The blocked
first tick rejects partial failed dependency commits; later legal detours are
allowed. Transaction-cap exhaustion is expressly not covered.

The final static-route summary records 51 fixtures with zero reported route
excess against its independent reference and ten runs per build. This preserves
static route evidence; it cannot accept dynamic crowd routes or progress.

The optional final redistribution diagnostic remains rejected, but the final
source improves its result from four arrivals to five. In
`artifacts/cooperative-final-redistribution-2026-09-26.csv`, all six canonical
goals remain fixed by the C++ assertions. At tick 760 only unit 2 remains
unfinished at (2448,2928) instead of (2304,2944), stationary since tick 332 with
empty detour. Unit 4 now reaches its assigned goal, so the earlier two-pending
snapshot is superseded. Stopped units 3 and 1 at (2304,2816)/(2304,3072) leave
an exact legal tangent line at z=2944. Vertical alignment at x=2448 followed by
horizontal travel to the goal is geometrically feasible against these neighbors
and the terrain; this is a frozen-scene inference, not an executed movement
policy. The concrete next defect is a goal-alignment throat missed by
single-blocker padded-corner search, not permission to push already arrived
units or alter assigned goals.
The primary's independently checked frozen-scene artifact is
`artifacts/cooperative-final-redistribution-static-paths-2026-09-26.json`;
I inspected its final endpoints and path and confirmed it refers to this final
single-pending snapshot rather than the earlier two-pending probe.

## Final scale evidence

Inspected `artifacts/cooperative-scale-2026-09-26/summary.json`, the current
Debug/Release executable fingerprints and each completed case's twelve trace
files (record, Debug and ten Release playbacks). Each case's traces match its
recorded SHA256. Recomputed raw record timings and living/exact-goal endpoints;
the values agree with the independent harness summary. The verifier's source
checks every recorded tick against canonical goals, terrain and full relative
sweeps; this reviewer inspected its code and completed audit results rather than
rerunning the multi-million-row geometry work alongside performance sampling.

| Final case | Living / at assigned goal | Step p95/p99/max ms | Audited rows |
| --- | --- | --- | ---: |
| 200 crossing | 200 / 0 | 1.9503 / 2.4495 / 6.8821 | 800,200 |
| 200 repeated orders | 200 / 0 | 2.2173 / 2.4839 / 4.2851 | 800,200 |
| 200 Stop/resume blockers | 200 / 0 | 1.8243 / 2.0530 / 2.7380 | 800,200 |
| 200 AI combat | 10 / 2 | 0.7603 / 0.9833 / 1.3444 | 800,200 |
| 500 crossing | 500 / 0 | 2.7471 / 3.0667 / 7.1788 | 2,000,500 |
| 500 repeated orders | 500 / 0 | 3.2742 / 3.7505 / 5.0344 | 2,000,500 |
| 500 Stop/resume blockers | 500 / 0 | 2.8185 / 3.2617 / 4.2644 | 2,000,500 |
| 500 AI combat | 48 / 0 | 3.3485 / 3.7615 / 5.2034 | 2,000,500 |

All Release record/replay observations in these eight cases pass the existing
numerical cost/memory gate. Debug timing is retained and is not Release budget
evidence. The blocker case records 1,600 stationary Stop samples and four
commands. AI goal matches are not movement arrivals or full-match acceptance.
Every completed noncombat case remains a failed crowd-progress gate.

Final summary completed 2026-09-26 09:17 America/Los_Angeles. Recomputed totals:
96 executions and 384,000 tick hashes; 11,202,800 audited unit rows, including
706,699 oblique steps. All 88 Release record/playback observations satisfy the
unchanged p95 <=4 ms, p99 <=8 ms and resident-memory limits. Worst observed
Release p95/p99 are 3.4181/3.9573 ms and peak resident memory is 5,468,160 bytes.
The retained maximum step spike is 21.2734 ms in 200-repeated Release repeat 5
(that run p95/p99 2.6074/3.2514 ms). The percentile budget passes; this maximum
must not disappear from the report. All six noncombat cases still have zero
exact arrivals, so the full crowd-progress milestone remains rejected.
The final 500-unit crossing stops moving at tick 604 and its maximum pending
idle remains 3,709 ticks; the later last-motion tick is not a repair.

Inspected both scale-input suite summaries: each configuration rejects 25
malformed replays/override combinations, preserves 18 output-alias cases,
rejects 14 ledger corruptions and passes two analytic sweep controls. These
include frozen-despite-Move and first-Stop drift corruption checks.

## Network preservation

Inspected `artifacts/cooperative-network-2026-09-26/summary.json`, including
all 22 case verdicts, distinct peer process IDs, completion/error states and
timing limits. Main successful cases complete 1,000 ticks; bounded recovery
cases complete 100 ticks, while incompatibility/desync/disconnect fixtures
produce their expected failures. Ten repeated playbacks agree. Independently
checked both peer and both headless executable fingerprints against the summary;
all four match the current files.

The 80 ms RTT impaired case reports generated-command p95 101.8206/101.4633 ms,
within the 150 ms diagnostic limit. The strict at-least-20-Hz booleans remain
false; the 160 ms/delay-2 case is about 19.776 Hz. These sparse tick-aligned
generated commands over loopback do not establish human input response,
physical network behavior, complete matches or a 30-minute soak. This is
preservation of actual separate-process transport coverage, not an in-process
simulation substituted for networking.

## Packaged controls and visual inspection

Inspected the final movement, crowd, offline and 500-unit controls JSON reports
and the independent movement/crowd verifiers. Direct, terrain-detour, off-center
retarget and nine stationary Stop samples pass. The small crowd report covers
400 ticks and 4,800 unit rows with eight evidence corruptions rejected. Offline
selection, orders, combat and restart pass. The scale report passes all 23 checks.
Its report fingerprint matches the replay summary: 700 tick hashes match Debug
and ten Release playbacks; 350,500 rows pass full swept checks, including 77,137
oblique steps, and eight corrupted reports reject. These existing packaged
fixtures preserve integration; they are not the new six-unit tangent-chain
fixture or the failed opposing-stream completion gate.

Actually viewed the final `cooperative-crowd-2026-09-26-chain-arrived.png`,
`cooperative-scale-controls-2026-09-26-wide.png`,
`cooperative-offline-2026-09-26-battle.png` and
`cooperative-crowd-contact-2026-09-26.png` in artifacts. The ten-frame contact
shows a selected walker moving around the left-side line and settling beneath
it, followed by stationary samples. This is sampled visual evidence only: no
continuous movie playback, skeletal deformation, transition smoothness or human
input-response proof was obtained by this reviewer.

Concrete visual shortcomings remain: pale walker bodies lose material detail
in bright highlights; adjacent silhouettes and health bars visually stack;
the selected 250-unit force becomes a dense cyan/white texture at overview;
terrain and ridges remain plain blockout geometry. Small-scale team colors and
HUD text are readable, but these images cannot accept production quality.
The contact's small actor pixels are especially unsuitable for animation claims.
No SC2 side-by-side comparison or human RTS playtest is established here.

The final six rendered separate-process network cases also pass expected
outcomes in `artifacts/cooperative-client-network-2026-09-26/summary.json`:
clean, 80/160 ms RTT, held frame, delay mismatch and disconnect. The first four
complete 240 ticks; mismatch rejects at tick zero and disconnect terminates at
tick 52. Inspected distinct process IDs and checked all five current package and
headless-replayer fingerprints against the recorded fingerprints; all match.

Functional network success does not pass input response: at 80 ms RTT the
event-to-execution p95 is 200.337/133.904 ms, so one peer misses the 150 ms target.
Event-to-display p95 is 233.327/166.692 ms. These are sparse synthetic input
samples, not human latency. Final scripted offline/500-unit frame p99 is
17.002/118.716 ms, both above 16.67 ms. The reports include vsync, capture and
fixture conditions; no ordinary-play, reference-hardware or causal performance
improvement follows. Movie-encoding timings are excluded from performance claims.

## Final verdict

Accept the bounded follower repair and regression/integration checkpoint. The
baseline demonstrably stopped or diverted legal tangent followers; final source
advances both six-unit directions exactly, preserves blocked Stop/Hold and
retarget semantics, and passes the reviewed full-sweep, cross-build, replay,
static-route, transport and packaged-control evidence. Stable bounded integer
transactions add no unhashed persistent state. The final correction avoids
retaining an unnecessary new detour after successful cooperative retry.

Reject general crowd-progress, response, production-art and shipping acceptance.
All six dense noncombat scale cases still have zero arrivals; the smaller
reversed-goal redistribution only reaches five of six exact destinations.
Transaction-cap exhaustion and general large-convoy responsiveness lack direct
fixtures. Release numerical simulation budgets pass with the maximum spike
retained, while rendered-frame and one 80 ms input-response observation fail.
No complete RTS matches, reference-hardware acceptance, manual play or AAA/SC2
quality approval is established.

Next: implement and execute collision-checked goal alignment for the final
unit-2 tangent throat, preserving arrived units and exact goals, before trying
broader merge/egress coordination. Keep the optional six-unit failure and
unchanged 500-unit crossing as rejection gates. This report is complete;
Git integration and automation handoff belong to the primary agent.

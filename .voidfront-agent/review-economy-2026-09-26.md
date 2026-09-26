# Independent economy increment review

Reviewed 2026-09-26 10:04 America/Los_Angeles (17:04 UTC). Reviewer owned
only this report, read source and actual evidence, and performed no Git or build
operations. STOP was absent and the primary run marker remained present.

**Verdict: accept the bounded offline salvage/construction increment. Reject
shipping, complete-small-match, network-economy and AAA/SC2-quality acceptance.**

## Scope and source findings

Reviewed `sim/economy.cpp`, simulation integration/header/tests, bridge economy
snapshot/input/replay methods, client input/HUD/fixture and
`tools/verify_economy_capture.py`. Authority remains in standalone integer C++:
workers, cargo, reserves, balances, construction and dynamic navigation are
hashed; `economy.cpp` participates in the content fingerprint. Compatibility is
version 6. Economic commands share canonical serialization; variable funds and
placement validate when applied, with deterministic result/sequence feedback.

Placement rejects overlapping units/terrain/resources/structures, requires an
own-anchor link, reserves access spacing, validates a builder approach before
deduction, caps structures below navigation capacity and invalidates routes.
Reissuing Build at an unfinished own site resumes without another debit.
These are bounded small-map rules, not general construction/pathfinding proof.

Findings sent to the owners and addressed:

- A subsequently occupied service goal could permanently stall a harvester or
  builder despite free perimeter slots. Deterministic staggered service-slot
  reselection and a held-blocker regression now cover the reported harvest case.
- F2/control-group recall retained an old selected deposit/building, making the
  HUD conceal worker/cargo selection. Entity selection now clears appropriately;
  the runtime fixture also click-selects a worker before using F2.
- Placement preview promised unaffordable sites would be red while geometry
  validation alone could show green. The client now also checks salvage.
- The independent input audit now requires ReturnCargo as well as Gather,
  Stop and Build before accepting the claimed interaction sequence.
- Same-tick competing purchases now test one funded purchase followed by a
  deterministic insufficient-salvage result and conservation.

The first source fixture stopped funding at tick780 with 91 salvage and active
workers. Its deadline was too short for the explicit early return plus repeated
trips. The revised fixture permits the observed cycle duration without changing
gather speed, costs or acceptance budgets. Failed artifacts are retained at
`artifacts/economy-source-probe-2026-09-26.*`. The earlier partial build's failed
service-slot test is also retained; final integrated evidence supersedes it.

## Evidence inspected

- `artifacts/economy-final-build-2026-09-26.log`: integrated MSVC Debug and Release
  8/8 CTest each, malformed/alias regressions and 2,000-tick cross-build/ten-repeat
  skirmish traces. The economy tests cover ownership, application-time rejection,
  cargo interruption, final deposit competition, conservation, construction
  pause/resume, one foundation detour, occupied service recovery and different
  future-command arrival times. Fixture state injection is used for isolated
  depletion/held-blocker cases; this is not player interaction evidence.
- `artifacts/economy-package-2026-09-26.json` and its stdout log: the exported
  package passes 25 software InputEvent checks, finishing at tick1045, hash
  `805025cf101424f6`. The player
  selects workers/deposits, gathers, explicitly returns, funds a Foundry, stops
  harvesting, tries invalid placement without spending, cancels placement,
  constructs, pauses/resumes without another debit and restarts identically.
- `artifacts/economy-replay-2026-09-26/summary.json`: 6,276 worker rows across
  1,046 snapshots conserve 4,000 salvage including deposits/cargo/spent cost;
  100 construction increments are audited. Independent rational segment/sweep
  geometry checks movement around recorded obstacles and between workers.
  Eight actual input commands bind to the recording; one Debug and ten Release
  replays match every executed tick and final economy state. Seven corrupted
  evidence variants reject. Those mutations establish rejection, not isolated
  coverage of every validator branch.
- Personally inspected the final package's harvesting, construction and completed
  PNGs. Resource balance, carried cargo, cost, Foundry progress and selection are
  legible. The construction image says 1%; the completed image identifies the
  selected completed Foundry with salvage1. These images support presentation
  state only; the input ledger/replays provide behavioral evidence.
- Follow-up regression summaries: `artifacts/economy-network-2026-09-26/summary.json`
  has 22 verified separate-process original-skirmish cases. The rendered package
  suite at `artifacts/economy-client-network-2026-09-26/summary.json` passes six
  cases and retains ten clean replays. The 400-tick combat regression passes at
  hash `55e093f505012de9`, winner1. Existing bounded crowd checks retain 17 fixtures,
  20 runs, 30,254 rows and 22 rejected mutations in
  `artifacts/economy-crowds-2026-09-26/summary.json`. These preserve existing
  behavior, not economy multiplayer or previously failed dense-stream progress.
- Follow-up movie evidence: personally inspected the ten sampled frames in
  `artifacts/economy-movie-contact-2026-09-26.png`, derived from
  `artifacts/economy-movie-2026-09-26.mp4` (primary agent reports 52.3 seconds,
  3,138 frames, 60 FPS, 1600x900). The samples show changing worker positions,
  recurring carried-cargo/returned-balance states and the later Foundry stage.
  The separate movie-run audit at
  `artifacts/economy-movie-replay-2026-09-26/summary.json` also verifies 1,045
  ticks, 6,276 worker rows, 100 construction increments and Debug/ten Release
  replays. This was contact-sheet inspection, not continuous video playback;
  sparse frames cannot establish animation smoothness, deformation, input feel
  or frame pacing. Label overlap, bright cargo and blockout geometry persist.

The package's once-per-second sampled resident peak is 267,206,656 bytes over
54.26 seconds. No frame-time, human input latency or reference-hardware performance
acceptance follows. Concurrent transport regression timings must not be treated
as isolated latency/performance measurements. This report does not infer economy
networking from original-skirmish network tests. The rendered 80 ms RTT case's
second peer has event-to-execution p95 150.161 ms, above the 150 ms target; sparse
concurrent-run observations do not establish responsiveness acceptance.

## Remaining gaps and next acceptance boundary

Actual captures still show deposit/building text intersecting worker health bars,
bright cargo/deposit cubes, scaled striders serving as workers, simple blockout
structures/terrain and no authored worker/construction animation. There was no
new reference-image comparison, motion-quality review, human playtest or audio
acceptance in this review. The increment visibly calls itself a prototype.

Foundries cannot produce units yet; economic AI, flux, technology, fog/scouting,
anchor combat victory and complete matches remain absent. Existing dense-stream
arrival, renderer timing and general crowd-quality failures remain open. Six
workers and one built foundation cannot establish dynamic placement quality for
arbitrary layouts or the 200/500-unit contract. Some service-slot replan cases
are covered by source tests only, not the packaged interaction fixture.

Next implement resource-funded production queues and population limits in the
same canonical simulation/client path, then economic AI and anchor victory.
Retain the recorded economy regression and require packaged purchase, queue,
spawn, selection and replay evidence. Do not create COMPLETE.md.

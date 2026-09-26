# Independent crowd recovery review - 2026-09-26

Scope: independent design, source, regression and actual endpoint/routing evidence
review. This reviewer owns only this report and performed no builds, simulation
launches, Git operations or heavy ledger audits. Read AGENTS, SPEC, PROGRESS,
TESTING, roadmap, the unchanged scale contract and the September 12/13 failed
crowd investigations. STOP was absent; the primary owned the active run marker.

## Baseline and acceptance boundary

The fresh `artifacts/crowd-baseline-2026-09-26/crossing.json` reproduces the retained
500-unit, seed-42, 128x128, 4,000-tick failure: all 500 alive, zero exact arrivals,
last movement at tick 530. Units 248/498 remain at (16317,16326)/(16451,16327),
stationary since tick 291. This reviewer confirmed these from actual metrics;
the earlier full trajectory audit remains separately dated.

All exact assigned goals, terrain, tick length, live population and canonical
opposed Move orders remain the gate. Partial arrivals and movement through the
mouth are useful diagnostics but cannot pass it. A low p95 during thousands of
stationary ticks is not useful crowd-progress acceptance.

## Design and source criticism during development

The initial review rejected counting lateral activity as progress. It reiterated
that coordinated recovery must preserve Stop/Hold, exact arrived units, the actual
firing decision and all simultaneous relative sweeps. A common translation only
preserves internal relative clearance when participant displacements are equal;
including units that already moved needs a different explicit proof. Persistent
recovery state must be hashed and cleared by explicit orders. Existing
`blocked_ticks` resets during unsuccessful local-detour searches and is not a
reliable long-term no-progress clock.

The experimental implementation instead used preventative directional routing
through portals derived from facing rectangle edges. Review requested and found:

- Both opposed cohorts are considered after all due commands have applied, so
  player-0/player-1 application order does not omit the first cohort.
- Eligibility checks the actual static route's integer intersection with the
  chosen portal. Opposite endpoint sides alone must not divert a path using an
  outer opening into the middle gap.
- Canonical final goals remain intact; separate temporary paths, heading and
  corridor fields are hashed and reset by commands. Experiments participate only
  in Move, preserving ordinary AttackMove firing behavior.
- Each actual move retains the existing full terrain and unit-sweep checks.
  The later half-corridor restriction checks whole segments, including every
  speculative detour leg, rather than checking only endpoints.
- Temporary entry/exit crossing is distinct from exact final arrival. Forcing a
  walker back to an exact intermediate exit can create head-on movement against
  the same original stream.

Distinct approach coordinates do not constitute an enforced queue or reservation
system. They only stagger geometric targets. Their successful ordering cannot be
assumed from source comments. Hard retained halves can also forbid a feasible
route around a held blocker after opposing traffic clears. Release must follow
actual geometric occupancy, not merely whether an opponent still has a distant
temporary exit in its path. This remained an experimental liveness gap.

## Actual six-probe evidence

Read each complete metrics file and independently recomputed exact arrivals,
alive counts, last-motion ticks, maximum pending idle and nearest-rank step
percentiles from all raw step timings. Every probe retains all 500 living units.
Files are `artifacts/crowd-portal-probeN-2026-09-26.json`, N=1 through 6.

| Probe | Policy change | Exact arrivals / 500 | Last movement tick | Longest pending idle | Step p95 / p99 ms | Maximum step ms |
| --- | --- | ---: | ---: | ---: | --- | ---: |
| 1 | Shared approach and exit lanes | 25 | 1000 | 3735 | 1.6700 / 2.0237 | 2.7182 |
| 2 | Align just ahead of own formation | 0 | 755 | 3967 | 1.9620 / 2.3291 | 3.1092 |
| 3 | Distinct longitudinal approach gates | 12 | 1791 | 3732 | 1.6740 / 1.9763 | 2.1013 |
| 4 | Egress before nearest canonical goals | 49 | 1924 | 3732 | 1.6554 / 1.9860 | 3.2137 |
| 5 | Exit-plane crossing and retained half | 84 | 1588 | 3676 | 1.7380 / 2.1416 | 2.4277 |
| 6 | Wider entry tolerance and dynamic half release | 72 | 1687 | 3762 | 1.7227 / 1.9010 | 2.0266 |

These are exploratory Release observations on this development host. They are
not cross-build movement acceptance, a full independent swept-ledger audit or
reference-hardware proof. Initial command steps range 1.2009..1.3788 ms; retained
maximums above include command work. The long stalled tails remain included.
No candidate passes the unchanged dense-progress gate.

### Concrete routing diagnosis

Probe 2 jams its own approach well before the terrain mouth. IDs 5/6/7 finish at
x=6022, z=15292/15423/15552 and stop at tick 136; early westbound IDs 251..258
mostly stop at ticks 62..116 around x=26700. Merely moving the shared entry
earlier relocated compression instead of coordinating the merge.

Probe 3 has 159 eastbound endpoints beyond the right ridge face and 218 westbound
endpoints beyond the left, but only 12 exact arrivals. Its retained final routing
dump is `artifacts/crowd-portal-probe3-routing-2026-09-26.json`. Of 500 units,
10 still have both temporary gates, 467 still target the exit, and only 23 have
cleared it; 11 of those 23 still miss their canonical goal.

The dump exposes specific incompatible effective directions: unit 498 at
(3611,16869) has cleared its westward exit and now wants (7552,17024) to the east;
neighbor 485 at (3751,16923) still wants westward exit (3456,17056). Unit 480 at
(3320,16965) has already crossed the exit's longitudinal plane but still aims
back at its exact center (3456,16864). Other pending westbound units 251/252
have detoured to z=16294/15598 despite a designated upper lane z=16480. Original
Move direction therefore does not describe every unit's current recovery goal.

Probe 5 prevents much of that middle crossing: 246 eastbound and 248 westbound
endpoints are already beyond the opposite ridge face. Nonetheless, 354 still aim
at the shared exit outside the constrained mouth, and 61 of the 145 units that
cleared their exit remain short of their canonical goal. Only one unit retains
both gates. Most remaining failure has moved to same-stream downstream movement
and final goal redistribution. Releasing the six remaining mouth crossings
cannot by itself resolve hundreds of downstream pending exits.

These counts come from actual endpoint/routing dumps, not a claimed continuous
visual inspection. No new movie, human playtest or packaged movement acceptance
is established by this investigation.

## Regression review and checkpoint recommendation

Reviewed the new C++ open-square relative-sweep interval checker and separate
Python full-ledger semantics. They check simultaneous relative paths rather than
only endpoints; tangent and corner-crossing analytic cases distinguish them.
New fixtures preserve short nonintersecting opposed journeys, a shooter whose
target leaves range after its shot, explicit Stop/Hold followed by retarget,
and outer-channel movement that must not divert to the central portal. These
are meaningful preservation tests, not proof that dense movement was repaired.

Reject all six authoritative simulation candidates. The best result still has
416 permanently unfinished units, while the new merge/egress and lane-lifetime
behavior lacks acceptance evidence. Retain the experiments and routing diagnosis,
restore the baseline simulation, and land only verified regression coverage and
non-authoritative diagnostic output. Do not present 84 arrivals as a repaired
crowd system or silently weaken the 500-arrival contract.

The next defensible algorithmic step is bounded cooperative merge and egress
planning: explicit longitudinal slot progression and follower dependencies,
geometrically valid exits before final-goal redistribution, and transaction
checks for any simultaneous coordinated displacement. It must address both
entry and exit ordering without shared exact bottlenecks or reversals into the
incoming stream. Repeating entry-coordinate, tolerance or half-width tweaks is
not supported by this evidence. Stop/Hold/firing, exact goals, complete relative
sweeps, deterministic work limits, cross-build hashes and the original full
fixture remain mandatory. All full-match, production and shipping gates remain
open; no movement-progress or AAA approval.

## Final restored-baseline evidence

Final evidence inspected at 2026-09-26 01:48 America/Los_Angeles:

- `artifacts/crowd-final-build-2026-09-26.log` records 7/7 MSVC tests in
  both Debug and Release, malformed replay/output-alias rejection, and 2,000
  matching tick hashes across configurations and ten Release repeats.
- `artifacts/crowd-final-small-2026-09-26/summary.json` records all twelve
  fixtures, 17,630 ledger rows, ten Debug plus ten Release runs with identical
  ledgers, and seventeen rejected mutations. I independently recalculated the
  totals and checked both current crowd-test executable SHA256 values against
  the summary. This includes the original seven fixtures; the outer-channel
  prefix explicitly does not promise eventual completion after contact.
- `artifacts/crowd-final-scale-2026-09-26/summary.json` records 2,000,500
  ledger rows, 118,406 oblique steps, both canonical commands and full swept
  clearance, followed by one Debug and ten Release replay runs. I checked the
  current headless executable hashes, ledger/replay/trace SHA256 values and all
  twelve full trace files against the recorded stream. The 4,000-tick trace
  also matches the fresh pre-experiment baseline byte for byte. This reviewer
  inspected the independent verifier's completed audit and its source; I did
  not redundantly execute its full two-million-row sweep audit.
- Recounting raw final metrics gives 500 alive, **0 exact arrivals**, last
  motion tick 530 and maximum pending idle 3,709. Protocol 4 and content ID
  `2323859433208175334` remain unchanged. Every final unit has the expected
  `next`, `route_goal`, `blocked_ticks`, `path`, and `detour` diagnostic fields.
- The final Release record reports simulation p95/p99/max
  1.7505/1.9971/2.4291 ms and peak resident memory 5,054,464 bytes. Passing that
  bounded cost gate while all units remain stuck is not a movement acceptance.

The primary agent reports byte-identical restoration of both authoritative
simulation files and retained the rejected patch at
`.voidfront-agent/experiments/2026-09-26-rejected-portal.patch`; the independent
trace comparison corroborates unchanged behavior for this fixture. Git
integration belongs to the primary agent and was not performed by this reviewer.

Final verdict: accept the regression tests and observational routing diagnostics
as a bounded checkpoint. Reject all six movement candidates and retain the
dense-stream milestone as failed. No new packaged play, networking, animation,
rendering, full-match or shipping claim follows from these headless checks.

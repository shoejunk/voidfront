# Independent crowd recovery review - 2026-09-13

Scope: read-only independent source and retained-evidence diagnosis. Primary agent
owns implementation, builds, Git and integration. This report is not shipping
acceptance. Reviewed SPEC, PROGRESS, TESTING, roadmap, current movement and spatial
sources, crowd tests, scale contract, retained crowd failure, scale auditor, and
actual 500-crossing metrics.

## Verified baseline

The retained `artifacts/scale-verified-2026-09-12/500-crossing.json` contains 500
live units, zero exact-goal arrivals, and last movement anywhere at tick 530 of
4,000. Units 248 and 498 both last moved at tick 291 and finish at (16317,16326)
and (16451,16327), respectively. This confirms the documented dense-stream gap;
low simulation cost during the stationary tail is not progress acceptance.

Current movement tries heading-adjacent integer candidates, then a complete
one/two-segment maneuver around a padded box for the first blocking unit. Every
candidate must be terrain-clear and clear of all relevant unit sweeps. A local
pocket can therefore reject all complete maneuvers despite safe immediate lateral
motion. Enlarging the stationary local obstacle planner cannot create freedom.

Independent reconstruction from the retained final positions using the separate
midpoint collision oracle found:

- Unit 248 can move +z32 alone. A -z32 translation needs only unit 247 translated
  with it; neither member intersects expanded terrain.
- Unit 498 can move either +z32 or -z32 alone.
- Horizontal translation dependencies expand to 25-30 members in this region.

These are feasible isolated snapshot actions, not proof a policy will complete
crossing. They support small lateral yields or pre-contact avoidance as a bounded
next experiment. The unchanged 4,000-tick crossing remains the acceptance fixture.

## Conservative implementation options and review requirements

Prevention: detect opposing effective headings several diameters before collision,
choose a deterministic right-hand separation direction, and test the resulting
short displacement with existing swept clearance. Persistent avoidance state must
be hashed and explicit commands must clear it. Goal coordinates and static route
quality are retained; completing/abandoning a temporary lateral maneuver must
refresh the route from the current position.

Recovery alternative: collect a small bounded dependency component and propose
one common lateral displacement. Its internal pair offsets remain constant;
external collisions and terrain must be checked before applying the transaction.
Stop/Hold, dead bodies, and currently firing AttackMove units cannot become
implicit yield participants. Apply no partial transaction and bound roots,
component size and neighbor queries in stable ID order. Exact simultaneous
relative-sweep checks are needed where this broadens the current union-of-sweeps
policy. The existing scale auditor already checks relative interpolation, so its
safety criterion need not be weakened.

Required criticism of any landed change: unchanged canonical fixture and goals,
complete ledgers, every swept unit/terrain interval, immediate Stop/Hold/retarget,
repeated and Debug/Release hash equality, all timing samples including command
spikes, and bounded active-crowd cost. Increased moving samples or fewer stalled
units are useful partial progress but cannot pass the all-arrival crowd gate.

## Current verdict

Diagnosis is evidence-backed. Implementation and new-run evidence review pending.
No crowd-progress, runtime responsiveness, general navigation or shipping approval.

Additional independent baseline check: streamed all 2,000,500 retained CSV rows
and matched every final coordinate and last-motion tick to metrics. The last
movement is tick 530; 248/498 both last move at tick 291. This verifies the
stationary-tail diagnosis from actual trajectories rather than metrics alone.

Canonical transaction-fixture candidate (geometric, not yet executed): use
Sim(1,4), single-unit setup Moves with exact-arrival waits, and place own units
in order ID4=(1024,2700), ID3=(1024,2420), ID2=(1158,2561), ID1=(1024,2560).
Hold ID4. Issue same-tick Moves ID1->(2304,2560), ID2->(512,2561),
ID3->(2304,2420). ID1's -z32 action requires ID3 to co-translate; +z32 must reject
because it intersects held ID4. The common {1,3} translation is geometrically
clear at this setup. This tests bounded dependency handling and Hold protection;
it is not a demonstrated permanent-lock regression because ordinary detours may
solve it after the upper unit moves. Setup and execution require primary-agent
validation in the actual simulator before any acceptance claim.

## Exploratory preventative patch review

The first uncommitted experiment adds right-hand waypoint bias for any opposing
pending mover within four cells and ahead. Actual-step terrain/unit sweep checks,
Stop/Hold exits, integer arithmetic and static-route invalidation remain intact.
No new safety violation was identified by source inspection alone.

Concrete behavioral gaps reported before landing:

- A biased waypoint can point into a ridge. If every heading-adjacent candidate
  fails terrain clearance, blocker remains zero and the unit does not enter the
  existing unit-blocker recovery. Retry the clear original heading, or require
  the biased segment to be terrain-clear before selecting it.
- Dot/ahead/range tests detect opposing traffic without verifying an actual
  collision course. Example: moves (1024,2560)->(1124,2560) and
  (1624,2560)->(1524,2560) remain at least 400 apart, yet trigger substantial
  bias. A finite-horizon encounter check must clamp to remaining travel; exact
  safe arrivals need priority. This is an authored geometric counterexample,
  not an executed simulation regression.

The unchanged 500-unit/4,000-tick probe was pending when these findings were sent.
This experiment has no acceptance verdict until actual results and final source
are reviewed.

## Coordinated-yield experiment: rejected, not shipped

Primary removed the preventative experiment after its unchanged full 500-unit /
4,000-tick crossing produced zero arrivals. Primary then tested a bounded postpass
co-translation experiment: at most 16 pending, nonmoving participants, a shared
32-unit lateral translation, 64 neighborhood expansions per tick, and rotating
request roots. External full tick sweeps and expanded terrain remained mandatory.

Independent source review found the intended simultaneous safety argument sound
for nonmoving participants: shared displacement preserves all internal relative
positions. External conservative sweeps and broadphase expansion cover both unit
motions. However, firing eligibility was checked using target distance after all
normal movement. A shooter may fire in range early in the tick, then its target
moves away, permitting the shooter to join a later yield group. A future version
needs eligibility captured at the actual firing decision, not recomputed distance.
This finding was sent before acceptance; the entire experiment was removed.

Actual `artifacts/crowd-yield-2026-09-13.json` reports zero arrivals, all 500 alive,
maximum pending stationary tail 3,651 ticks, and some movement at tick 4,000.
Primary reports the unchanged independent auditor passed all 2,000,500 rows.
This reviewer stopped a redundant audit when primary disclosed its active audit;
no second full audit is claimed. Metrics and the final 3,999/4,000 CSV snapshots
were independently inspected, followed by exact axis-aligned dependency closure
reconstruction against the final external sweeps:

- 497 units did not move at tick 4,000. Across 1,988 cardinal translation closures,
  1,334 are clear with at most 16 members; 250 further clear closures exceed 16.
  Another 328 fail terrain (129 also exceed 16), and the other 76 fail against
  one or more of the three units already moving (some also terrain/cap).
- Unit 248 ends (16444,16295), stationary since tick 898. Negative-z closure has
  16 members and hits terrain at 227/244/477/493; positive-z closure has 12 and
  hits terrain at 463. Negative-x closure has 20 members and is terrain-clear.
  Positive-x motion conflicts with moving 482/483.
- Unit 498 ends (16451,16423), last moved at 3,942. Positive-z translation is
  clear alone; negative-z closure has 17 members and hits terrain. Negative-x
  closure has 19 members and is terrain-clear.
- The only tick-4,000 movers are 482/483/484. Across ticks 3,900..4,000, each
  occupies only two positions with identical x=16583 and z values 32 apart.
  Unit 482 alternates z=16180/16148 (73/28 samples); the other two remain still
  for 100 samples and shift on tick 4,000. Continuing motion is lateral
  oscillation, not crossing progress.

Raising the component cap alone is not justified: many small legal lateral
translations already exist. The stalled middle is pressed into both ridge ends.
A future bounded experiment should deliberately decompress backward from the
blocked front, preserve a passing side across ticks, and avoid accepting repeated
side-to-side activity as progress. It must retain eligibility captured at firing,
Stop/Hold immobility, exact goals, complete unchanged crossing ledgers, deterministic
hashes and workload bounds. Goal approach and successful crossing remain the
measures; none of the observed oscillation passes the crowd gate.

Final movement verdict: both experiments rejected and removed. Primary reports
baseline simulation restored byte-identically. No movement-recovery change is
approved or shipped by this review. General crowd progress remains a blocking
roadmap gate; production gameplay and shipping acceptance remain open.

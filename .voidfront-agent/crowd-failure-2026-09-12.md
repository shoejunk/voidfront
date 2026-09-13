# Dense crowd failure retained for the next movement increment

The real 128x128 crossing fixture is a failed progress gate. It has two long
terrain ridges, an eight-cell central passage, 500 live units and canonical group
Move commands in opposite directions. Do not replace it with a smaller, shorter,
empty or one-direction scenario to claim this gate passed.

`artifacts/scale-crossing-before-2026-09-12.csv` contains 2,000,500 rows, including
the initial state and all 4,000 ticks. The independent critic checked its final
positions against metrics: all 500 remain alive, zero arrive, and every position
is unchanged from tick 531 through tick 4000. Units 248 and 498 stop at tick 291,
at (16317,16326) and (16451,16327). Their forward steps collide reciprocally.
Neighbors block all four complete padded-corner maneuvers, even though some
immediate lateral displacements are clear. This is a dependency between moving
neighbors, not a valid reason to displace a Stop/Hold unit.

Rejected experiments are retained under artifacts, not in the final simulation:

- A two-queries-per-tick rotating fallback built temporary visibility worlds with
  the nearest twelve swept bodies. Zero units arrived. The critic reconstructed
  valid starts/goals with no route and independently flood-filled the rectangle
  arrangement: unit 248's reachable pocket was only x=16313..16323,
  z=16324..16389. Adding more stationary obstacles cannot open that pocket.
- Short one-sided lateral maneuvers also produced zero arrivals. The critic found
  that testing only the first step could commit a waypoint inside terrain and
  then bypass unit-blocker recovery. That experiment was removed completely.
- Two predictive velocity candidate experiments produced zero arrivals. The
  larger candidate search was also much too expensive. One build attempt
  overlapped that first predictive run and failed to link its running executable;
  its timing is explicitly excluded from acceptance evidence. The reduced
  variant still missed progress and the p95 target. Neither is shipped.

The current source retains the previously verified bounded local detours.
The scale verifier correlates every recorded command with independently assigned
goals, checks every unit and interpolation interval, retains dead/unfinished IDs
and all raw timing samples, and reports safety/determinism separately from
progress. Passing performance while stationary does not pass crowd movement.

Next work needs coordinated yielding among units already ordered to move, or
avoidance that prevents formation compression before the pocket closes. Preserve
immediate Stop/Hold/retarget, exact single-unit goals, arbitrary headings, stable
identity/order, hashed planning state, conservative swept clearance, deterministic
work limits, the 4/8 ms budgets and complete cross-build/repeated traces. A
reduced canonical reproducer may supplement this fixture, never replace it.
Dynamic construction, per-command response, rendered large-map behavior and
human/reference-hardware acceptance remain separate requirements.

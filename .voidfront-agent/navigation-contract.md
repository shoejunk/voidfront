# Bounded static navigation contract (2026-09-07)

Declared before implementation/reference comparison results: each reachable fixture
must return a feasible route whose Euclidean length is at most
`1.01 * independent_shortest_length + 2` fixed-point coordinate units. One world
unit is 256 coordinate units. Report each excess and the worst/mean excess;
passing this tolerance accepts only these bounded static route fixtures. It does
not accept the full SPEC movement, crowd, dynamic obstacle, responsiveness,
representative performance, or shipping gates.

The navigation API is `vf::nav::World` in `sim/navigation.hpp`, constructed from
an axis-aligned bounds rectangle, obstacle rectangles, and integer clearance.
Invalid world input throws `std::invalid_argument`; `valid(Point)` reports legal
centers, `clear(Point, Point)` reports segment feasibility, and
`route(start, goal)` returns ordered integer waypoints excluding start and
including goal. An invalid endpoint, unreachable goal, or equal endpoints returns an empty
route; the probe catches invalid-world exceptions and distinguishes these with
validity/clear flags. World input coordinates have absolute value <=32768,
clearance is 0..32768, and at most 64 positive-size rectangles are permitted.
An exposed graph exceeding 1024 vertices throws. Collapsed shrunken bounds
(line/point) are legal; inverted shrunken bounds reject.

Clearance is the axis-aligned square half-extent (64 for the current walker),
implemented geometrically by shrinking bounds and inflating obstacles. Inflated
obstacle open interiors are forbidden, while exact boundary tangency is allowed.
This deliberately is square clearance, not an approximation claimed to be an
exact circular-radius shortest path. Straight segment interiors must obey the
same rule. Integer ceil-Euclidean edge weights and stable ties are authoritative;
the comparison reference uses unrounded Euclidean lengths.

The independent Python reference (`tools/verify_navigation.py`) uses exact
rational edge-event subdivision and midpoint classification for collision,
cross-checked against slab clipping on 4,000 seeded random segments and all
fixture-derived corner/endpoint pairs. It uses a Cartesian product of
obstacle/bounds edge coordinates for its visibility graph. It uses floating
Euclidean distances and Dijkstra solely as an offline oracle, never in the
simulation. Analytic open/oblique, single-box, long-detour, corridor and disconnected fixtures
validate reference behavior separately before testing binaries. The reference
has more candidate points than the C++ obstacle-vertex graph and does not reuse
its collision implementation or integer edge weights. Both are visibility-based
polygonal shortest-path methods; this shared mathematical model is a limitation,
so analytic fixtures and independent segment checks remain mandatory.

Fixtures include open oblique and tiny off-center travel, a stair-stepped angled
wall (axis-aligned approximation, not arbitrary polygon support), concavity,
overlapping inflated obstacles, exact-pass and blocked narrow passages,
unreachable goals, long detours, boundaries, invalid inputs and maximum input
capacity. Debug/Release route JSON must be byte-identical across ten runs each.
The verifier preserves inputs, routes, individual errors, executable SHA256 and
start/end timestamps in a new evidence directory; existing directories reject.

These tests do not establish unit stepping, crowd avoidance, dynamic replan
quality, runtime playability, network transport, or performance budgets. Those
need separate simulation, replay, packaged gameplay and measurement evidence.

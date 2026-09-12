# Next scale evidence contract

The existing 200/500-unit diagnostics use the 32x24 game map. They cannot pass
the SPEC's 128x128 representative-map requirement, even when measured under 4/8 ms.
The next benchmark must construct and step the actual authoritative Sim on a
128x128 map with real terrain and 200/500 live units. A stand-alone spatial index,
duplicated movement loop, oversized bounds around the old 32x24 scenario, or only
static route queries is insufficient.

Retain setup geometry, spawn positions, canonical inputs, full per-tick hashes,
counts of arrivals/stalls/deaths and non-authoritative timing/memory samples.
Run Debug/Release and repeated identical input streams. Measure initial group
orders, common destinations, repeated orders and moving-blocker replanning.
Report p95/p99 and maximum including expensive command ticks; keep simulations
isolated from builds, other benchmarks, media encoding and packaged captures.

Maintain SPEC targets: simulation p95 <=4 ms and p99 <=8 ms, peak resident memory
<=2 GB. Command admission/execution timing is separate from actual first movement,
network response and rendered feedback. Passing a budget alone does not pass
progress: explicitly report all units still short of their assigned destination
and any repeated/stationary tail. Crossing traffic/chokes must complete with no
overlaps before dynamic crowd acceptance. Static route acceptance remains the
separately declared 1% plus two coordinate-unit tolerance.

The first scale harness may expose incomplete crowd behavior; record the failure
and fix it without weakening budgets, filtering samples, omitting blocked units
or shortening the trial to hide the tail. Dynamic construction needs canonical
construction commands and preserved navigation/replay state, not a test-only
runtime callback that mutates authoritative terrain. Packaged large-map and
human/reference-hardware acceptance remain additional gates.

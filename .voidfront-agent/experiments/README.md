# Rejected movement experiments

`2026-09-26-rejected-portal.patch` preserves probe 6 against source baseline
76507b778705f7e9ae940896e0d0de4a9d549f05. It is excluded from the build. Do not
apply it as a fix or resume it automatically: the unchanged 500-unit/4,000-tick
crossing still has only 72 exact-goal arrivals. Protocol was deliberately not
advanced during the experiments; these recordings are not release content.

The patch adds geometry-derived directional portal lanes, distributed approach
gates, intermediate-plane crossing, a retained half inside the terrain mouth,
and geometric opposing-cohort release. All six variants failed progress.
Probe 5 reached 84 destinations; earlier variants reached 25, 0, 12 and 49.
Local source snapshots, replay/hash traces, endpoint routing and timing samples
are retained under `artifacts/crowd-portal-probe*-2026-09-26.*`. The first two
variants have no retained source snapshot; the patch reproduces only probe 6.

Read the independent review and PROGRESS before further movement work. The
next approach needs coordinated queue entry and exit with follower dependencies,
not another fixed waypoint offset, arbitrary waiting delay or reduced gate.

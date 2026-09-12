# Independent broadphase review

Reviewer: independent movement_review agent, source and retained evidence examined.
Verdict: accepted as a bounded broadphase optimization; no blocking finding.

Confirmed conservative collision reach 128+32+32 and targeting reach 1536+32,
inclusive bucket bounds, ascending-ID tie preservation, and live-at-tick-start
membership equivalence because damage applies after all movement. Compared actual
pre-change source retained in artifacts/crowd-baseline-2026-09-12 with the change.
Requested explicit bounds/insertion/output-clearing tests; these were added.

Independently verified integrated Debug/Release 5/5 CTest, replay/alias checks,
amended Debug boundary pass, all 2,000 baseline/post-change trace rows at both
200 and 500 units, and cross-build plus ten repeat traces. Repeat trace SHA256:
375AB3DC0A4E5411EF859809D8E62BC58036E955B5B30B4423AF8AD18FF13A80.

Observed 200-unit p95/p99 2.440/2.933 -> 0.965/1.532 ms and 500-unit
15.259/16.373 -> 3.898/5.240 ms. Current 32x24 headless harness/development host,
including AI/command work and excluding trace hash/write. The 500-unit p95 has
little budget margin. Both large battles remain ongoing at tick 2,000.

No crowd progress, 128x128/reference-hardware budgets, command latency, new
packaged gameplay, complete-match, production-content or shipping acceptance.

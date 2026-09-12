# Independent bounded crowd review

Reviewer: movement_review independent agent; actual source, logs, CSV/JSON,
binary fingerprints, screenshots and movie-derived contact sheet examined.
Verdict: bounded movement increment accepted; no remaining blocking finding in
the reviewed source/evidence. Completed network suites were subsequently audited.

Findings fixed and rechecked:

- Moving combat targets reset avoidance before its two-tick trigger. A canonical
  fixture reproduced the failure; pursuit updates now preserve local avoidance.
- Completing/abandoning a partial maneuver could leave a static route from an old
  position. All such transitions refresh/invalidate the route.
- The packaged verifier originally anchored Stop to its first application tick,
  allowing unreported displacement on that tick. It now compares against the
  event-tick position and rejects a speed-valid +1 displacement across the pause.
- An analytic terrain-negative fixture was actually a legal tangent. Replaced
  the crossing case with a proven interior intersection and retained the tangent.

Confirmed final integrated Debug/Release 6/6 CTest, replay/alias/cross-build/repeat
checks, all20 matching crowd ledgers and current binary fingerprints. The
allocation refinement preserves every row of both 2,000-tick diagnostic traces.
Active-tick p95/p99 0.924/1.097 ms (200) and3.548/3.858 ms (500) are current32x24
map measurements, not representative128x128 or reference-hardware acceptance.

Normal/movie packaged evidence each verifies all4,800 unit snapshots, five
InputEvents, nine stationary Stop ticks and eight rejected corruptions. Swap
arrivals are32/31; chain arrivals113/112. Inputs are generated separately, so
different dispatch ticks are expected and no equal-stream claim is made.
FFprobe confirms1600x900,60fps,1,201frames,20.016667seconds. All four normal
screenshots, movie Stop/arrival screenshots and twelve-frame contact inspected.
Sampled frames show sideways avoidance and progress past the stationary chain.

HUD is readable. Bright meshes and crowded rings obscure individual clearance;
Stop feedback remains at map origin. Sparse frames do not establish animation
smoothness, human response or playability. The seventh C++ fixture proves pursuit
avoidance activation/safety, not pursuit completion. General choke/stream
robustness, dynamic construction,128x128 budgets, production presentation and
shipping remain open. Static-route evidence explicitly predates allocation
rebuild; the navigation sources were unchanged by that refinement.

## Subsequent network audit

Headless: all 22 cases, eight 1,000-tick ordinary pairs, 44 recordings, 316 timed
local commands, 38 replayed nonempty traces, 20 repeat traces and four current
binary fingerprints independently verified. Packaged retry: six cases, 12
recordings, 28 complete event/queue/canonical/render joins, 20 Debug/Release replay
traces, ten repeats and five current fingerprints verified. Positive pairs
confirm 240 ticks; mismatch rejects 0/0; disconnect preserves 52/51-tick prefixes.
The interrupted first client attempt remains excluded as inconclusive.

Packaged 80 ms RTT p95 event-to-execution 133.881/134.063 ms and first-render
166.713/166.701 ms; 160 ms/delay4 execution 233.902/233.827 ms and first-render
266.927/266.752 ms. Only three inputs/player/profile. Strict 20 Hz remains unmet
in both suites. Bounded loopback transport preservation only; no physical
networking, human latency, complete economic match or 30-minute acceptance.

# Independent scale and controls checkpoint review

The existing movement_review critic examined actual source, retained failure
states, replay/metadata files, adversarial evidence controls and rendered output.
This is a development checkpoint review, not a shipping approval.

## Corrections made before final verification

- Map-aware Sim admission was initially broader than Lockstep receipt. The
  receipt path now rejects coordinates outside the actual map before buffering;
  both axes and maps have regression coverage.
- The scale ledger needed independent canonical-command/goal correlation.
  It now reconstructs destination assignment from an exhaustive sorted reference,
  checks expected orders/goals including dead units and rejects frozen Move
  false arrivals and drift on the first Stop application tick.
- Dense fallback experiments were inspected against actual trapped units. A
  flood-fill independently confirmed a sealed local pocket; unilateral side
  goals could additionally commit into terrain. These unsuccessful approaches
  were removed, preserving the verified bounded local detour implementation.
- Control-group casualty assertions initially risked sharing the implementation's
  filtering helper. Final expected survivors are derived separately and casualty
  ID 1 is explicitly absent after actual combat at tick 346.
- The final helper's navigation `--self-test` invocation tested the oracle only.
  The primary identified the early return and required the full 51-fixture suite
  separately before committing; final evidence is recorded in TESTING.md.

## Evidence inspected

Only eight completed scale case entries are accepted: five from
`artifacts/scale-verified-2026-09-12/summary.json`, three from
`artifacts/scale-retry-2026-09-12/summary.json`. The interrupted first batch's
partial 500-repeated execution is excluded. The critic independently checked
all 96 actual traces: each has 4,000 ordered ticks and matches its case's golden
bytes, totaling 384,000 tick hashes. All 96 metadata sets agree. Parsed 682
canonical VFR3 commands match the recorded summaries and prescribed traffic
schedules. Current Debug/Release fingerprints match both evidence folders.

After timing completed, the critic rechecked all eight large CSV hashes and
exact row counts against the evidence: 11,202,800 independently swept unit rows.
All 88 Release
record/replay observations pass p95 <=4 ms, p99 <=8 ms and the resident-memory
cap on this development host. Maximum spikes remain visible: 28.668 ms at
500-repeated Release repeat 6, tick 1909; 16.973 ms at 500-moving-blockers Release
repeat 0, tick 553. Neither is a command tick; no scheduling-cause claim is made.
All six traffic cases still have zero arrivals with every unit alive. Numeric
performance success during a stationary tail does not pass useful progress.

The critic checked actual report hashes for final packaged crowd and movement:
4,800 crowd rows, swap tick 32, nine Stop ticks, 65 oblique steps and eight
mutation rejections; 400 movement positions, 184 oblique steps, three exact
arrivals, nine Stop ticks and maximum route excess 0.0185896206%. The offline
400-tick report includes selection, orders, combat and restart, winner 1.

Final source controls pass 16 checks; exported controls pass 17 including image
capture, with zero errors. The critic inspected the actual final 1920x1080 image:
three shortcut rows fit inside the bottom panel and feedback is by the selected
actors. Earlier exported `--script` runs hung and are excluded; the normal
`--controls-smoke` entry is the verified path.

Final headless transport review covers all 22 cases, 44 VFR2 recordings,
38 nonempty replay traces, 316 applied local input identities, 20 repeat traces
and four matching current binary fingerprints. The full suite passes; sparse
generated inputs and measured sub-20 Hz rates retain the limitations in TESTING.md.

Final client review covers all six cases, 12 VFR2 recordings, 20 cross-build
replay traces, 28 applied local events, ten clean repeats and five matching
current fingerprints. Four positive pairs confirm 240 ticks, mismatch remains
0/0 and disconnect retains 52/52 executed ticks with 49/49 confirmed. All 20
static-navigation JSONL files contain 51 rows and are byte-identical, stderr is
empty and both current probe fingerprints match the zero-excess report. The
reviewer's initial raw-byte digest comparison differed because the verifier
hashes normalized text; raw file equality and current fingerprints both pass.
No further source finding remains in this bounded checkpoint review.

## Limits and next work

Final crowd/movement/offline frame-interval p99 values are 19.693/22.198/17.272 ms,
all above 16.67 ms. Bridge p99 is 0.042/0.042/0.054 ms. Capture intervals are not
isolated CPU-frame measurements. Selection rings overlap and marker contrast,
art, animation and presentation quality remain unfinished. Automated input does
not establish human responsiveness or physical-network behavior.

Dense opposing traffic is a failed gate. Preserve the full canonical fixture
while introducing coordinated yielding or avoiding compression before units
become trapped. Dynamic construction, per-command movement/response, rendered
large-map play, reference-hardware budgets, economy/production/fog/full AI,
complete matches, production content and every shipping gate remain open.

# Independent production increment review

Reviewed 2026-09-27 America/Los_Angeles. This reviewer owns only this report
and `tools/verify_production_capture.py`, performs no Git or build operations,
and checked STOP absence and the primary run marker before editing.

**Verdict: accept the bounded offline production increment. Reject
complete-small-match, production multiplayer, shipping and AAA/SC2-quality
acceptance.**

## Source inspection

Inspected production implementation and its simulation integration, protocol
validation, hashing, headless replay/metrics, bridge command conversion and
snapshots, client input/HUD/fixture, and production unit tests. Authority remains
standalone integer C++, using the same canonical command/replay interfaces.
TrainStrider7 and CancelProduction8 interpret the single actor as a structure
ID and require zero coordinates. Receipt checks distinguish unit/structure
namespaces; application checks ownership, life, Foundry kind and completion.
The existing network setup explicitly rejects production commands and remains
a combat scenario, so no production multiplayer claim follows.

The paid queue is a count of identical Striders: each costs50 salvage and takes
100 ticks. Five queue slots and a player cap12 count living workers/Striders
plus reserved items across factories. Cancellation removes the tail and refunds
50; front progress survives unless the queue becomes empty. A separate lifetime
roster cap reserves IDs for purchases, and dead IDs are retained as tombstones.
Production progress, queue and blocked state enter the authoritative hash and
the content fingerprint includes production.cpp.

Spawning runs before the tick's movement solver, at one of eight deterministic
perimeter exits. Static navigation clearance and living-unit occupancy must
pass before a unit is appended. Completed blocked fronts retain both payment
and population reservation. Newly appended units enter the ordinary movement
spatial index as stationary blockers. This is a bounded exit policy: failure to
use an arbitrary geometric gap outside those eight points is deliberate and
does not establish general placement/crowd quality.

The headless diagnostics grow their unit arrays after births; replay feeds each
recorded command at its tick so later commands can name already-produced IDs.
Future commands for existing factories are compared at different arrival times
in the production tests. Test fixtures inject funds, blockers, dead units and a
second factory to isolate queue, cap and blocked-exit rules. Those tests are not
player-path evidence.

## Independent validator

The validator parses actual VFR3/VFC1 bytes and binds all accepted UI commands
to their logged tick, operands and actor ownership. It examines every snapshot,
trace entry and stable identity; conserves initial salvage across bank, cargo,
deposits, Foundry purchases and spawned/queued Striders; checks displayed
population against live/reserved counts; and verifies precise debit/refund and
front progress in the stopped-harvester production interval.

Static movement and pairwise relative sweeps use the independent rational
geometry oracle, including each new stationary spawn against that tick's moving
units. Births must consume completed queue fronts at the owning factory's
perimeter. A blocked front must have no available supported exit. Every client
hash must match one Debug and ten Release replay runs; final economic and
production fields must also match. Binary/report/replay hashes identify the
inputs. Deliberately corrupted reports test rejection; they are not isolated
proof of every validator branch.

## Outstanding acceptance gaps

The package fixture is designed to harvest200, build a Foundry, buy two units,
cancel/refund one, buy a replacement, reject an unaffordable purchase, produce
two Striders, select/move one and restart. It does not reach population12,
queue5 or block all exits through packaged player inputs; those rules have
source-test coverage only. No human play, continuous animation review,
responsiveness measurement, balance or representative hardware acceptance is
provided by this fixture.

Economic AI, anchor victory, flux, technology, fog/scouting and complete small
matches remain absent. Existing failed dense-stream, scale/performance,
latency and production-art/audio gates remain open. Shipping review and
COMPLETE.md remain inappropriate. The next gameplay step is a canonical-command
economic AI and attackable anchors with reliable win/defeat/restart evidence.

## Evidence personally inspected

- `artifacts/production-build-2026-09-27.log`: MSVC Debug and Release each pass
  9/9 CTest suites, including production. Existing malformed replay/output-alias
  checks pass, with2,000 matching cross-build tick hashes and ten Release repeats.
  These are compilation/rule/replay checks, not gameplay quality evidence.
- `artifacts/production-package-2026-09-27.json` and its stdout: the actual
  exported package passes27 software InputEvent checks at tick1795, final hash
  `73ccf6ab3b32fdae`, protocol7 and content `6b60e3504b2e7828`. Its9 recorded
  commands cover gathering, Stop, paid construction, three successful purchases,
  one cancellation/refund, an unaffordable purchase and movement of produced ID7.
  Selection and identical restart are fixture assertions. Source-probe evidence
  is retained separately and is not substituted for the package.
- `artifacts/production-replay-2026-09-27/summary.json`: all1,796 snapshots /
  10,912 unit rows pass the independent audit. It conserves4,000 salvage, checks
  100 construction and200 production progress increments, observes two births
  and zero blocked-spawn ticks, and rejects seven corrupted reports. Every one
  of1,795 packaged hashes matches Debug and ten Release replays, with identical
  final economy/production fields and recorded binary/report/replay SHA256s.
- Personally inspected all five actual package PNGs: `queued`, `refunded`,
  `purchase-rejected`, `produced` and `moved`. Queue2/5 and7% progress are visible;
  cancellation shows salvage50, queue1/5 and10%, so the displayed front progress
  survives. Purchase rejection is legible and produced population changes from
  3+2 queued to5+0. The moved image shows a selected Strider and destination ring.
  Images support visible states; the input/hash ledger supports execution.

The visual verdict remains a clear failure against the production target.
Foundry and anchor are nearly identical plain cube silhouettes; workers reuse
smaller Striders; deposits/cargo are bright blocks and terrain is a flat gridded
arena bordered by repeated blocks. Health bars cross deposit/anchor names, and
newly produced units crowd the Foundry label. Costs, queue/progress, population,
hotkeys and rejection feedback are readable in the1920x1080 package, but the
interaction remains text/hotkey driven without production action buttons or
unit queue portraits. This run changes no authored art, construction/production
animation, audio or VFX. No new side-by-side SC2 reference comparison or human
playtest was performed, and screenshots cannot establish animation smoothness
or responsiveness.

`production-package-2026-09-27-host.json` reports302,645,248 resident bytes from
once-per-second process sampling over91.67seconds. That is a development-host
observation, not a frame-time/simulation-time/response/reference-hardware pass.
New transport/crowd regression summaries preserve prior scenarios only; they
cannot establish production multiplayer, full-match AI or the failed dense
Scale128 traffic requirement. The source includes a hard lifetime4096 roster
cap, which must remain visible as a bounded implementation limitation when
future long-match/soak behavior is designed.

## Follow-up packaged movie evidence

Personally inspected `artifacts/production-movie-contact-2026-09-27.png`, an
eight-frame contact sheet from the successful final packaged movie. Samples
show worker resource trips, Foundry construction, the selected ready factory's
paid queue, increasing front progress and a produced Strider beside the factory.
This is sampled-state inspection, not continuous video playback; the sparse
frames do not independently capture every cancellation, second birth or move.
The separate interaction report and replay supply those execution checks.

`artifacts/production-movie-final-2026-09-27.json` passes27 checks at tick1791,
hash `c07e55a5e7d8a4c1`. Its separately recorded commands and all1,791 hashes
match Debug plus ten Release runs in
`artifacts/production-movie-replay-2026-09-27/summary.json`:10,886 audited unit
rows,4,000 conserved salvage, three purchases, one refund, two births and
100/200 construction/production increments. No blocked spawn occurs.

Read `production-movie-metadata-2026-09-27.json`: the final1600x900 recording
has5,377 frames at60fps,89.616667seconds. AVI and converted MP4 are retained as
`production-movie-final-2026-09-27.avi` and `.mp4`. Host sampling reports
275.3946924seconds wall time and309,153,792 peak resident bytes. Encoding slowed
the capture; these timings are excluded from performance acceptance. The first
concurrent movie was stopped by the180second capture-helper watchdog and its
artifacts remain separately retained. The primary reports increasing only the
movie capture watchdog according to requested recording duration; no gameplay
speed or SPEC performance budget changed.

The contact sheet retains the same blockout silhouettes, bright resource/cargo
cubes and label/healthbar congestion. It does not add human input, animation
deformation/smoothness, audio, frame-pacing, SC2 comparison or shipping approval.
The bounded increment verdict above is unchanged.

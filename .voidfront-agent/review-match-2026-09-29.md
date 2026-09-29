# Independent economic AI and anchor-combat review

Review date: 2026-09-29 America/Los_Angeles. Reviewer owns only this report,
performs no Git/build operations and does not author the implementation. STOP
was absent; the primary agent confirmed exclusive run ownership.

## Scope and current verdict

Final bounded verdict and actual evidence are recorded below. Baseline was the prior paid production increment. Full
small-match, AAA/SC2 quality and shipping acceptance remain rejected: contested
flux, technology, fog/scouting, setup, full content, human gameplay, crowd
progression and representative performance gates are unmet.

## Required inspection for this increment

- Economic AI must gather/build/train/attack using recorded canonical commands;
  a fixture that directly inserts resources or attackers cannot establish this.
- Unit/structure target namespaces, persistent pursuit, queued order completion,
  queue cancellation/caps, serialization and hash inclusion need rule coverage.
- Structure destruction must clear paid queues and reserved population under an
  explicit refund/loss rule and invalidate static navigation. Simultaneous anchor
  deaths must have deterministic outcome semantics, including a possible draw.
- Inspect actual package snapshots/captures for destroyed geometry, visible
  winner/defeat, post-match command handling and restart parity. Scripted inputs
  and screenshots do not prove human responsiveness, balance or animation.
- Preserve Debug/Release and cross-build recorded hash evidence. Combat loss
  invalidates the prior production validator's all-units-live assumption and
  requires accounting for lost paid queues; this needs independent treatment.
- Economy remains offline unless its canonical commands and births are actually
  transported in separate clients; prior combat transport tests do not prove it.

Source baseline inspected: simulation command application, movement/combat,
economy/production, production rule tests and capture validator, client input,
prior independent production report and project acceptance/roadmap documents.

## Findings sent during implementation

The bounded scope deliberately excludes queued unit commands and explicit focus
or moving-target pursuit. Enemy-building right click emits ordinary AttackMove
at the building center, retaining proximity acquisition and enemy-unit priority.
The roadmap must continue to list queue/focus-target capability as incomplete.

Source review caught a presentation integration defect: building attacks leave
the unit-only target ID zero, while the existing renderer requires that ID for
facing, attack animation and beam feedback. The primary/simulation/client agents
were asked to add a separate structure target snapshot and visible attack path.
Resolution and package evidence remain pending.

Source review also found that the economic replay command cap initially stopped
AI and human commands. The primary changed this: commands continue after the
cap; replay_complete becomes false and replay_bytes returns empty. Reviewed the
corrected bridge source, including canonical tick/player/sequence replay sorting.
This is source review, not an executed 100,000-command stress test.

## Updated source review

Reviewed standalone simulation, match rule tests, protocol8 change, bridge,
headless profile, packaged fixture and independent match-audit implementation.
The AI reads integer snapshots and emits Gather/Build/TrainStrider/AttackMove
commands on fixed 20-tick decisions. It finds a bounded legal anchor-linked
Foundry site, resumes unfinished construction, returns idle workers to mining,
trains within salvage/population/queue limits and sends produced Striders toward
the enemy anchor. It has no flux/tech/scouting or strategic adaptation and cannot
replace killed workers. This is a basic economic opponent, not complete RTS AI.

Building combat uses distance to an axis-aligned footprint, unit target priority,
stable structure-ID ties and simultaneous end-of-tick damage. Move suppresses
fire; Stop/Hold can shoot in range without chasing. Production happens before
combat, so a front completing on a building's death tick may already have spawned;
remaining paid work is forfeited without refund. Tombstones retain IDs, released
reservations and rebuilt navigation. Living command anchors define victory/draw.
Terminal ticks advance and consume buffered input sequences without gameplay
mutation; the packaged bridge rejects new human commands after the outcome.

The earlier attack feedback defect is corrected in source with separately hashed
`target_structure_id`, exported as `target_structure`; presentation uses its
position for facing, attack animation and a beam. Source review does not prove
the actual rendered effect. No new target command or queued unit order is added.

The match audit now retains structures alive before the tick as collision blockers
through their destruction tick and adds newly purchased structures. Its independent
checks cover canonical input ownership, resource conservation including tombstones
and forfeited queues, population, speed/static/relative sweeps, anchor outcome,
terminal freeze and a conservative in-range/cooldown structure-damage upper bound.
It is not a complete independent combat-priority simulator. Cross-configuration
replay hashes validate determinism, not rule correctness by themselves.

The proposed packaged fixture gathers/builds/trains through software InputEvents,
issues enemy-anchor AttackMove and deliberately stops reinforcing to reach defeat,
then restarts. It advances eight ticks per rendered frame. It is package input
integration and reachability evidence, not ordinary-speed play, player victory,
manual input latency, animation smoothness, performance or complete-match proof.

## Executed build evidence inspected so far

`artifacts/match-build-2026-09-29.log` records MSVC Debug and Release builds,
10/10 CTest suites passing in each configuration, separate record/replay traces,
15 malformed replay rejections, output-alias rejection checks and 2,000 matching
cross-build hashes plus ten repeated playback matches. `LastTest.log` records
unmodified-start AI versus passive outcome at tick2304 (winner1, hash
4588601587254292155) and AI duel at tick6348 (winner0, hash13969595948391887881).
These headless outcomes are deterministic small economic scenarios, not human
matches, ordinary-speed client evidence, balancing or full RTS completion.

Final match rule source additionally asserts distinct hashed structure targets,
Move/terminal target clearing and a lethal production-completion boundary. The
last fixture explicitly separates the fresh spawn from attacker acquisition so
unit priority does not redirect the shot; it proves the documented order of
production then combat, not a player-built combat situation.

## Accelerated packaged evidence personally inspected

`artifacts/match-package-2026-09-29.json` passes27 checks at tick2856,
final hash`0c492e6de53740e4`, with2,857 snapshots and193 structure-target
beam/attack-clip requests. Five actual human software inputs gather, stop,
build, train and right-click the enemy anchor. The canonical replay contains
four enemy Gather commands, one Build, five TrainStrider and five AttackMove
commands. The opponent pays for five units; it does not receive injected funds
or attackers. Player Foundry and anchor are destroyed. Enemy Foundry ends452HP,
showing that AttackMove can attack an intervening building rather than lock the
clicked anchor. This behavior is consistent with the stated proximity semantics.

Personally inspected all four actual1920x1080 PNGs: `opposing-production`,
`anchor-under-attack`, `defeat`, `restarted`. Both Foundries and the produced
player unit appear in production; the attack image shows beams toward the
976HP player anchor; defeat has a readable explicit outcome/restart overlay and
absent player anchor; restart restores1000HP anchors and three player workers,
with enemy workers gathering again. These images verify visible states and
sampled beam feedback; they do not show continuous motion or every attack.

`artifacts/match-replay-2026-09-29/summary.json` audits18,573 living-at-tick-start
unit rows,194 building damage observations, destroyed structures1/4 and all4,000
salvage. No paid queue is forfeited in this packaged route; destruction-with-paid-
queue remains rule-test coverage. All2,856 recorded hashes match Debug and ten
Release replays. Eight corrupted reports reject, including unfunded damage and
false shot target. Rejection mutations provide evidence for these chosen cases,
not proof of every validator branch. Report/replay/replayer fingerprints are
retained in the summary.

Art remains a clear failure against the target. Anchors/Foundries are simple
cubes, workers reuse scaled Striders, yellow cargo/deposits are bright blocks,
and terrain is a flat grid with repetitive walls. White health bars overlap
structure/deposit text. No authored construction/destruction effect, production
art/audio, side-by-side SC2 reference comparison or human play is established.
No static image supports animation quality, response latency or frame pacing.

## Ordinary-speed package and regression evidence

`artifacts/match-realtime-2026-09-29.json` uses the ordinary20Hz accumulator
with the same software InputEvent route, passes27 checks at tick2849/hash
`15ed20dd06504f87`, retains2,850 snapshots and187 structure beam/attack requests.
Personally inspected all four corresponding PNGs: both sides produce; a visible
beam strikes the992HP player anchor; defeat removes that anchor with the explicit
restart overlay; restart restores workers/anchors and enemy mining. This extends
the package integration evidence to ordinary-speed execution. It is still an
automated losing route, not human play or a measured responsiveness/animation pass.

`artifacts/match-realtime-replay-2026-09-29/summary.json` verifies all2,849 hashes
against Debug and ten Release replays,18,393 living-at-start unit rows,188 damage
observations,4,000 conserved salvage, zero paid-queue forfeiture and eight
corrupted-report rejections. Different input ticks explain the different outcome
trace from the accelerated run; each recording independently reproduces exactly.
Host sampling reports356,945,920 peak resident bytes over148.9944849seconds. This
is development-host process observation, not representative performance proof.

Personally inspected the22-case headless network summary and six-case packaged
network summary; protocol/replay preservation passes, including fault cases.
These remain combat skirmishes, not transported economy, production or anchor
victory. Headless80ms generated-command p95=109.9491/109.6742ms, but pace is
19.9344/19.9283Hz;160ms with two ticks yields about17.75Hz. Runs are concurrent,
not isolated. Packaged80ms event-to-execution p95=150.335/134.243ms: player0
exceeds the unchanged150ms budget. Samples are only three inputs per peer/case.
The400-tick combat smoke passes with winner1, but frame p99=17.473ms still exceeds
16.67ms. No latency/pacing/frame or reference-hardware acceptance is added.

The preserved production audit passes1,799 ticks,10,938 unit rows, three purchases,
one refund, two births and Debug plus ten Release playback runs. It observes no
blocked spawn. These are preservation checks for their prior bounded scenarios.

## Final verdict and next evidence

**Accept the bounded offline economic-AI and attackable-anchor increment. Reject
complete-small-match, economic multiplayer, performance, AAA/SC2-quality and
shipping acceptance.** No unresolved blocking implementation defect was found
in the inspected increment. The concrete missing building-shot presentation was
fixed and actual screenshots now show its beams. The damage/conservation audit
was strengthened after independent review; its remaining scope limits above stay
explicit. No new movie, continuous motion assessment or SC2 comparison was made.

Next: produce a packaged player-victory route from ordinary resources through
reinforcement and enemy-anchor destruction, including restart and exact replays.
Then deliver contested flux, meaningful technology, fog/scouting and setup with
AI participation, while finishing queued orders and explicit target semantics.
The original content/art/audio, human play/balance, dense-crowd, latency, scale,
reference hardware,30-minute network and independent shipping gates all remain.
Do not create COMPLETE.md or stop the continuing automation on this evidence.



## Follow-up movie addendum

Personally inspected `artifacts/match-movie-contact-2026-09-29.png`, the twelve-
sample contact sheet from the retained movie. Samples show worker mining trips,
Foundry construction and queue progress, produced units crossing the arena,
unit/building combat and attackers reaching the player anchor. The sheet does
not itself show the final defeat/restart sequence; those states are supported
by the separately inspected package PNGs and recorded state. This was sampled-
frame inspection only, not continuous video playback or normal-motion review.

`match-movie-media-2026-09-29.json` records1600x900,371frames at60fps and6.183333
seconds for `artifacts/match-movie-2026-09-29.mp4`; the original AVI is retained.
The fixture advances eight simulation ticks per rendered frame, compressing the
2,856-tick gameplay route into this short video. Playback cannot establish
ordinary movement speed, temporal smoothness, responsiveness or frame pacing.

The movie report passes27 checks. Personally read
`artifacts/match-movie-replay-2026-09-29/summary.json`: final hash
`0c492e6de53740e4` and replay SHA256 match the earlier accelerated route; all
2,856 hashes again agree with Debug and ten Release runs. It retains18,573 audited
unit rows,194 damage observations,193 structure attack requests,4,000 conserved
salvage, zero forfeited paid queues and eight corrupted-report rejections.
Host sampling reports412,934,144 peak resident bytes over21.8805308seconds while
encoding. These are capture observations, excluded from performance acceptance.
The bounded verdict and all outstanding quality/gameplay gates remain unchanged.

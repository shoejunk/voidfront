# Independent player-victory and role-selection review

Review date: 2026-09-29 America/Los_Angeles. Reviewer owns only this report;
primary owns integration and Git. STOP was absent; active run marker identifies
the primary task. Inspected SPEC, PROGRESS, TESTING, roadmap, prior match review,
current source differences, actual source-run and final packaged PNGs, and
executed audit evidence. Final bounded verdict appears below.

## Concrete defect and bounded repair

Prior F2 selected workers and Striders together. An army AttackMove then cleared
workers' resource/construction state through ordinary simulation command
application. Prior package PNG advertised "F2 all units"; this was a control
design trap rather than an undocumented difference. F1 now selects only living
owned workers; F2 selects only living owned Striders. Both clear pending attack/
build targeting and entity selection. Generic combat mode retains its Strider
selection behavior. Economy/production/defeat fixtures now use F1 for workers.
PLAY and economy HUD explain the split, with group/camera help restored.

No simulation, bridge, content identity or gameplay constants are modified in
the inspected implementation. The victory fixture extends the existing capture
fixture, issues software keyboard/mouse InputEvents, observes snapshots for
decisions and never injects resources, units, damage or authoritative state.
It sends one worker to build while others mine, resumes that worker, purchases
reinforcements, defends, then selects at least five Striders and counterattacks.
This is a scripted player route, not a human strategy/balance assessment.

## Source-run evidence personally inspected

`artifacts/victory-source-final-2026-09-29.json` reports 29 passing checks,
6,408 ticks, winner0, hash `cfad785ac934e8e0`, seven role-selection events,
33 input attempts and 213 structure attack/beam requests. The first army
selection is empty; worker selections contain IDs1/2/3; the later F2 contains
five live combat IDs. Both build and attack pending modes are exercised.

Personally viewed source-final counterattack, victory and restarted PNGs.
Counterattack shows `00 WORKERS / 05 STRIDERS` while three workers remain by
the base/deposit. Victory has an explicit readable enemy-anchor-destroyed
overlay and absent opposing anchor; restart shows both1000HP anchors, three
own workers and resumed opposing mining. Static images support those visible
states, not continuous animation, manual input response or frame stability.

Review found the new fourth hint line overflows the old bottom-panel layout:
the order notice is below the panel and against the1080px viewport edge.
Reported to primary for a layout repair and fresh packaged screenshot review.
Resolution: primary raised economy selection/help32px and expanded only that
panel32px upward. Personally viewed the final-package ordinary-speed
`victory-realtime-2026-09-29-opposing-production.png`: all four help/notice rows
are readable inside the panel with a bottom margin. The demonstrated clipping
defect is resolved at this1920x1080 capture size; no responsive-layout matrix
or accessibility acceptance is implied.

## Audit source inspection

The shared audit now requires the route-specific winner and destroyed opposing
anchor. Both players' canonical replay commands bind to input/ownership, with
Gather/Build/Train/AttackMove evidence. Existing resource/population, swept
collision, conservative building-shot budget, terminal-state and restart
checks are preserved. The audit is not a complete independent combat-priority
or simulation implementation.

Victory adds seven role events bound to actual retained tick snapshots, exact
living owned role membership, empty starting army, pending-mode cancellation,
and binding between last F2 membership and the recorded army AttackMove. The
immediately following tick must retain every previously gathering worker's
Gather order/resource ID and exclude those workers from the army command.
Human-input binding occurs before these role checks; negative cases mutate
role selection, retained build mode and route in addition to the earlier eight
report corruptions. Personally inspected the source-final audit log: all6,408
hashes match Debug plus ten Release replays; 51,847 living-at-start unit rows,
145 building damage observations and4,000 conserved salvage pass; destroyed
structures are2/3, zero paid-queue forfeiture. All eleven chosen corruptions
reject. Separately executed in-memory role-audit adversarial checks: the real
seven events pass; wrong live role, forged bound snapshot, missing pending
attack evidence, retained attack mode and missing army input all reject.
These selected corruptions do not prove universal
validator correctness or independent physical input capture.

## Executed package and preservation evidence

Personally read `artifacts/victory-build-2026-09-29.log`: pinned4.7.2 verified,
MSVC Debug and Release each pass10/10 suites; existing malformed/alias and
cross-build/repeated replay checks pass. These establish compilation/test
preservation, not gameplay quality.

`artifacts/victory-package-replay-2026-09-29/summary.json` independently records
the preliminary packaged accelerated route with the same6,408 ticks/hash,
51,847 unit rows,145 damage observations, seven role events and213 structure
attack requests as the source run. Debug plus ten Release playbacks and all
eleven report-corruption cases pass. That package precedes the HUD-only repair;
the rebuilt package's ordinary-speed route is recorded below.

Read `artifacts/victory-client-network-2026-09-29/summary.json`: two rendered
clients with distinct PIDs19592/40428 complete240 ticks, matching trace hashes,
three inputs per peer and ten repeat playbacks. This clean0ms impairment combat
scenario is a control-path preservation check. It does not add economic
multiplayer, representative latency/performance or30-minute match evidence.

Final rebuilt package: `artifacts/victory-realtime-2026-09-29.json` passes29
checks using the ordinary20Hz accumulator,6,556 ticks, winner0 and hash
`c0ba2300ef0084ab`. It retains seven role events,36 input attempts and192
structure attack requests. Personally inspected all four actual1920x1080
opposing-production/counterattack/victory/restarted PNGs. Counterattack shows
zero workers/five Striders selected while workers remain near the home economy;
victory shows the explicit destroyed-enemy-anchor outcome and absent opposing
anchor; restart restores both anchors and three own workers. Help/notice rows
fit the repaired panel. Individual screenshot samples do not prove continuous
motion, animation quality or response timing.

Personally read `artifacts/victory-realtime-replay-2026-09-29/summary.json`:
all6,556 hashes match Debug plus ten Release playbacks;51,674 unit rows,
148 damage observations,4,000 conserved salvage, zero paid-queue forfeiture,
seven role events and all eleven negative reports pass. Its canonical input
schedule differs from the accelerated route; each recorded stream reproduces
exactly, rather than implying both live schedules should share one hash.
Host report records335.9084067seconds and1,079,848,960 peak resident bytes.
This fixture retains every full snapshot in memory and ran alongside a short
network regression; those process observations are not ordinary gameplay-memory
or representative frame/performance acceptance.

## Final bounded verdict

Accept the worker/army selection repair and the scripted packaged player-victory
plus restart evidence, including ordinary-speed execution and exact replays.
The concrete UI clipping regression found during review was repaired and
visually rechecked. No unresolved blocking implementation defect was found in
this bounded increment. This is not completion of the small-match milestone.
Next player capability is contested flux and meaningful technology with AI
participation, followed by fog/scouting and setup; preserve the missing queued
orders/exclusive focus targeting and all prior scale/quality failures.

## Acceptance limits

No full-small-match, economic-multiplayer, performance, AAA/SC2 or shipping
approval. Contested flux, technology, fog/scouting, setup, queued unit orders,
exclusive focus targeting, complete content, human play/balance, crowd progress,
response/frame budgets and full shipping review remain open. Buildings are
blockout cubes; workers reuse the combat model; health/text overlaps and crowded
combat effects remain visible. No new SC2 comparison or continuous video review.
Do not create COMPLETE.md on this evidence.

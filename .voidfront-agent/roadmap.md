# Continuing roadmap

## 2026-11-26 — a seven-tick move waits for a search slot

Use the ordinary seed-42 Scale128 recordings
`.voidfront-agent/replays/2026-11-26-scale500-gap-{move,bypass}.vfr`
with simulation content `936b292c1236b4ff`. The starting roster contains
250 units per side. Three units use canonical orders to stage two held
posts and walker 258. At 1,000 it receives one Move from (29584,10224)
to (29440,10240). All 499 other units stay still after launch. The walker
reaches its slot at 4,820, taking 191 seconds of game time. The same setup
with a short alignment and queued original goal arrives at 1,006.

Instrumented copies isolate the scheduling cause. With a single mover,
the local search is needed at relative ticks 1, 32, 63, 94 and so on.
The eight-ID budget first admits 258 at 64–65, then 188–189. The first
123 requests miss their slot. The 124th request qualifies at 3,814 and
the walker arrives at 3,820. Granting just its first request recovers the
isolated seven-tick trip. The nearby-unit query at each request contained
two or three stationary neighbors. The recorded normal game and diagnostic
fixtures are in `artifacts/holders-review-2026-11-26/final`.

Next, spend deterministic search work on units that actually need it.
Measure time from request to service for one and many requesters. Verify
progress under sustained contention while bounding expensive queries per
tick. Preserve Hold, relative swept clearance, exact destinations, the
two-spawn seven-tick approach, the 1,214 flux trips and 1,440 formations.
Rerun the 200/500 crossings under load. The three-moving-walker deadlock in
the 1,2,0,4,3,5 formation remains a separate case once a search can run.

The selected worker now reads at both rear anchor positions. The next
visual checks are the intersecting bars on the two held workers and the
enemy anchor nameplate at zoomed viewport edges. Human play, physical
network soak, reference-hardware performance and production content remain open.

## 2026-11-25 — passing between two held workers and the flux field

Use the two November 25 `two-holders` recordings in
`.voidfront-agent/replays` with simulation content `a2147866b44eca01`.
Both begin from ordinary seed-42 Glass Reach. Two friendly workers hold
(7488,5888) and (7616,5760). At 442 the middle worker receives Move
from its anchor to (14464,10144). At 662 it stops at (7359,5868) and
it is still there at 2,942. Its assigned destination remains valid.
The normal-start failure also rotates to the other spawn.

First instrument the terrain and unit rejections at the first failed
step and the chosen local detour. With both blockers immobile, a Move
via (7168,5120) and a queued final leg arrives at 928. From the already
stuck state a single repeat of the original Move at 912 arrives at
1,182. Require automatic exact arrival, preserved swept clearance and
unchanged Hold orders. Recheck the 1,214 two-holder placements. Currently
1,036 arrive and 178 stall. Unconditionally replanning a failed terrain
leg even with a unit blocker rescued only four more cases in a private
experiment. The ordinary reproduction and controls are in
`artifacts/tangent-review-2026-11-25`.

Keep the 2,888 solo routes, 1,152 one-neighbor trips, mirrored paid
mining matches and queued/Stop/Hold behavior as controls. The six-unit
reversed destinations still finish five of six, and the 200/500-unit
crossings still need their own crowd solution. The small-map result
does not close the representative battle or input-response gates.

For the next presentation pass, use the November 25 paid image where
the selected miner returns behind the northwest anchor. Compare each
anchor service position from both spawns. Preserve the corrected resource
labels and cargo visibility at mining positions. The 500-unit 1080p
opening also needs a separate performance investigation before large
battles can be assessed.

## 2026-11-24 — a miner stops beside the central flux field

Start with the hidden-departure recording in `.voidfront-agent/replays`.
After the survivor withdraws safely, the opponent moves its guard in
fog. The mining team later sees the post empty. Gather at 92,460 sends
the survivor back out, but it stalls one fixed-point unit from the central
flux obstacle. On the northwest side it sits at (7710,6079) with goal
(14464,10144). Its blocked-tick counter stays at zero. By 103,339 it
has spent 10,646 ticks idle. The southeast case rotates the position
to (8674,6209). The recruit continues earning income in both games.

Reproduce the stall in both stock builds. Inspect the route, current
waypoint, movement proposal, collision rejection and replan trigger
at the first missed step. In the recorded control, repeating the same
canonical Gather just once at 92,800 made the original survivor mine at
93,061 and deliver at 93,636 on both sides. A short intermediate Move
also restored income. Fix the movement or route lifecycle indicated by
that evidence. Preserve the cargo, destination, collision clearance,
guarded withdrawal and mirrored paid openings. The six-unit reversed
goals and the 500-unit stress case remain separate acceptance tests.

A second field trial isolates a moving defender at offset (-3,+6). An
unnecessary retreat delays payment from 1,061 to 2,188 when the defender
leaves. Continuing to mine becomes lethal if it turns back. An ordinary
sidestep at 471 let both miners bank ten at 1,080 on departure and survive
reversals at 472, 473 and 477. See `artifacts/unsafe-review-2026-11-24`.

The short movement reproduction is
`artifacts/unsafe-tests-2026-11-24/final/isolated_stall.cpp`. With one
living worker, no opponent units and no AI, recall reaches home at 470.
Immediate Gather stalls at the same tangent at 703, earning nothing by
5,001. Repeat Gather at 803 and the worker mines at 1,064 and delivers
at 1,639 in both orientations. Use this case to instrument the earliest
failed step before modifying movement.

The straight segment to the cached waypoint is clear at 633 at
(5820,4916). After a fixed-point step of (27,17) it is obstructed at
634. From the eventual stopped position the navigation query returns
corner (7744,6080), then the original goal. The rotated corner is
(8640,6208). See `route_geometry.log` beside the short reproduction.

## 2026-11-23 — an exposed mining position

The first and third starting workers can repeatedly bank salvage past
the offset (-1,-6) Strider on both sides of Glass Reach. Their assigned
mining positions lie on opposite sides of the deposit. The middle worker
uses the south slot in its own orientation, inside the guard's acquisition
radius. With that slot open, the final policy still loses it at 1,032
or 1,031. If another worker occupies that slot, the miner can instead
use a safer side and repeatedly deliver. Across 18 nearby
pursuit positions, six remain lethal per spawn.

Next, use the middle worker as the sole survivor. Reproduce an ordinary
ReturnCargo on first sight at 384, with safe arrival at its anchor at 788.
Handle an unsafe destination without diverting the other two workers
from their safe slots. Verify a stationary guard, departure out of sight,
and newly visible clearance. Measure first confirmed safe re-entry and
banking, as well as losses, repeated empty orders, and worker purchases.
The independent supervised run banked ten at 2,370 after the guard left.
Preserve the paid openings, partial final loads and the 18-position matrix.

The command card identifies the weakest worker in a damaged group. At
the south-east patch the minimap can still cover part of its selection
ring. Human selection/response testing, dense movement, physical-network
soak, reference performance and production content remain open.

## 2026-11-22 — remaining approach to the alternate salvage field

On the six-tile approach, both miners now see the guard soon enough to
go around it, return loaded and repeat the trip. The Windows bridge gives
the simulation an opportunity to issue the order on that exact tick.
The dated TESTING entry covers the paid openings, finite ore and cargo display.

Next, reproduce the independently measured guard one tile to the side,
at offset (-1,-6) from the alternate deposit in the mining player's
orientation. The current AI loses its worker at 935 with no delivery.
The reviewer used ordinary commands through offset (-5,+2) to bank ten
at 1,115 with full HP from both spawns. Use the observed guard and the
worker's actual service position to find a legal path. Verify repeated
loaded returns, partial loads and both orientations. Re-run the 18 guard
positions and the ordinary purchased-defender games. The review and
recorded commands are in `artifacts/interceptor-review-2026-11-22`.

The human 1vAI/1v1 playtest, crowded movement, physical-network soak,
reference-hardware budgets and production content remain open.

## 2026-11-21 — defended mining and moving interceptors

The economic AI preserves its workers against one, four and eight Hold
guards at the alternate deposit. The controlled withdrawal and escort
cases cover unseen departure, visible clearance, full and partial
deliveries. Read the dated TESTING entry before extending the policy.

Next, reproduce the two mirrored moving interceptions from
`artifacts/defended-native-2026-11-21/approaching-mirrored.log`. A lone
Strider six tiles toward the approach is harmless on Hold and lethal
when its AttackMove intercepts the scout. Measure changes in visible
relative position, when the worker can first retreat, route clearance,
damage and eventual delivery. Use observations the mining team can
make. Preserve the six-tile far-side Strider/Lancer distinction, the
six fixed-garrison openings, and the worker's loaded return. A
larger fixed exclusion radius previously suppressed a safe delivery.

The six-unit reversed-goal stall and 500-unit crowd-progress gate,
human 1v1 and 1vAI play, physical-network soak, reference performance
and production art/content remain on the roadmap.

## 2026-11-20 — bounded camera and access to covered ground

Home, minimap navigation and zoom now keep the ground viewport within the
map. Holding Space makes unit selection and orders available beneath the
command card and minimap. The Windows export retains both players' Home
views and the four-edge and two-corner input recordings in the dated
TESTING entry.

Next, measure the economic AI's trip to the alternate salvage field with
an enemy defender present. Start with ordinary home depletion and compare
the two spawn orientations. Record sight of the defender, worker losses,
repeat purchases, military availability and salvage actually delivered.
Use those observations to choose between waiting, sending an available
escort and abandoning the trip. Verify the chosen policy through canonical
commands and native replay before changing its spending rules.

For the next presentation pass, observe human Home, zoom and corner
orders at 720p. Check whether players discover Space, how much playable
ground the upper status strip obscures, and whether they can identify
which unit owns each health bar when workers cluster near an anchor.
Camera geometry and scripted clicks cover only part of that evaluation.

## 2026-11-19 — scouting after home salvage runs out

The economic AI now checks the opposing start, remembers exhaustion that
its team has actually seen, and budgets its last 50 for a worker or a
Strider. The dated TESTING entry covers both orientations, changes in sight
during a trip and a reproducible opening with all 2,000 home salvage mined.

Next, fix camera framing at the map edges. Reproduce the empty space at
both spawns using the ordinary home, minimap and zoom controls. Constrain
the ground footprint of the visible viewport at each supported zoom, then
verify selection and orders near all four edges in a fresh package. Inspect
the overlap between health bars and resource labels at the opposing deposit.

For the next AI increment, test the alternate site against a defending army.
Measure worker losses, escort timing and how often the AI repurchases a lone
scout before changing that policy. Human matches, physical-network soak,
reference-hardware budgets and the production content gates remain open.

## 2026-11-18 — income after a raid

The economic AI now restores its opening workforce with paid anchor orders.
The packaged counterattack killed a worker, and the AI's replacement mined
and delivered salvage before the next assault. See the dated TESTING entry.

Next, play out the same decision when the home deposit is exhausted.
Measure what the AI actually knows about alternate salvage through fog,
how it uses its last 50 salvage, and whether the surviving or recruited
worker can reach a fresh deposit. Exercise mirrored positions, depleted
and reachable sites, and a map with no salvage left before choosing the
spending or scouting change. Recheck the ordinary offline match and replay.

Two-human lobby, rematch and return-to-offline play still need direct
observation. At the map edge, the current counterattack capture also shows
substantial space outside the battlefield. Check camera framing at both
spawns and zoom limits when returning to presentation work.

## 2026-11-17 evening — packaged match and lobby checkpoint

Two clients now exercise the exact TCP disconnect while UDP still has a
checksum outstanding, save both confirmed archives and display the next
setup action. The full contested input fixture now buys, refunds and
researches through the visible Foundry card. See the dated TESTING entry.

Next automated gameplay increment: use the newly available TrainWorker
anchor command for funded AI worker replacement after a raid. The current
economic policy counts surviving workers but issues no TrainWorker order.
Exercise both players with zero or one surviving worker, a paid queue and
insufficient funds. Check that reservations prevent repeated purchases,
that the recruit resumes gathering, and that native replays agree. Recheck
the complete packaged economic-AI match after any policy change.

The human gate still needs two players to run the lobby, contest a match,
rematch with changed settings and return offline. Record input and camera
feedback, mouse/keyboard accessibility of terminal actions, and visibility
of the resource strip and waypoints over crowded units. Reference-hardware
performance, physical-network play/soak and all production gates remain open.

## 2026-11-15 current checkpoint - negotiated loopback rematch

The setup screen now supports a persistent two-window loopback lobby. Host
offers settings, guest explicitly accepts, and a fresh native session starts
after each agreement. Both clients archive confirmed replays before allowing
another match. Outcome text wraps within a centered panel. The native sim and
UDP protocol/content remain unchanged.

Next exercise full matches with human input and retain recordings, including
a player-driven rematch and return to offline play. During that pass, add a
targeted integrated TCP disconnect while a peer is still confirming its native
terminal tick. Move result graphics above unit overlays, hide action hints
during setup, and give the guest's rematch settings more space. Keep previous
AI recovery, finite resource strategy, pathfinding, large battle performance,
finished content/art/audio, LAN/Internet impairment and shipping gates open.

## 2026-11-08 current checkpoint - fresh-session transport isolation

Foreign-identity full transport envelopes are now ignored without readiness,
ACK, simulation or progress-timeout effects. Current-identity compatibility
errors remain terminal. Native Debug/Release, actual same-port successive peer
pairs with all7 authentic old wire kinds,22case short impairment/fault suite and
two-session Godot bridge contract pass; see TESTING and independent session review.

Next implement coordinated setup/rematch negotiation and player flow: agree a
fresh identity and all settings before readiness, allow cancel/disconnect, retain
the first terminal recording, then complete another shared packaged game. Current
tests externally supply identity/settings; they do not implement that negotiation.
Fresh terminal capture review also found player1's defeat banner shifted right,
clipping its offline-return text. Fix and recapture this alongside the existing
dense queue/label/HUD readability debt when implementing the rematch player flow.
Keep every prior human/content/art/audio/performance/pathfinding/SPEC gate open.

## 2026-11-01 current checkpoint - contested queues and offline return

Fresh packaged pair confirms first winner7277; native22 full replays/119 prefixes,
44 commands/82 phases/35 corruptions pass. Both armies fight, both queued-shot
witnesses target opposing combat units, and the same surviving P0 Lancer resumes
its original pending anchor assault. Both former player roles return via R to
offline player0, F1 selects workers and RMB moves them; ten separate native offline
audit runs pass. See current PROGRESS/TESTING and independent contested review.
This adds verification/lifecycle hooks; existing authoritative rules are unchanged.
Initial worker-target witness was rejected and tightened before a fresh passing run.

Next implement coordinated network setup/rematch: peers agree fresh session
identity/settings before readiness, preserve terminal evidence, reject stale traffic
and verify another shared game plus disconnect/cancel behavior. Then direct full
1vAI/1v1 match play with recordings and concrete usability findings. Current6vs2
script is contested combat proof, not competitive balance or human acceptance.
Improve cramped role/waypoint HUD, labels/rings and dense queued formation overlays.
Keep terminal-fault/tail-prefix/transport-identity, AI recovery/finite-resource and
stationary-blocker/chase/service work on the roadmap. All dense-crowd/frame/response,
finished faction/map/content/art/animation/audio/VFX, physical-network/soak, human,
balance and SPEC shipping gates remain open. No COMPLETE.md.

## 2026-10-25 current checkpoint - last-worker recovery

Source `e721d48` fixes the demonstrated sole-surviving-flux-worker income failure:
retain one salvage worker and deliver mixed cargo through canonical Gather.
Native mirrored casualty test fails before/passes after, preserving paid queues,
banks, rotation and replay parity. Fixed test counter overflow and modernized
match verifier for Research/Lancer/queues/plating/typed resources. Fresh pinned
Debug/Release12/12 and packaged exploration/victory/restart audits pass; see
current PROGRESS/TESTING and independent recovery review for exact evidence.

Fresh real paired clients confirm first winner7147; native audit22replays/106prefixes/70phases/28captures/24corruptions passes. Defender army holds fullHP; contested combat and rematch remain open.

The Oct20 statement that AI cannot rebuild at22salvage is superseded: rebuilt
source metrics show22->32 at16000->16020 and natural duel tests require actual
production after prior full army wipe. Symmetric unresolved cap is not a
permanent recovery failure, balance result or full-match acceptance. General
chase/service-point iteration dependence and stationary-blocker routing remain.

Next implement coordinated match setup/rematch and verify successful-pair return
to offline with accepted movement; add contested opposing-army/pending-combat-queue
package evidence and direct complete-match play. Exercise terminal endpoint loss,
wrong-tick/hash/disconnect/no-winner-cap/accepted-tail-prefix and transport identity
regressions. Later AI/content work must support worker replacement/all-workers-dead
recovery, finite-resource strategy and varied army policies; one-worker salvage
reservation does not fulfill those requirements. Improve capture framing (current
counterattack PNG misses army), label/bar overlap and selection/waypoint clutter.
All dense-crowd, frame/response, production content/art/audio/animation, physical
network/soak, human/balance and final SPEC gates stand. No COMPLETE.md.

## 2026-10-18 current checkpoint - symmetric-map package verified

Latest PROGRESS/TESTING supersede older terminal-next notes: actual packaged
peers complete/confirm first winner7088 below cap12000;22full native replays,
105prefixes and24corruptions pass. New-map exploration/AI defeat,player victory/
restart and queued economy-network route retain package proof. Stale exploration/
scale checks repaired; no rules changed. Independent critics inspected actual
source/captures. Held armies remain undamaged; human acceptance is absent.

Exact next: correct/test rotational AI Foundry and worker service policies,
then vary AI/build parameters before attributing/changing lower-ID iteration.
Map symmetry does not make these policies symmetric; prior30/30P0 did not isolate
first-mover bias. Then coordinated setup/rematch,successful-pair offline return/
input,contested armies/pending-combat queues,direct complete games. Early-outcome
fault/tail-prefix and transport compatibility tests remain. Preserve every crowd/
frame/response,production content,art/audio,physical-network/soak,human/balance and
SPEC shipping gate. No COMPLETE.md.


## 2026-10-11 checkpoint - network route and unit feedback

Paired rendered economy clients now retain canonical evidence through exact
production/refund, flux/research/Lancer, four queued formation legs and anchor
victory. Both confirm 12000 hashes; 22 complete replays, 106 prefix checks and 19
corrupted cases pass rejection. Unit roles/front production, Lancer feedback,
per-kind health bars and network player/delay text are corrected. This is a
scripted attacker versus a holding opponent whose combat units survive undamaged;
it does not establish contested 1v1, human play, balance or the small-match gate.
See current PROGRESS/TESTING and independent reviews dated 2026-10-11.

Exact next: end the shared session when both peers confirm the authoritative
outcome, preserving terminal checksum, disconnect/desync and replay-prefix
protection. Winner tick 6988 currently waits until cap 12000 (250.6 seconds),
while input/restart are blocked. Then coordinated setup/rematch and contested
opposing-army/pending-combat queue fixtures, followed by direct full-match play.
The Oct5 simulation firing-suspension test remains separate from package evidence;
this run's group queues drain before combat. Six-unit overlay clutter/HUD occlusion
and blockout art remain. Preserve every failed crowd/frame/response, production
content, human-play and SPEC shipping gate. No COMPLETE.md.

## 2026-10-04 current checkpoint - queued feedback and verified error return

Latest PROGRESS/TESTING supersede older flux/setup/economy-network-next notes: those systems now exist. Selected units display authoritative queued destinations and full capacity. Ordinary packaged inputs verify one worker's FIFO/overflow/clearing/reselection/restart, with Debug+ten Release replays. Terminal network HUD instructions survive economy refresh; real missing-peer R returns to ticking offline play. No completion or human-play claim.

Exact next: extend the packaged two-client economy fixture through cancel/refund, research/Lancer, queued combat orders and victory; retain canonical recordings and cross-build traces. Add a combat-unit queue test that suspends pending legs during acquisition/firing plus multi-unit formation/overlay evidence. Then coordinated setup/rematch and direct full-match playtesting. Preserve all previously failed crowd/response/frame and production-content/AAA/shipping gates. See review-queues-2026-10-04.md for concrete limitations.

## 2026-09-30 user-requested larger map and fog checkpoint

64x48 economy map, camera/minimap navigation and deterministic shared team fog
are implemented and packaged. Gathering, construction, production, AI attacks,
victory/defeat and restart have retained replay evidence. See current PROGRESS,
TESTING and PLAY. Terrain occlusion and strategic AI scouting remain limited.
The next content priorities remain contested flux and meaningful technology,
then minimal setup and remaining small-match integration. Earlier fog-next
instructions are superseded by this bounded fog implementation; all final
content, performance, multiplayer and shipping gates remain required.

## 2026-09-29 player-victory and role-control checkpoint

Packaged player victory against active economic AI is now retained at accelerated
and ordinary 20 Hz pace, with input restart and every tick matching Debug plus ten
Release replays. F1 selects workers, F2 army; the counterattack leaves miners
working. HUD includes role/group/camera help. See current TESTING/PROGRESS and
review-victory-2026-09-29.md for evidence and limits. Scripted success does not
establish human play, balance or the complete small-match gate.

Next implement contested flux and meaningful technology through canonical commands
with economic-AI participation; fog/scouting and minimal setup follow. Preserve
queued-orders/focus-targeting/economy-network requirements and every failed dense-
crowd, response/frame, production-content and shipping gate. This supersedes the
player-victory-next line in the historical checkpoint below. No COMPLETE.md.

## 2026-09-29 economic AI and anchor outcome checkpoint

The default offline economy now has canonical-command Gather/Build/Train/
AttackMove AI, attackable structures, anchor victory/defeat/draw and restart.
Destroyed Foundries forfeit paid queues and reopen their navigation footprints.
The package has audited player production, opposing economy, attacks, defeat
and restart at accelerated and ordinary speed; player-victory remains unverified
in the package. Enemy-building context is proximity AttackMove, not exclusive
focus targeting. Queued unit orders remain missing.

Next retain a packaged player-victory route against economic AI, then implement
contested flux plus meaningful technology and AI use through canonical commands.
Fog/scouting and a minimal setup screen follow. Do not claim the full small-match
milestone before that complete route, full required systems and direct playtest.
Preserve all dense-crowd, latency/frame-budget and production-quality failures.
See current PROGRESS/TESTING and review-match-2026-09-29.md for evidence limits.
This checkpoint supersedes the older economic-AI-next instruction below.

## 2026-09-27 production checkpoint: economic AI and victory next

Paid Foundry queues/population now work in the package: T trains a 50-salvage
Strider, X refunds the last queued unit, five queue slots, 100 training ticks and
12 live-plus-reserved population. New units can be selected and commanded.
See current PROGRESS/TESTING and independent production review for exact scope.
Continue the small-match sequence at economic AI and anchor combat/victory,
using the same canonical commands. Define destruction treatment for paid queues
when adding building combat. Finish queued unit orders/enemy context targeting
alongside that match integration. Flux/technology/fog/setup follow. This current
checkpoint supersedes older production-next instructions below while preserving
all gameplay-first priorities and final scale/quality/shipping gates.

## 2026-09-26 economy checkpoint: production is next

The first packaged worker/salvage/construction increment is verified; see the
latest PROGRESS.md, TESTING.md and economy review. Continue the small-match
sequence below at step2: resource-funded unit production, queues and population
limits, with visible purchase/queue/spawn feedback and canonical replay tests.
Then deliver economic AI and anchor victory. The earlier instruction to deliver
the first economy increment has been fulfilled for its bounded scope. Existing
large-crowd/performance/production-quality failures remain required final gates.
This checkpoint preserves the user's gameplay-first sequencing change below.

## Current priority: complete a playable small match (2026-09-26)

User-directed sequencing change: build the complete small RTS gameplay loop now.
This section supersedes older movement-first next steps in dated checkpoints,
reviews and handoffs. Preserve the existing game, deterministic architecture,
tool pins, regression coverage and every final SPEC acceptance requirement.
Large-scale crowd recovery and performance acceptance remain required, but are
no longer prerequisites for implementing economy, construction or match content.

The next development run must deliver the first playable economy increment:
command anchors, workers, salvage deposits, gather/return orders, resource
accounting and construction of a production structure through canonical commands.
Expose selection, resource totals, build costs and construction progress in the
actual packaged client. Reuse existing combat and presentation where suitable.
Start with this bounded feature increment and carry it through to playable
interaction; do not spend the run exclusively on a new movement experiment.

Implement the remaining small-match scope in this order, with each checkpoint
adding a usable part of the same match:

1. Worker harvesting and construction, as specified above, including valid
   placement and movement around the structures actually used in the match.
2. Resource-funded unit production, production queues and population limits;
   clear command feedback, queued unit orders and enemy context targeting.
3. A canonical-command economic AI that gathers, builds, produces and attacks;
   destroy the command anchor to win, with defeat and reliable restart.
4. Contested flux, a meaningful technology progression and fog/scouting, with
   the AI participating in those systems and a minimal skirmish setup screen.
5. Integrate and play complete matches, address demonstrated blockers, and
   provide readable unit/building roles, essential action feedback and sound.

Use one small map and a modest starting population to develop this milestone.
That working scope does not replace the final two factions, three maps, unit
rosters or 200/500-unit budgets. Fix movement/collision, dynamic construction,
determinism or performance defects immediately when they block the small match;
retain failed large-scale fixtures and defer unrelated solver research until
the gameplay loop works. Do not hide failed scale gates or mark them passed.

Small-match acceptance requires an actual packaged game in which a player can
start from workers and an anchor, gather both resources, construct, produce,
tech, scout, fight an economic AI and reach victory or defeat, then restart.
Retain complete-match command recordings with matching Debug/Release replays,
inspect gameplay captures and record direct playtest findings and remaining
limitations. A headless AI battle, a build, or passing infrastructure tests alone
does not establish this milestone or human-play quality.

Each development handoff must state what the player can newly do, how that was
verified in the package, and the next missing gameplay capability. If a blocker
prevents that increment, retain a concrete reproducer and a bounded repair plan;
do not repeatedly substitute diagnostics-only passes for gameplay delivery.
Build Debug and Release and run the relevant existing and new checks before
committing implementation. Broaden verification for affected simulation,
replay, network and presentation behavior; avoid repeating unchanged suites
without a new change, failure or unresolved concern. Shipping still requires
the full regression and independent review in SPEC.md.

## Milestone order

1. **Foundation / playable skirmish (first increment verified):** independent deterministic sim, canonical commands/persistent replays/tests, pinned extension, camera/selection/move/combat, original animated Blender unit and packaged runtime evidence exist. Production art, crowd routing and manual responsiveness remain open.
2. **Real network lockstep (packaged loopback client path verified):** protocol 2 scheduled frames and independent executed checksums now drive two rendered Godot packages through the reusable Session. Players select their own army; readiness/stalls/errors/final confirmation and explicit input delay are visible. The six-case client suite covers clean/80/160 ms RTT, withheld input, mismatched delay and disconnect, with exact applied-prefix replay and original event-to-execution/render timing. Offline mode remains intact. This is loopback combat-skirmish evidence, not LAN/Internet or full RTS multiplayer acceptance. Only three synthetic commands per peer/profile were measured; strict pacing and frame targets remain unmet. Follow-up network gates: broad input phase/cadence and queued-during-stall/tail tests, packaged post-advance desync recording, physical endpoints, coordinated match setup/restart and complete-match/30-minute soak. Preserve all CLI/API/client regressions and tool pins.
3. **Complete small match (active milestone):** implement the playable economy increment and ordered gameplay scope above. Existing any-angle routes, bounded crowd fixes, control groups and replay/network infrastructure support this work. Repair movement defects that block the match; the failed dense-stream and scale-performance gates do not block gameplay feature development.
4. **Scale, content and production systems:** after the small match works, complete general crowd recovery, dynamic routing and representative 200/500-unit performance, expand to two asymmetric factions and three maps, and finish the art/animation pipeline, UI/audio/VFX, tutorials, accessibility and match UX. Preserve the existing scale contract, failed reproducers and quantitative budgets.
5. **Quality iterations:** independent visual/RTS/determinism/performance critics, reference comparisons, large battles, balance, responsiveness and network soak on documented hardware.
6. **Shipping gate:** full regression, actual complete games, reproducible package, adversarial independent review against every SPEC requirement. COMPLETE.md only with passing evidence, then pause automation.

Reorder work when measured defects justify it, preserving architecture and all quality gates. No deadline-based relaxation.

## 2026-09-13 checkpoint refinement

Historical evidence and deferred investigation notes follow. Their sequencing
instructions are superseded by the current priority above.

The existing Scale128 scenario is now available in the actual offline package
with configurable 1..250 walkers per side, map-aware camera/orders/minimap and
restart preservation. Packaged 200/500-unit InputEvents and full recorded tick
hashes replay across builds/repeats; see TESTING.md. Large-map networking remains
separate. This is diagnostic access to the same simulation, not complete matches.

Early opposing-stream bias and bounded common lateral translations both failed
the unchanged 500-unit crossing case and were removed. The critic proved legal
lateral moves can merely oscillate and press neighbors against the ridge ends.
Next movement work: purposeful backward decompression and a retained passing side,
with explicit Stop/Hold/firing eligibility and the same full safety/progress gate.
Do not repeat undirected lateral fallback or merely increase component limits.

Large-map captures expose poor crowded readability and failed frame intervals.
The same-run twelve-unit capture also slows under hidden-window conditions, so
first profile ordinary foreground play with controlled presentation conditions;
separate capture/fixture overhead, snapshot conversion, skeletons, draw calls and
HUD cost before choosing an optimization. Preserve the original budgets and
report actual product responsiveness separately. The former movement-first
content prerequisite is superseded by the current small-match priority.

## Required movement and pathfinding acceptance (retained final gates)

These requirements remain mandatory for final acceptance. During the active
small-match milestone, prioritize the subset needed for the actual match;
complete the general scale requirements in milestone 4.

2026-09-20 evidence refinement: ordinary-play stage profiling is now available
in `tools/profile.ps1`, with raw focus/window/stage samples, payload fingerprints
and an independent verifier. A measured HUD optimization batches static minimap
cells and avoids hidden-bar projections; final 500-unit HUD p95 is 4.501 ms.
Whole-frame p99 remains 20.400/63.897 ms for 200/500 units on the development
host. Focus flags pass in hidden launches, but unoccluded human foreground play,
same-state engine animation/skeleton/render attribution and representative frame
acceptance remain open. Preserve the independent evidence in TESTING.md. This
does not change the failed crowd-progress gate. It does not block the current
small-match gameplay implementation.

The bounded current-map implementation now has arbitrary authoritative headings and static clearance routes. Finished movement must additionally satisfy the dynamic crowd, route quality and performance gates below, with direct travel toward unobstructed destinations and efficient routes around obstacles. Neither four/eight-direction movement nor presentation-only smoothing satisfies this requirement.

The algorithm and navigation representation remain open: constrained Delaunay triangulation is not required. Choose the strategy using measured route quality, runtime cost, memory use and deterministic behavior. All authoritative routing, collision and avoidance remain in the standalone integer/fixed-point simulation, with stable ordering and identical Debug/Release replay results.

Acceptance requires:

- **Optimal or near-optimal paths:** measure route length/cost against an independently validated shortest feasible reference using the same unit clearance and terrain costs. Include open ground, angled walls, concave obstacles, narrow passages and long detours. Define and record a quantitative near-optimality tolerance before accepting the implementation; report worst-case as well as aggregate error. Visual smoothness alone is insufficient.
- **Fast, efficient execution:** profile initial group orders, shared destinations, repeated orders and replanning after blocker changes on the existing representative 200-unit and 500-unit scenarios. Record pathfinding time, simulation p95/p99, memory and command-response latency; meet the existing SPEC budgets without weakening them. Bound and deterministically schedule work to avoid frame/tick spikes.
- **Robust movement in actual play:** validate unit clearance, consistent speed across headings, responsive stop/retargeting, crowded chokes, opposing streams, moving blockers, unreachable goals and dynamic construction without clipping, permanent crowd locks or needless zigzags. Measure dynamic crowd behavior separately from static shortest-path quality so necessary collision avoidance is distinguished from poor global routing.
- **Evidence before acceptance:** retain deterministic regression fixtures, route-quality comparisons and performance captures; inspect packaged gameplay recordings and obtain independent pathfinding/playability review. Any-angle movement and efficient near-optimal routing are required functionality, not optional late polish.

Controlled diagnostic contract (2026-09-25): fixed seed/map/tick 40, fixed
camera and clip phase 0.25 seconds. Pose refresh repeatedly submits that pose;
pose frozen initializes the identical pose and disables automatic updates.
This measures manual resubmission plus downstream effects, not ordinary
animation, isolated skeleton/GPU time, or gameplay. Expanded static geometry
and exact bone-pose hashes must match for comparisons. A rebuilt native DLL
caused the first implementation comparison to reject; fresh matched-native
captures use a separately retained baseline PCK with the final EXE/DLL.

## 2026-09-26 movement experiment rejection

Six portal-lane variants failed the unchanged 500-unit crossing (25/0/12/49/84/72
exact arrivals). All authoritative changes were removed. The reviewed checkpoint
retains twelve small regression trajectories, exact relative-sweep checks and
final routing diagnostics. Read `review-crowd-recovery-2026-09-26.md` and the
rejected experiment archive before resuming. Fixed gate offsets and passing-half
restrictions did not coordinate same-stream merging, egress or goal fan-out.
The deferred movement investigation calls for bounded follower dependencies and cooperative entry/exit ordering,
with a geometrically valid progression objective and full collision checks;
retain exact goals and immutable Stop/Hold/firing eligibility. Passing the small
tests is preservation evidence only. Dense arrival, full matches, production
content, responsiveness, reference performance and shipping remain open.

## 2026-09-26 cooperative follower checkpoint

Bounded transactional following fixes the demonstrated tangent-convoy defect:
six units now advance together without artificial stops or lateral diversions.
Canonical goals, exact full-tick relative sweeps, immutable Stop/Hold/firing
participants, rollback and deterministic work caps are preserved. This does not
solve opposing-stream progression, and the full arrival contract remains intact.

The optional six-unit reversed-goal probe isolates the next smaller defect.
Five units arrive; unit 2 stops at (2448,2928), targeting (2304,2944), between
already-arrived neighbors at z=2816 and z=3072. Independent frozen-scene geometry
permits vertical alignment to z=2944 at x=2448, then horizontal travel into the
exact slot. When this deferred investigation resumes, implement bounded, collision-checked goal-axis or multi-blocker
clearance-boundary candidates. Prove this canonical fixture before combining it
with cooperative entry/exit ordering on the unchanged 500-unit opposing stream.
Do not displace stopped arrivals, change goals, substitute static feasibility for
executed progress, or claim transaction-cap/cycle exhaustion coverage that has
not been exercised. Preserve replay/network/package gates and original budgets.

## 2026-10-19 checkpoint - rotational policies done

AI/service/exit/formation/tie policies are now exact rotations (mirror test). Residual duel
bias is combat tie order. Exact next: quantify/neutralize combat-order bias, vary AI build
parameters, general stationary-blocker deadlock handling, then setup/rematch and contested
network fixtures. All prior gates stand. No COMPLETE.md.

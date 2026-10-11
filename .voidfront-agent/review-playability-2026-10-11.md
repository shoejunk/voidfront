# Independent visual / RTS / performance review - 2026-10-11

Reviewer: `/root/playability_critic`. Owns this report only; no implementation,
Git operations, game launches, builds, or fixture authorship. STOP absent;
primary run marker present. Read AGENTS, SPEC, PROGRESS, TESTING and roadmap.
This is a bounded review, not a shipping review or quality approval.

## Baseline source findings

Examined `client/main.gd`, `client/hud.gd`, `client/net_economy_smoke.gd`,
`tools/verify_net_economy.py`, bridge snapshot fields, simulation unit/production
constants, PLAY, and the retained 2026-10-04 queue review. Current sources confirm
the prior handoff's queue destination semantics; the memory registry was used
only to orient that lookup.

1. Economy selection labels all nonworker units STRIDERS, including Lancer kind 2.
   Foundry training progress says STRIDER regardless of front queued type; refund
   notice says queued Strider even when it refunds a Lancer. These are concrete
   identity errors exposed by the planned research/Lancer route. Snapshot
   `queue_lancers` bit 0 already identifies the front; generic or accurate wording
   can fix these without modifying simulation rules.
2. Unit health bars use reset-era global `game.max_hp`, rather than authoritative
   per-unit maximum health. Workers/ordinary Striders initialize at 100, Lancers
   at 70 and hardened Striders at 150. A healthy Lancer reads 70%; a hardened
   Strider damaged from 150 to 100 still reads full. This is a misleading combat
   cue. Any correction should derive the maximum from simulation-owned facts.
3. Economy status replaces the network header's player/delay information. The
   remaining connection text shows UDP ports/ticks but loses readily readable
   ownership/input-delay identity in otherwise symmetric economy windows.
4. Setup is an offline keyboard panel; network launch still requires matching
   commands in two terminals. R blocks active shared restart, then detaches
   only this client to offline at completion/error. No coordinated paired
   setup/rematch exists. Outcome/help corrections alone cannot close that gate.
5. Queued overlays are accepted destination connectors, not navigation routes.
   Every selected unit gets separate markers with the same ordinal. Until final
   group captures are inspected, one-worker readability cannot establish group
   clutter, edge clipping, combat readability, or accessibility.

The primary received findings 1-3 for a coherent presentation correction.
Baseline fixture source ended after gather/build/Strider and idled: it did not
establish network refund, research/Lancer, queue/combat or anchor victory.
The baseline verifier principally trusts fixture checks and hash traces; later
evidence needs explicit phase/state checks and replay binding.

Historical actual images independently opened during this review:
`artifacts/queue-final-2026-10-04-filled.png` (1920x1080) and
`artifacts/netecon-final-2026-10-04/p1.png` (1280x720). The filled single-worker
chain is legible as 0-4 and FULL 4/4, but the anchor label overlaps a worker health
bar. Both retain bright reused walker rigs, cuboid structures/deposits, tiled
blockout terrain and large static instruction panels. The earlier network image
ends at SESSION LIMIT REACHED rather than anchor victory and lacks player/delay
identity. These are baseline observations, not evidence for this run's package.

## Final evidence review

Final retained screenshots independently opened from
`artifacts/net-match-final-2026-10-11`: player 0 group-queued, group-arrived,
lancer-queued, combat and outcome; player 1 outcome. These are 1280x720 stills.
Both final terminal p0.png/p1.png, reports and host.json were also inspected.
Full replay execution/audit belongs to the separate determinism reviewer; this
reviewer did not run it. No network acceptance is inferred from images alone.

Reports pass with both clients at 12000 executed/recorded/confirmed ticks and
winner 0, 75/63 checks, 22/18 labeled inputs, 39/31 phases and 14 captures each.
Host metadata records distinct processes 51428/16808 and ports 57754/57755.
Independently hashed frozen EXE/PCK/DLL and matched all three host fingerprints:
EXE d34d36f3be1a6c49c56525ae86469b92e4f417ddf0b43cf00dd80c385c4b0562;
PCK ca457e6e5f0813851256dd734ad91ec68ae7930075367669846909b8b5326391;
DLL 4bbf602b1faccfc36a39339b9ad5f4224ecad4ae74228f19004df0c17ede7400.
Terminal images have readable explicit victory/defeat, Complete status and R
return-to-offline action. Bottom help explains relaunching both clients for a
new network session. This supports the bounded terminal presentation correction.

Independently read final `tools/verify_net_match.py` (authored by the determinism
critic, not this reviewer). Its declared economy/research/spawn/group endpoint
checks bind labeled events to VFR3 commands, compare every phase and capture
snapshot to native replay-prefix diagnostics, and compare complete traces with
length-sensitive zip_longest. No obvious blocking defect found in that bounded
contract. Capture HUD/selection/path metadata is not comprehensively compared
to native state, although group phase paths are authoritative-bound; actual
opened images and source checks supply separate presentation evidence. This
verifier does not audit every-tick swept collision safety or response/frame
budgets and cannot establish motion or human decisions.

Source integration checked during the run: bridge snapshot now derives max_hp
from Unit default 100, Lancer 70 and simulation researched-state Strider bonus 50;
HUD divides by that per-unit value. Selected Lancers get their own count and
Foundry front type uses queue_lancers bit 0. Refund wording is generic and Lancer
submission displays both costs. Player/input-delay identity is retained in the
connection line. These address baseline findings 1-3 in source. Primary added
order 10 to structure-order marker placement,
fixing Lancer training feedback appearing at map origin.

Expanded fixture source has explicit fixed roles: a resource-funded attacker
and a smaller passive Hold defender, both using software InputEvents. It queues
four legs on mixed armies near their own bases before the attack. Even a passing
run would establish scripted economy-route reachability plus bounded group
formation/overlay behavior, not competitive player decisions, balance, queued
combat firing suspension in the package, or hostile crowded-path acceptance.

First pair, not final acceptance: actually opened
`artifacts/net-match-first-2026-10-11/p1-{production-queued,completed,refunded,flux-revealed}.png`
(1280x720). New role line includes LANCERS and readable PLAYER 2/DELAY 2 identity;
Foundry front progress remains visible at 7% after refund. Anchor label still
intersects an adjacent worker/healthbar, reused workers remain bright and tiny,
and flux discovery text is partly covered by black fog at the reveal boundary
(only the trailing 000 is visible).
Player 0 fixture reports failure to fund its larger army by tick 2000. The
primary retains this failed run and launched a distinct final run with unchanged
funding/army targets and a longer declared wait. It does not pass this review.

Also opened first-pair player 1 lancer-queued, lancer, group-queued and
group-arrived PNGs. Foundry correctly says LANCER 1%; two-unit selected roles
and FULL 4/4 are readable. The two separate 0-4 chains remain distinguishable,
and arrived image removes them. A healthy Lancer healthbar appears full,
consistent with the corrected 70/70 field. One starting unit/connector lies
behind the bottom instruction panel, and the leftmost flux label is clipped
at the viewport edge.
Destinations in dark fog are legitimate accepted commands, but connecting
lines cannot promise discovered/obstacle-free navigation. Ranged-unit identity
still relies on stretching the shared walker rig rather than finished art.

Final player 0 six-unit group-queued image correctly shows five Striders, one
Lancer and FULL 4/4. Per-unit destinations are present, but repeated ordinal
labels and crossing dashed connectors form a dense overlapping ladder; the
player cannot easily follow one unit's complete chain. Several selected units
and connector origins sit behind the top status panel. The drained image removes
the paths, yet closely packed models, rings and healthbars overlap at the shared
formation. This bounds the new group evidence; it does not establish clear
production group feedback or crowd responsiveness. The final Lancer-queued
image correctly identifies the front type. New player/delay identity remains
readable without contradiction.

Both final outcome images clearly distinguish VICTORY/DEFEAT and destroyed
anchor at tick 6988, confirmed 6988. They still display "Awaiting session
confirmation" and running status. The player 0 image named combat contains an
empty dark field with HUD, not visible fighting; it supplies no visual combat,
animation or targeting-quality proof. The phase report can establish acquisition
separately. Result panels cover most surviving units/buildings. No movie,
listening, manual interaction or SC2 comparison was supplied.

Actual outcome snapshot keeps both defender combat units alive and undamaged:
Strider 150/150 and Lancer 70/70, still Hold at x52.5 and z18.5/17.5. The defender
army does not protect the anchor at x59.5,z36.5. Opposing anchor is 0 HP and own
anchor stays 1000 HP. The scripted route therefore establishes resource-funded
anchor attack/victory under fixed asymmetric roles; it does not demonstrate a
contested opposing-army fight, player strategy or balanced full 1v1.

Host wall time is 608.1437505 seconds for the configured 12000-tick pair;
sampled per-process peak resident bytes are 272330752 and 267759616. These are
launcher observations, not CPU/frame/simulation/input-latency measurements.
No representative 200/500-unit/reference-hardware performance acceptance follows.
The failed prior pair overlapped part of this capture, so no optimization or
controlled performance comparison is supported.

## Next player-facing blocker: victory does not end the network session

Source-backed: `net/session.cpp` sends Finish only when stats.tick equals
configured o.ticks (line 240), accepts terminal confirmation only at that cap
(line 300), and enters Complete only there (lines 364-375). It does not terminate
on authoritative winner. The client disables gameplay orders after outcome,
continues polling ticks, and refuses R until Complete/Error. Consequently a
normal 36,000-tick launch can leave an early victor/loser waiting minutes on
"Awaiting session confirmation" even when executed-state hashes are already
confirmed. Actual final outcome images demonstrate this at 6988/6988. This run's
12000 cap leaves 5012 ticks, or 250.6 simulation seconds, before terminal
confirmation can start. The default 36000 cap would leave 1450.6 seconds
(24 minutes 10.6 seconds) after the same outcome. This is a required match-lifecycle
repair. Next run should define outcome-driven coordinated terminal confirmation
with replay-prefix preservation and disconnect/desync protection, then consensual
paired rematch/setup. No unilateral reset of the shared simulation.

## Retained acceptance gaps

Verdict: accept the bounded identity/health/marker and terminal presentation
improvements, with the actual evidence and source corrections above. The new
scripted network economy route supplies a coherent next integration checkpoint;
canonical/replay acceptance requires the separate executed verifier report.
The strongest next player-facing fix is outcome-driven shared session ending,
then coordinated setup/rematch and direct full-match playtest. Group marker
clutter, HUD occlusion and missing visible combat/motion evidence need follow-up.

Human play, physical input responsiveness, animation/motion/deformation/audio,
SC2 reference comparison, LAN/Internet, 30-minute completed network matches,
coordinated setup/rematch, balance, production factions/maps/content, dense crowd
progress and reference-hardware representative 200/500-unit performance remain
open. Small scripted economy matches and still images cannot establish these.
No AAA/SC2-quality, completion or shipping approval.

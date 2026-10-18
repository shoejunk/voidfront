# Symmetric-map integration review - 2026-10-18

Independent source/evidence reviewer for the primary weekly run. STOP was absent;
SPEC, PROGRESS, TESTING, roadmap and AGENTS were read. No Git operations or
authoritative simulation changes were made by this reviewer.

## Source findings and bounded repair

- The exploration fixture uses the corrected enemy anchor (59.5,35.5). The paired
  economy fixture derives anchor/resource coordinates from authoritative snapshots;
  no stale old-anchor or old-terminal-tick constant was found there.
- `verify_exploration_capture.py` still required replay protocol 9 although the
  current simulation uses protocol 12. At the primary's request, this reviewer
  added `initial_snapshot` to the exploration report and bound replay
  protocol/content/map to that setup, retaining fixed seed 1/count 3 and every
  Debug plus ten Release native replay. Five deliberate setup/trace corruptions
  and bounded header checks were added. Python syntax parsing passed. The fresh
  packaged exploration audit subsequently passed (details below).
- `verify_match_capture.py` still recognizes commands 0..8 and treats only 7/8
  as structure commands. Current AI can emit Research 9 and TrainLancer 10 once
  flux is explored. This is a conditional audit gap, not a demonstrated current
  fixture failure; if encountered, update full economic/damage semantics as well
  as command parsing. Do not simply relax the order bound.

## Independent review of terminal audit

The deterministic reviewer's `verify_net_match.py` edits bind both caps, bound
them, require terminal/confirmed ticks to equal the outcome phase, and replay the
preceding tick to establish no earlier authoritative winner. Source inspection
found the new preceding-tick prefix included in the native replay set. Coherent
post-outcome endpoint padding and invalid/mismatched caps gained corruption
checks.
The followup negative-test hardening was also source-reviewed: endpoint/cap
cases require their intended rejection reason, so a later baseline trace mismatch
cannot hide a removed early-outcome assertion. Coherent padding grows the cap
when necessary. Summary fields retain requested and first-outcome ticks.

This reviewer independently executed the report-only audit against retained real
evidence: Oct 17 reports (terminal/outcome 6988) pass; Oct 11 reports
(terminal 12000, outcome 6988) reject with "session terminal tick must equal
authoritative outcome tick". This is compatibility/regression evidence for the
auditor, not a new network run or a replay of old content using current binaries.

## Balance diagnosis remains open

The latest PROGRESS entry attributes residual mirrored-duel P0 wins to ID/tie
priority, but AI construction is not rotationally symmetric: `make_ai_commands`
multiplies preferred x offsets by player direction and leaves z offsets unchanged.
Nominal preferred sites therefore have offsets (+5,+3) and (-5,+3), whereas
rotational partners require (+5,+3) and (-5,-3). Build feasibility can select a
fallback, so actual chosen sites must be retained in the next controlled balance
probe. A symmetric map and repeated seeds do not isolate first-mover bias from
this strategy/layout asymmetry. Do not fix tie priority based on that attribution
alone.

## Fresh exploration and scale-adapter evidence

This reviewer inspected `artifacts/weekly-exploration-2026-10-18.json` and
`artifacts/weekly-exploration-audit-2026-10-18/summary.json`: 3032 ticks, 36 checks,
5 human and 14 AI canonical commands, protocol 12/content `49a23b592d609ecb`,
Debug plus ten Release traces matching every captured hash, and all five deliberate
setup/trace corruptions rejected. These are actual current-layout package results.

The home, scouted, explored, enemy-contact and defeat PNGs were actually opened
with `view_image`. The scouted region is bright and becomes dim after retreat;
enemy contact shows orange enemies, attack lines and damaged Foundry; the defeat
banner is legible. The scalar `enemy_visible` in vision samples refers to the
opposing anchor, not all enemy units, so its remaining false at enemy-contact is
consistent with this capture.

An initial apparent help-text truncation was rejected on further inspection:
opening the scouted PNG at original detail shows all control suffixes, and a
pixel comparison of the scouted/explored help rectangle returns no difference.
There is no supported help-clipping defect from these captures. Blockout
terrain/buildings, reused bright rigs, screen-edge/HUD
occlusion and overlapping world labels still prevent a production-quality claim.
Still images do not establish deformation, smooth animation, audio, responsiveness
or battle performance.

The primary's `bridge_scale_test.gd` metadata fix was independently source-reviewed.
Comparing the same bridge's ordinary initial protocol/content with scale reset
metadata correctly checks preservation without a stale protocol-4 literal. The
actual map/population, invalid-reset preservation, canonical movement/reset parity
and active-session mutation guards are intact. This is an adapter contract;
protocol/content compatibility correctness is exercised by separate native replay
checks, not independently established by comparing the bridge with itself.

## Fresh player-victory evidence

This reviewer inspected `weekly-victory-2026-10-18.json` and its independent audit
summary: 4432 ticks, winner 0/hash `6fb7a43ab1ef02f3`, 38736 unit rows, 125 damage
ticks, 6000 conserved resource units, 11 deliberate corruptions rejected and Debug
plus ten Release matching replays. The active AI/player route reaches victory and
restarts on the current layout. It runs eight simulation ticks per rendered frame
and overlaps the paired run, so it does not establish ordinary timing or performance.

The opposing-production, counterattack, victory and restarted PNGs were opened.
The victory banner and clean three-worker restart are legible. The counterattack
PNG shows dark unexplored ground and a white ring, with no visible army/combat;
the victory camera mostly shows the home/flux area. These captures therefore do
not visually demonstrate the enemy anchor's destruction or visible army combat.
The canonical replay/semantic audit supplies separate authoritative outcome proof.

## Fresh paired symmetry capture review

Both actual packaged reports under `artifacts/net-match-symmetry-2026-10-18`
are successful and complete at tick 7088, confirmed 7088, requested cap 12000,
winner 0/hash `c68e8d4f139b2618`. P0 retains 75 checks/22 inputs/39 phases;
P1 retains 63 checks/18 inputs/31 phases; each retains 14 fixture captures.
The independent deterministic reviewer is executing the full native replay and
prefix audit separately; these counts do not substitute for that audit.

This reviewer opened both players' group-queued/group-arrived, combat,
anchor-damaged, outcome and final terminal PNGs. Two-unit defender queued chains
are distinct and drain; six-unit attacker chains have intersecting connectors
and crowded ordinals, with several origins/units behind the top HUD. Arrival
captures show the mixed formations with overlays cleared. P0 combat is an empty
dark area; P1 combat shows distant incoming units rather than army fighting.
Both anchor-damaged captures show the damaged orange anchor (992 HP) with
attackers/attack lines. Outcome captures show Finishing/awaiting confirmation;
final terminal captures show Complete and the offline-return instruction at
7088. This is visible early-completion feedback on the actual new layout.

Both final snapshots show P1's held Strider 150/150 and Lancer 70/70; all six
P0 combat units also have full HP. P0 anchor remains 1000 HP; P1 anchor is 0.
Thus the fixture is a scripted attacker versus holding opponent, with no damaged
opposing armies. It does not establish contested competitive 1v1. Terminal UI
instructions are verified visually, but R/offline movement after this successful
pair and coordinated rematch are not exercised by this fixture.

## Remaining evidence limits

No new implementation blocker was found in the reviewed source/captures. The
scripted attacker versus holding opponent remains distinct from contested army
combat; coordinated setup/rematch, pending-combat queues, direct human play and
all final SPEC/AAA/shipping gates remain open.

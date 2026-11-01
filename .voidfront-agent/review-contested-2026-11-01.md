# Independent contested-match and offline-return review - 2026-11-01

Review status: bounded verification PASS; AAA/shipping acceptance rejected.

The independent reviewer inspected SPEC.md, current handoff, the changed client
and launcher, the contested fixture, native Session terminal behavior, and the
extended replay auditor. No implementation files were changed by the reviewer.

The client freezes original player, winner, final tick, canonical shared recording,
and terminal capture before sending ordinary R/F1/RMB events. The separate offline
report/recording prevents the new player-zero inputs from entering the original
shared trace. The native Complete state follows its existing full-timeout terminal
linger and socket closure; reset after Complete does not newly skip that linger.
Positive-tick offline reset snapshots permit native prefix verification.

Source review caught the contested auditor selecting the first queued command,
which belonged to the earlier formation route. That would reject all later valid
combat commands. The auditor now binds the named contest_queue fixture input to
its matching per-player canonical command. Root also added an explicit first
offline tick wait after reviewer feedback to make reset snapshots auditable.

Independent compatibility check: current audit_reports accepted the retained
Oct25 passive-route reports and both recordings: 40 canonical commands, terminal
tick7147. This is structural compatibility with historical evidence only; it is
not a new package or runtime claim.

The opt-in combat route remains deliberately unequal (six attackers versus two
defenders) and scripted. Even successful reciprocal damage, queue suspension,
resume, outcome, and offline-return evidence cannot establish competitive1v1,
human usability, balance, rematch coordination, LAN/Internet, impairment/soak,
large-battle performance, production presentation, or AAA/shipping acceptance.
Every corresponding SPEC gate remains open. No COMPLETE.md is justified.

Fresh screenshots, native replay audit, measured outcomes, and final bounded
verdict are recorded below, including the rejected initial candidate.

First live candidate: directly inspected p0/p1-queued-combat.png and
p0-queue-resumed.png under artifacts/contested-net-match-2026-11-01. The P1
image frames both armies exchanging tracer fire; both selection and pending
waypoint are visible. The P0 army is crowded, with dense yellow waypoint lines.
The resumed image frames the surviving army with the pending overlay cleared.
However root identified that the P0 tick6490 firing witness targeted a worker,
not a combat unit. The independent critic recommended requiring an opposing
nonworker target in both fixture and auditor and obtaining fresh paired evidence.
The first candidate does not close that stronger opposing-army queued-shot claim.
The tightened current auditor rejects this candidate specifically at
`queued firing target is not a damaged opposing combat unit`; the retained log
is artifacts/contested-first-audit-rejection-2026-11-01.log. Both original reports
record Complete/confirmed7277, followed by offline reset tick1 and movement tick6.
Directly inspected its P0 terminal and P1 offline-return images: the outcome/R
instruction and restored worker selection/accepted-order feedback are visible.
These observations do not override the rejected stricter combat witness.

Concrete visible presentation debt: reused similar walker rigs, flat gridded
terrain, cubic buildings/deposits, bright clustered selection rings, overlapping
labels, HUD occlusion near screen edges, and small text at1280x720. These captures
do not satisfy the SC2 production/readability benchmark. No motion/listening,
manual control or frame-time acceptance follows from these still images.

Strict replacement pair: artifacts/contested-final-match-2026-11-01, frozen
artifacts/contested-final-package-2026-11-01. Independently extracted both witnesses:
P0 Lancer10 fires at opposing Lancer9 on tick6584 (target38->32HP, cooldown0->10),
retaining pending AttackMove; P1 Lancer9 fires at opposing Strider13 on tick6552
(target150->144HP, cooldown0->10), retaining pending Move. These now both target
combat units. Both reports confirm first outcome7277/hash8dc6aebe3034b10b and
return to offline reset1/movement6. Directly inspected strict final p0/p1 queued
combat, P0 queue-resumed, P1 terminal, and P0 offline-return PNGs. Both armies are
actually framed during fire; correct confirmed DEFEAT/R text and restored offline
worker selection/order feedback are visible. Presentation debt above persists.

Independently parsed all22 full native replay trace files written by the main
audit (Debug once and Release ten repeats for each client). Every7277 tick/hash
matches the live P0 network trace. Both raw VFR3 recordings are byte-identical,
SHA256d020234f1048bde0d0e9401e20c5f562fbc73cb1e0f95a26189b192bddb8cf8d.
The main audit subsequently completed the prefix/negative/offline checks below.

Read the current approved MSVC verification log: pinned Godot reports
4.7.2.stable.official.ed1daf0bf; Debug and Release each report12/12 CTest passed;
15 malformed recordings per configuration are rejected; cross-configuration
2000-tick traces and ten repeated traces match. This supports regression/build
claims only. The initial sandbox build stall and scoped helper recovery are
separate from gameplay or performance evidence.

Final audit completed2026-11-01T20:20:12.690707+00:00:
artifacts/contested-final-audit-2026-11-01/summary.json reports22 full native
replays,119 Debug prefixes,10 offline runs (four complete Debug/Release and six
Debug prefixes),44 network commands,82 full phase snapshots, and35 rejected
corruptions. The reviewer independently read this summary, verified all frozen
EXE/PCK/DLL hashes still match the launcher record, compared both witnesses'
native prefix fields against their live snapshots, checked native winner-1 at
7276 and winner0 at7277, and compared the two configurations' complete six-tick
offline traces for each former player role. Both offline Debug/Release traces
are byte-identical and restore unfinished AI matches. The35 negative cases
include worker-target substitution, absent cooldown/damage, drained queue,
missing resume, false offline ownership/network state, absent displacement and
broken offline replay. No final source blocker remains for the bounded claim.

Verdict: accept the retained scripted loopback evidence that both sides' queued
combat units fire on opposing combat units without consuming pending orders,
the surviving attacker resumes its pending anchor assault, both clients confirm
the first outcome, and each returns through ordinary R/F1/RMB events to moving
offline workers. The authoritative combat/network rules were not changed by
this increment. The deliberately unequal scripted armies and captures do not
close complete competitive1v1, coordinated rematch, direct human play, balance,
soak/physical-network, performance, finished content/art/audio, SC2-quality or
shipping gates. Next: coordinated setup/rematch and direct complete-match play;
retain the existing presentation/readability and every SPEC deficit.

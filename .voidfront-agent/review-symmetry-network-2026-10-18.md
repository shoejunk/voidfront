# Deterministic/network review - 2026-10-18

Scope: inspected SPEC, PROGRESS, TESTING, roadmap, AGENTS and absent STOP;
reviewed economy constructor/map symmetry regression, AI/service-point ordering,
Session early-outcome handling, bridge applied-prefix recording, and paired-client
verifier. No Git, authoritative simulation, transport, or client implementation
changes were made by this reviewer. Root subsequently assigned exclusive ownership
of `tools/verify_net_match.py` for the evidence-validator repair below; this report
is therefore independent source criticism of the game, but not independent approval
of the validator repair. A separate reviewer must review that repair.

## Demonstrated evidence-validator gap and repair

Before editing, called `audit_reports` on retained
`artifacts/net-match-final-2026-10-11`: it accepted terminal 12000 even though
the outcome phase is 6988. The relaxed terminal<=cap condition therefore did not
guard the early-end behavior. After the repair, those exact report/replay inputs
reject with `session terminal tick must equal authoritative outcome tick`.
`artifacts/net-match-2026-10-17` still passes the same report/replay audit at 6988;
these lightweight checks did not replay either historical map with current binaries.

Repair: both requested caps must agree and be within the Session 1..100000 bound;
outcome phase, final snapshot, applied recording, trace and mutually confirmed
terminal ticks must match. The full audit now replays the tick immediately before
the claimed outcome and requires winner -1 there, proving that the endpoint is
the first authoritative outcome rather than merely a later winning state.
Added coherent post-outcome padding plus zero/overflow/below-terminal/mismatched
cap mutations. Existing semantic, canonical-input, full-hash, prefix and other
corruption audits remain. The summary records requested cap and first outcome tick.

## Source findings and limits

1. Economy positions/terrain in the current constructor are rotation partners.
   The map test checks blocked-cell symmetry, anchor/worker/deposit cell pairing
   and paired flux distance. This establishes geometric layout symmetry for the
   implemented fixture, not gameplay balance. For future robustness, compare
   exact fixed-point pairs as well as cell indices and equal resource amounts.

2. The economic AI is not rotation-equivariant: `make_ai_commands` multiplies
   candidate Foundry x offsets by player direction while applying the same signed
   z offsets for both players (`sim/voidfront_sim.cpp` 668-670). A (+5,+3) player0
   preference rotates to (-5,-3), whereas player1 tries (-5,+3). Service-point
   preference uses global unit ID (`sim/economy.cpp` 33), and congestion retry
   phase also depends on global ID (123). These are confounds in a supposed
   mirrored AI duel. The reported 30 seed runs do not vary economy behavior;
   they cannot isolate the cause of P0 winning. Lower-ID/tie-order first-mover
   effects may contribute, but the precise causal attribution is unresolved.
   Next balance investigation should retain mirrored policy/placement schedules,
   service slots and actual canonical commands before changing priorities.

3. Session early completion derives the endpoint locally from the authoritative
   winner, then still requires contiguous mutually verified checksums, matching
   Finish/FinishAck and retry linger. No packet arrival supplies a winner.
   Bridge recording retains only applied frames, including post-advance errors;
   accepted future inputs can remain unapplied at a terminal outcome. The clean
   economy route schedules no such tail commands, so it cannot prove that tail
   disposition behavior.

4. The early-end transport semantic change was made without changing protocol
   or including `net/session.*` in the content digest. Current changed simulation
   source changes the content ID and rejects prior-map peers, incidentally
   shielding this package from those old peers. A session-only future semantic
   change needs an explicit compatibility policy/version test; source content
   identity alone does not cover transport implementation changes. No mixed-old/
   new-session compatibility experiment was run in this review.

5. Existing generic terminal-loss/desync/disconnect cases are not evidence for
   those faults at an outcome-derived early endpoint. The new Session API test
   is a happy-path AI/passive early completion test. Follow up with early-outcome
   Finish/FinishAck loss, wrong-terminal tick/hash, disconnect, cap-without-winner,
   and accepted-but-unapplied future command fixtures and retained replay prefixes.

## Current-run execution

Independent 300-tick seed7 economy/AI runs in both current MSVC configurations
pass with identical executed-state trace SHA256
`ad4003abb58fa1b6272f0d7c0a7ed260393c563a99e80fd73011ea00a10da072`.
Evidence: `artifacts/critic-symmetry-ai-2026-10-18/summary.json`, both VFR3
recordings, traces, metrics, binary hashes and logs. No Foundry is built by300.
Both balances remain30, but service choices already differ under rotation:
P0worker1 goal(1696,1920) rotates to(14688,10368), whereas paired P1worker4
has goal(14464,10592); their cargo is6/4. Other paired worker goals and cargo
also differ. This executes a concrete policy/worker-service symmetry confound
before combat; it does not establish which confound decides the eventual winner.

The integration critic independently reviewed the final validator source and
reproduced the historical positive/negative report audits; see
`.voidfront-agent/review-symmetry-integration-2026-10-18.md`.

Full strengthened audit PASS:
`python tools/verify_net_match.py artifacts/net-match-symmetry-2026-10-18 --out artifacts/critic-symmetry-net-audit-2026-10-18`.
Summary started2026-10-18T19:09:15.916385Z, completed19:12:50.613766Z;
verifier SHA256 `d86b9745c1a626323a5c1fa36cd6f6100e498b27ecac9aa7d8dbac95f6f29fd7`.
Two rendered client PIDs22732/15052, ports60333/60334, both complete and
confirm7088 ticks, winner0, hash`c68e8d4f139b2618`, against requested cap12000.
Both recordings are byte-identical SHA256
`fd876a80be42c35679aadd80973d66d8baf072325d6054bf53f6ec7e99edf056`.
All22 full Debug/Release replays (one Debug and ten Release per player), all
105 native Debug prefix runs,70 phase snapshots and28 capture snapshots pass.
All24 deliberate corruption cases reject, with the five new terminal/cap cases
failing the specifically intended checks. Native prefix7087 has winner-1;
prefix7088 has winner0, proving first-outcome completion for this route.
The summary retains package, binary, report, replay and PNG fingerprints.

## Bounded verdict and next steps

PASS for the current scripted loopback economy route and exact first-outcome
terminal/replay integrity. The verifier repair has separate independent source
review; this reviewer authored and executed it. The current geometric symmetry
does not establish behavioral symmetry or balance, and direct300tick evidence
shows worker policy/service choices already differ under rotation.

Next: retain rotated AI policy/placement/service and canonical input comparisons
to isolate the observed duel bias; add outcome-derived early-terminal fault/tail
fixtures; then coordinated setup/rematch and contested pending-combat queues.
No current impairment rerun, physical-network/30-minute soak, human play, balance,
representative performance, production-content quality or shipping acceptance.
The existing failed crowd/frame/response and all SPEC gates remain open.

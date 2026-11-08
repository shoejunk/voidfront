# Independent fresh-session transport review - 2026-11-08

Scope: actual diff in net/session.cpp, net/session.hpp, net/session_tests.cpp and
tools/verify_session_reuse.py. Reviewed by a separate critic agent. This review
does not approve coordinated rematch negotiation, a second packaged match,
physical networking, authentication, playability or shipping quality.

Source findings: no correctness blocker found in the bounded identity filter.
The parser first requires the complete 44-byte envelope and expected magic, then
ignores foreign identities before evaluating that session's protocol/content or
payload. The receive loop continues before join, readiness, ACK, authoritative
state or progress mutation. Receive work remains bounded per poll and timeout is
checked before draining packets. Current-identity version/content/player/setup/
input-delay mismatches remain strict terminal failures. A session identity is a
caller-agreed transport discriminator, not authentication or negotiation.

Native tests cover completed-pair endpoint reuse, nine reordered stale envelopes
including old settings/protocol/content, unchanged pre-join state/provider count,
fresh canonical replay, stale-only traffic failing the handshake progress timeout,
and seven current-identity mismatch cases. They are same-process API tests and
cannot substitute for the actual separate-process relay evidence.

The new Python fixture retains all seven authentic transport kinds from a real
first peer pair, then repeatedly injects them from the configured remote endpoint
while another pair uses the same ports with a new identity and seed. Both current
streams must confirm all 100 ticks and replay in the opposite build configuration.
Distinct seed/state streams are asserted. Owned helpers are bounded and cleaned up.

Evidence-quality finding fixed during review: binary fingerprints were originally
persisted only after successful completion. Requested and inspected new run.json
written before helpers start, retaining provenance for failures as well as passes.
The fingerprinted old-binary run at
artifacts/session-reuse-before-fingerprinted-2026-11-08 passes its original pair,
then both new peers fail at tick zero with `incompatible session` when old traffic
is injected. The initial sandbox-only handshake timeout is not product evidence.

Post-fix verdict: PASS for bounded fresh-session isolation. Independently inspected
artifacts/session-reuse-final-2026-11-08/summary.json and raw witnesses, recomputed
all four executable fingerprints against current binaries, compared all four
network traces with their opposite-build replay traces, required 100 entries and
full confirmation, and parsed all 28 raw wire envelopes for identity/player/kind.
First PIDs39788/26096 and fresh PIDs35384/23568 reuse ports56569/56571. Each fresh
peer receives3507 injected old datagrams; both confirm100 ticks with final hash
4549765800361604596. New identity64040904/seed99 differs from first64040903/seed42.
This establishes actual process-to-process loopback isolation, not in-process
networking inferred from a simulation test.

Integrated log artifacts/session-verify-approved-2026-11-08.log records pinned
Godot4.7.2, MSVC Debug/Release12/12CTest each,15 malformed replay rejections and
seven output-alias cases per configuration,2000 cross-build identical hashes and
ten repeated traces. Initial sandbox compiler failure is retained separately.
Also reviewed the added client/bridge_network_test.gd assertions against actual
bridge APIs; artifacts/session-bridge-2026-11-08.log reports ok:true. This runs two
successive same-process bridge sessions with changed identity/seed/economy/delay,
fresh readiness/sequence/timing/recording state, frozen first evidence and offline
reset. It is useful integration coverage, not a rendered paired rematch.

Broader separate-process fault suite also passes22 cases at100 ticks, including
0/80/160ms profiles, jitter/loss, delayed/lost packets, incompatible settings,
desync and disconnect cases. Independently read
artifacts/session-network-final-2026-11-08/summary.json, recomputed its four binary
fingerprints, checked reported applied trace lengths for every peer, and compared
all38 available full/prefix opposite-build replay trace files byte-for-byte with
their network traces. The harness reports ten repeated replays equal and three
malformed replay rejections. This short concurrent run (--jobs3) supports fault
regression only: no isolated timing, response budget, soak or complete-match claim.
The following appendix records the later packaged image inspection. All prior
SPEC quality gates remain open.

## Fresh packaged regression - actual image inspection

Inspected actual files from artifacts/session-net-match-2026-11-08:
p1-harvesting.png, p0-completed.png (construction, not match completion), p0.png,
p1.png, p0-queued-combat.png, p1-queued-combat.png, p0-offline-return.png and
p1-offline-return.png. Both terminal HUDs visibly show COMPLETE/confirmed7207;
P0 shows victory and P1 defeat. Both offline-return images show the starting
three-worker scene without the network connection row. Opposing armies and
weapon beams are visible in the queued-combat captures; pending queue semantics,
damage and accepted offline movement require the native replay audit.

Concrete presentation shortcomings: P1's terminal banner is shifted right so the
offline-return instruction is clipped at the image's right edge. Selected combat
units have overlapping rings/bars and many dotted queue tails cross the battle.
Flat tiled terrain/fog, cubic structures/deposits, reused walker silhouettes and
small crowded HUD text remain. These images do not support polished art, readable
competitive battles, animation, human responsiveness, balance or AAA acceptance.
No movie, audio/listening, human, reference-performance, physical-network or soak
evidence was inspected.

Packaged native verdict: bounded regression PASS. Summary
artifacts/session-net-audit-2026-11-08/summary.json reports22 full replays,
121 native prefixes,35 rejected corruptions,44 canonical commands,82 phases and
ten offline-return runs. Independently verified all22 full replay trace files
against both7207-entry network traces; all report/replay/terminal/fixture-capture
fingerprints; and frozen EXE/PCK/DLL hashes. Native7206 has winner-1 and7207 has
winner0. Both shared VFR streams are byte-identical. PIDs40260/32576 use
UDP64291/64292; final shared hash311581103f35b35a.

Directly inspected native shot-prefix states: P1 Lancer9 targets opposing
Strider13 at6551/6552 (150->144HP), and P0 Lancer10 targets opposing Lancer9 at
6583/6584 (38->32HP). Both cooldowns0->10, stationary shot positions and pending
queues persist. This confirms genuine opposing-combat-unit shot witnesses rather
than workers being damaged. Also compared both roles' six-tick offline traces
between Debug/Release; both restored localplayer0 and detached networking after
the intentionally retained terminal state. The automated auditor additionally
checks original-tail resumption and accepted offline movement/native prefixes.

Fresh package PCK19ff39b3293378323922dca5b8bb01fc1cb82d98946606e85eaf5ce98d286f9c,
DLL94affa5e129213ecd200703f6a678209b019367394c7e3bc08896724d3250c9d. This is the
existing unequal scripted contested economy route, not coordinated network
rematching, competitive1v1, human play or balance acceptance. Different terminal
tick from prior recordings is bound to this run's exact canonical command stream;
no historical endpoint was substituted. No shipping or AAA approval.

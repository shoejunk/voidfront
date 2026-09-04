# Independent network review - 2026-09-03

Reviewer: independent agent `independent_network_review`. Scope: source inspection of standalone simulation, closed tick frames, UDP evidence peer, VFR2 loader/content fingerprint, and impairment/replay harness. Primary owns builds, executions and integration. No shipping approval.

## Source findings and disposition

1. **P1, asymmetric ACK loss/reordering caused false protocol failure.** Initial peer rejected a next-tick frame while still awaiting its own prior-tick ACK, despite the other peer legitimately having advanced. An early Finish had the same final-tick race. Reported to developer; source now buffers one future frame and early terminal hash. This is the correct bounded stop-and-wait window, but actual impaired executions are needed to establish the fix.
2. **P2, Hello echo loop.** Initial handler replied to every Hello, producing indefinite handshake traffic. Source now replies only to the first received Hello; a normal frame can establish compatibility if the Hello reply is lost.
3. **P2, failed-run recording was not a playable diagnostic prefix.** Initial exception paths left frame count zero and full target ticks despite appended command data. Source now patches executed tick and command count and checks flush/close on normal success and exception cleanup. Runtime comparison of timeout/desync prefixes is still required. A zero-tick prejoin failure deliberately has no playable match prefix.

## Deterministic and defensive design assessment

- Executed-state hashes cover deterministic simulation state and executed sequence counters, excluding pending network arrival state. Closed frame receipt cannot mutate authoritative state; advancement copies the simulation and commits only when both frames and every command validate.
- Frame decoding bounds lengths and counts, rejects trailing bytes/noncanonical order, preserves output on failure, validates ownership and bounds future buffering. Explicit little-endian integers avoid padding and host byte order. Simulation sources use integers and fixed ticks without Godot or transport clocks.
- Transport uses explicit bounded envelopes, session/player/setup/protocol/content checks and configured loopback source endpoint. It is an unauthenticated local evidence harness, not a production Internet protocol. Malformed traffic is fail-closed; bounded draining prevents unlimited receive work per timer check.
- CMake fingerprints authoritative source/header content with line-ending normalization, and VFR2 loading rejects incompatible content and malformed/trailing input. Future external authoritative data must also join this fingerprint when introduced.
- Timeout is intentionally lack of authoritative advancement, not arbitrary packet silence. Duplicate traffic must not extend a stalled turn. Terminal hashes are checked and Finish/FinishAck are retried with a full timeout of response availability after local confirmation. This handles bounded packet loss; it does not guarantee both participants make the same completion decision under a persistent one-way partition.

## Executed evidence

Pending primary build and run artifacts. Source inspection alone is not separate-process networking, gameplay, responsiveness or performance proof. This section must be replaced with actual inspected results before the milestone is accepted.

## Remaining SPEC gates

Loopback automated combat, even with passing packet impairment and replay tests, cannot establish networked Godot play, complete economic 1v1/1vAI matches, input response at 80 ms RTT, a 30-minute soak, Internet behavior, scalable dynamic crowd pathfinding, reference-hardware performance, production art/audio/UI, balance or AAA/SC2 parity. No new visual changes were reviewed and no new visual quality claim is made.

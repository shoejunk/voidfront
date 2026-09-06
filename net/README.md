# Lockstep transport milestone

`voidfront_peer.exe` is a Windows loopback UDP evidence harness. Two separate
processes each generate only their own player's skirmish AI inputs. They exchange
scheduled tick frames, validate separately tagged executed-state hashes, and advance the same
standalone simulation. Godot client networking is not connected yet.

Build/test from the repository root with `./tools/verify.ps1 -Network`, or run
`python tools/verify_network.py` after building both configurations. Evidence is
written to a fresh timestamped directory under `artifacts/network/`, including
`run.json`, `summary.json` and per-case logs, peer reports,
canonical input recordings and tick/hash traces. The harness checks distinct
peer PIDs, compares every executed tick across configurations/impairments, replays
each recording in the opposite configuration, and runs ten repeated playbacks.
The relay uses real UDP sockets, not calls into either peer simulation. Seeded
per-direction random streams choose loss, jitter and duplication; actual packet
counts and relay delays are recorded. OS scheduling means exact packet histories
are not reproducible; canonical applied inputs and resulting state traces are.
Each run records UTC times and SHA256 fingerprints of both peer and replay
executables. An explicit `--out` must name a nonexistent directory, so failed
reruns cannot inherit an old successful report. Case failures print immediately
while other bounded workers finish. The relay counts and tolerates Windows UDP
connection-reset notifications from endpoints that have not bound or have closed;
all other socket errors remain fatal and peers retain progress timeouts.

The transport checks protocol, source content identity, session, player assignment,
seed, unit count and requested tick count in every datagram. The handshake also
requires identical input delay. It binds exclusively
to `127.0.0.1`, accepts only its configured endpoint, retries unacknowledged frames,
and pipelines future input without waiting for receipt ACKs to execute a turn.
Missing frames stall authoritative advancement. No-advancement timeouts report the current
tick; duplicate traffic cannot keep a stalled match alive indefinitely. Each peer
confirms every executed checksum, compares and acknowledges terminal state hashes,
and lingers for a timeout interval
to answer retransmitted terminal packets. Finite packet loss can still cause a
disconnect; successful local termination cannot guarantee the remote peer received
every final ACK under an indefinite one-way partition.

The core frame buffer in `sim/lockstep.*` supports 64 future ticks and 16 commands
per player/frame. Transport protocol 2 removes the impossible requirement for a
future input to contain its preceding future state hash. Kinds 6/7 carry separate
executed-state checksums and acknowledgements (u32 state tick, u64 hash). An input
receipt ACK confirms bytes received; a checksum ACK confirms compared execution.
The verification window permits at most 16 executed ticks beyond contiguous
mutually confirmed state. Missing checksums eventually stop execution even while
future inputs arrive. This detects divergence with bounded delay rather than
requiring a round trip before every step.

`--input-delay-ticks` schedules commands sampled from deterministic state tick t
for command tick t+delay (1..16, both peers must agree). The initial delay frames
are empty. Samples are taken once per source tick and never depend on packet
arrival order. Transport pacing targets one step per 50 ms; clocks and timing
samples remain outside `sim/`. Changing delay changes the canonical input stream,
so cross-impairment trace comparisons use the same delay. Current suite uses 2.

This is protocol and generated-command timing evidence, not human input-response,
playable multiplayer, reference hardware performance, LAN/Internet transport or
a 30-minute complete-match soak. A nonblocking client session adapter, match setup
and UI remain integration work. Authentication, NAT traversal, reconnect,
resynchronization, packet fragmentation and hostile remote networking are absent.

Manual paired process invocation (separate terminals):

```powershell
./build/windows/Debug/voidfront_peer.exe --player 0 --port 42000 --remote-port 42001 --session 42 --ticks 1000 --trace artifacts/p0.trace --record artifacts/p0.vfr --report artifacts/p0.json
./build/windows/Release/voidfront_peer.exe --player 1 --port 42001 --remote-port 42000 --session 42 --ticks 1000 --trace artifacts/p1.trace --record artifacts/p1.vfr --report artifacts/p1.json
```

The automated relay additionally tests 0/80/160 ms configured RTT, up to 20 ms
one-way jitter, 1% random loss, duplication, deliberately dropped ACK/terminal
packets, withheld frames/checksums, incompatible protocol/content/input delay,
desync and disconnect. Lagged verification can let peers end at different bounded
ticks after a fault; each applied prefix is replayed and their common prefix must
agree. Desync diagnostics identify the mismatched executed-state tick.
Use `--ticks` and `--jobs` on `verify_network.py` to control duration/concurrency.
Concurrent cases are correctness tests; elapsed times are not isolated benchmarks.
Peer reports now distinguish missing-data stalls from scheduled waits, expose
tick timestamps and command generation/application events, and record bounded
backlog peaks. The verifier recomputes rates/percentiles from raw samples and
checks source-to-execution tick mapping. Pacing excludes startup and terminal
linger. Command timing starts at generated canonical input; sparse tick-aligned
AI commands omit the human polling phase, client dispatch and presentation.
`summary.json` records strict timing-target booleans separately from protocol
success; a passing protocol suite does not silently accept a missed budget.
Use `--jobs 1` to measure cases without other network cases running concurrently.
The withheld-frame fixture watches executed checksum packets to reject advancement
beyond the held turn; future input transmission is expected during a stall.

Network recordings use VFR2: the VFR1 24-byte header with container byte 2 followed
by an 8-byte little-endian authoritative source content ID, then the same canonical
commands. The ID is the leading 64 bits of a SHA256 over normalized authoritative
source/header contents, generated during CMake configuration. It is a compatibility
tag, not a security/authentication mechanism. Whitespace-only source edits also
invalidate compatibility. Failed sessions finalize their applied input prefix;
zero-tick failures are intentionally not playable replays. Replaying records with
`voidfront_headless --replay ... --hash-mode state --trace ...` compares executed
state independently of future input receipts. Legacy VFR1 files retain their
protocol-only compatibility and default full-hash behavior.

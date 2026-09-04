# Lockstep transport milestone

`voidfront_peer.exe` is a Windows loopback UDP evidence harness. Two separate
processes each generate only their own player's skirmish AI inputs. They exchange
closed tick frames, validate prior executed-state hashes, and advance the same
standalone simulation. Godot client networking is not connected yet.

Build/test from the repository root with `./tools/verify.ps1 -Network`, or run
`python tools/verify_network.py` after building both configurations. Evidence is
written to `artifacts/network/summary.json` and per-case logs, peer reports,
canonical input recordings and tick/hash traces. The harness checks distinct
peer PIDs, compares every executed tick across configurations/impairments, replays
each recording in the opposite configuration, and runs ten repeated playbacks.
The relay uses real UDP sockets, not calls into either peer simulation. Seeded
per-direction random streams choose loss, jitter and duplication; actual packet
counts and relay delays are recorded. OS scheduling means exact packet histories
are not reproducible; canonical applied inputs and resulting state traces are.

The transport checks protocol, source content identity, session, player assignment,
seed, unit count and requested tick count in every datagram. It binds exclusively
to `127.0.0.1`, accepts only its configured endpoint, retries unacknowledged frames,
and buffers one overtaking frame while the previous ACK is in flight. Missing
frames stall authoritative advancement. No-advancement timeouts report the current
tick; duplicate traffic cannot keep a stalled match alive indefinitely. Each peer
compares and acknowledges terminal state hashes and lingers for a timeout interval
to answer retransmitted terminal packets. Finite packet loss can still cause a
disconnect; successful local termination cannot guarantee the remote peer received
every final ACK under an indefinite one-way partition.

The core frame buffer in `sim/lockstep.*` supports 64 future ticks and 16 commands
per player/frame. The initial harness uses one outstanding tick and no real-time
pacing. At significant RTT this cannot sustain the specified 20 Hz match rate.
It is protocol evidence, not input-response, playable multiplayer, reference
hardware performance, LAN/Internet transport or a 30-minute complete-match soak.
Pipelined scheduled input, a nonblocking client session adapter, match setup and
UI remain the next integration work. Authentication, NAT traversal, reconnect,
resynchronization, packet fragmentation and hostile remote networking are absent.

Manual paired process invocation (separate terminals):

```powershell
./build/windows/Debug/voidfront_peer.exe --player 0 --port 42000 --remote-port 42001 --session 42 --ticks 1000 --trace artifacts/p0.trace --record artifacts/p0.vfr --report artifacts/p0.json
./build/windows/Release/voidfront_peer.exe --player 1 --port 42001 --remote-port 42000 --session 42 --ticks 1000 --trace artifacts/p1.trace --record artifacts/p1.vfr --report artifacts/p1.json
```

The automated relay additionally tests 0/80/160 ms configured RTT, up to 20 ms
one-way jitter, 1% random loss, duplication, deliberately dropped ACK/terminal
packets, a withheld frame, incompatible protocol/content, desync and disconnect.
Use `--ticks` and `--jobs` on `verify_network.py` to control duration/concurrency.
Concurrent cases are correctness tests; elapsed times are not isolated benchmarks.

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

"""Exercise independent Windows UDP peers through a measured impairment relay.

All impairment and clocks live outside authoritative simulation. This is a
loopback protocol regression, not an Internet, input-latency or soak benchmark.
"""
import argparse
import concurrent.futures
from datetime import datetime, timezone
import hashlib
import heapq
import json
import os
from pathlib import Path
import random
import selectors
import socket
import subprocess
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
HIDDEN = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0


def allowed():
    if (ROOT / ".voidfront-agent/STOP").exists():
        raise RuntimeError("Voidfront STOP switch is present")


def read_trace(path, ticks):
    lines = path.read_text().splitlines()
    if len(lines) != ticks:
        raise AssertionError(f"{path}: {len(lines)} trace entries, expected {ticks}")
    for tick, line in enumerate(lines, 1):
        values = line.split()
        if len(values) != 2 or int(values[0]) != tick:
            raise AssertionError(f"{path}: invalid trace at tick {tick}")
        int(values[1])
    return lines


def compare(a, b, label):
    if a != b:
        for tick, (left, right) in enumerate(zip(a, b), 1):
            if left != right:
                raise AssertionError(f"{label}: first divergence at tick {tick}: {left} != {right}")
        raise AssertionError(f"{label}: trace lengths differ")


def replay(executable, record, trace):
    allowed()
    result = subprocess.run([str(executable), "--replay", str(record), "--hash-mode", "state",
                             "--trace", str(trace)], capture_output=True, text=True,
                            timeout=60, creationflags=HIDDEN)
    trace.with_suffix(".log").write_text(result.stdout + result.stderr)
    if result.returncode:
        raise AssertionError(f"replay rejected {record}: {result.stderr}")


def run_case(case, build, out, ticks):
    allowed()
    name, configs, rtt, jitter, loss, fault = case
    positive = fault in (None, "terminal-loss", "delayed-frame", "lost-ack")
    if fault in ("terminal-loss", "delayed-frame", "lost-ack"):
        ticks = min(ticks, 100)
    folder = out / name
    folder.mkdir(parents=True, exist_ok=True)
    peers = [build / cfg / "voidfront_peer.exe" for cfg in configs]
    selector = selectors.DefaultSelector()
    relays, reservations, children, logs = [], [], [], []
    queue, sequence = [], 0
    rng = [random.Random(73291), random.Random(19087)]
    counts = {"received": [0, 0], "dropped": [0, 0], "forwarded": [0, 0],
              "duplicates": [0, 0], "delays_ms": [[], []], "forced_drops": [0, 0],
              "connection_resets": [0, 0]}
    terminal_seen = [[0, 0], [0, 0]]
    held_since = None
    start = time.monotonic()
    try:
        for player in range(2):
            relay = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            relay.bind(("127.0.0.1", 0))
            relay.setblocking(False)
            relays.append(relay)
            selector.register(relay, selectors.EVENT_READ, player)
            reserve = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            reserve.bind(("127.0.0.1", 0))
            reservations.append(reserve)
        ports = [s.getsockname()[1] for s in reservations]
        for s in reservations:
            s.close()
        # Each peer sends to its relay endpoint; relay forwards from the other
        # endpoint so normal peer source-address checks stay enabled.
        for player in range(2):
            prefix = folder / f"peer{player}"
            args = [str(peers[player]), "--player", str(player), "--port", str(ports[player]),
                    "--remote-port", str(relays[player].getsockname()[1]), "--session", "64040903",
                    "--ticks", str(ticks), "--seed", "42", "--units-per-team", "6",
                    "--timeout-ms", "2000", "--trace", str(prefix.with_suffix(".trace")),
                    "--record", str(prefix.with_suffix(".vfr")), "--report", str(prefix.with_suffix(".json"))]
            if player == 1:
                if fault == "content":
                    args += ["--content-id", "1"]
                elif fault == "protocol":
                    args += ["--protocol-version", "999"]
                elif fault == "desync":
                    args += ["--desync-tick", "25"]
                elif fault == "disconnect":
                    args += ["--exit-at-tick", "25"]
            log = prefix.with_suffix(".log").open("w")
            logs.append(log)
            allowed()
            children.append(subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT,
                                             creationflags=HIDDEN))
        deadline = start + max(30, ticks * (rtt + 50) / 1000 * 3)
        while any(p.poll() is None for p in children):
            allowed()
            now = time.monotonic()
            if now > deadline:
                raise TimeoutError(f"{name}: owned peer watchdog expired")
            for key, _ in selector.select(0.002):
                player = key.data
                while True:
                    try:
                        data, address = key.fileobj.recvfrom(65536)
                    except BlockingIOError:
                        break
                    except ConnectionResetError:
                        # Windows UDP may report ICMP from a peer that has not
                        # bound yet or has already exited. This is not a packet;
                        # peer progress timeouts still detect real disconnects.
                        counts["connection_resets"][player] += 1
                        break
                    if address != ("127.0.0.1", ports[player]):
                        raise AssertionError("unexpected sender at isolated relay")
                    counts["received"][player] += 1
                    kind = int.from_bytes(data[40:44], "little")
                    frame_tick = int.from_bytes(data[60:64], "little") if kind == 2 else None
                    forced = False
                    if fault == "terminal-loss" and kind in (4, 5):
                        terminal_seen[player][kind - 4] += 1
                        forced = terminal_seen[player][kind - 4] <= 3
                    elif fault == "lost-ack" and kind == 3 and player == 1:
                        ack_tick = int.from_bytes(data[44:48], "little")
                        # Withhold ACKs long enough for the other peer's next
                        # frame to overtake them, then permit retry recovery.
                        forced = ack_tick == 25 and counts["forced_drops"][player] < 3
                    elif fault == "delayed-frame" and kind == 2:
                        if player == 1 and frame_tick == 25:
                            held_since = held_since or now
                            forced = now - held_since < .5
                        if player == 0 and frame_tick > 25 and held_since and now - held_since < .5:
                            raise AssertionError("peer advanced while the remote tick frame was withheld")
                    if forced:
                        counts["forced_drops"][player] += 1
                        continue
                    if rng[player].random() < loss:
                        counts["dropped"][player] += 1
                        continue
                    delay = max(0, rtt / 2 + rng[player].uniform(-jitter, jitter)) / 1000
                    copies = 2 if rng[player].random() < 0.03 else 1
                    counts["duplicates"][player] += copies - 1
                    for copy in range(copies):
                        sequence += 1
                        due = now + delay + copy * 0.003
                        heapq.heappush(queue, (due, sequence, player, data, now))
            now = time.monotonic()
            while queue and queue[0][0] <= now:
                _, _, source, data, enqueued = heapq.heappop(queue)
                destination = 1 - source
                relays[destination].sendto(data, ("127.0.0.1", ports[destination]))
                counts["forwarded"][source] += 1
                counts["delays_ms"][source].append((now - enqueued) * 1000)
        for log in logs:
            log.flush()
        reports = [json.loads((folder / f"peer{p}.json").read_text()) for p in range(2)]
        codes = [p.returncode for p in children]
        if positive:
            if codes != [0, 0] or any(r["status"] != "complete" for r in reports):
                raise AssertionError(f"{name}: peers did not complete: {codes} {reports}")
            traces = [read_trace(folder / f"peer{p}.trace", ticks) for p in range(2)]
            compare(*traces, f"{name}: communicating peers")
            for player in range(2):
                target = folder / f"replayed{player}.trace"
                # Replay each process recording in the opposite configuration.
                executable = build / "sim" / configs[1 - player] / "voidfront_headless.exe"
                replay(executable, folder / f"peer{player}.vfr", target)
                compare(traces[player], read_trace(target, ticks), f"{name}: replay {player}")
            if loss and sum(counts["dropped"]) == 0:
                raise AssertionError(f"{name}: configured loss never exercised")
            if traces[0] == []:
                raise AssertionError("empty network trace")
            if fault and sum(counts["forced_drops"]) == 0:
                raise AssertionError(f"{name}: required forced impairment never exercised")
            if fault == "terminal-loss" and sum(counts["forced_drops"]) != 12:
                raise AssertionError("did not force-drop all terminal packet kinds in both directions")
            if fault == "delayed-frame" and min(r["stall_ms"] for r in reports) < 500:
                raise AssertionError("withheld turn did not produce measured stalls")
        else:
            if any(code == 0 for code in codes):
                raise AssertionError(f"{name}: fault incorrectly succeeded: {codes}")
            diagnostic = " ".join((folder / f"peer{p}.log").read_text() for p in range(2)).lower()
            expected = {"content": "incompat", "protocol": "incompat", "desync": "desync",
                        "disconnect": "timeout"}[fault]
            if expected not in diagnostic:
                raise AssertionError(f"{name}: missing {expected} diagnostic: {diagnostic}")
            for p in range(2):
                executed = (folder / f"peer{p}.trace").read_text().splitlines()
                expected_tick = 0 if fault in ("content", "protocol") else 25
                if len(executed) != expected_tick or reports[p]["ticks"] != expected_tick:
                    raise AssertionError(f"{name}: expected stop at {expected_tick}, got {reports[p]}")
                if expected_tick:
                    target = folder / f"prefix-replayed{p}.trace"
                    executable = build / "sim" / configs[1 - p] / "voidfront_headless.exe"
                    replay(executable, folder / f"peer{p}.vfr", target)
                    compare(executed, read_trace(target, expected_tick), f"{name}: failure prefix {p}")
        delays = counts.pop("delays_ms")
        counts["delay_ms"] = []
        for values in delays:
            values.sort()
            counts["delay_ms"].append({"min": min(values, default=0), "max": max(values, default=0),
                                       "p50": values[len(values) // 2] if values else 0})
        result = {"case": name, "ticks": ticks, "configurations": configs, "rtt_ms": rtt,
                  "one_way_jitter_ms": jitter, "loss_probability": loss,
                  "elapsed_seconds": time.monotonic() - start, "pids": [p.pid for p in children],
                  "relay": counts, "peers": reports, "verified": True}
        (folder / "result.json").write_text(json.dumps(result, indent=2))
        print(f"PASS {name}: separate PIDs {result['pids']}, {result['elapsed_seconds']:.2f}s, "
              f"dropped={counts['dropped']}", flush=True)
        return result
    except Exception as error:
        print(f"FAIL {name}: {error!r}", flush=True)
        raise
    finally:
        for p in children:
            if p.poll() is None:
                p.kill()
            p.wait(timeout=10)
        for log in logs:
            log.close()
        selector.close()
        for s in relays + reservations:
            s.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=ROOT / "build/windows")
    parser.add_argument("--out", type=Path, help="new evidence directory (must not exist)")
    parser.add_argument("--ticks", type=int, default=1000)
    parser.add_argument("--jobs", type=int, choices=range(1, 4), default=3)
    args = parser.parse_args()
    if not 50 <= args.ticks <= 100000:
        parser.error("ticks must be 50..100000")
    allowed()
    started = datetime.now(timezone.utc)
    if args.out is None:
        args.out = ROOT / "artifacts/network" / (started.strftime("%Y%m%dT%H%M%SZ-") + uuid.uuid4().hex[:8])
    args.build, args.out = args.build.resolve(), args.out.resolve()
    args.out.mkdir(parents=True, exist_ok=False)
    print(f"Network evidence: {args.out}", flush=True)
    binaries = [args.build / cfg / "voidfront_peer.exe" for cfg in ("Debug", "Release")]
    binaries += [args.build / "sim" / cfg / "voidfront_headless.exe" for cfg in ("Debug", "Release")]
    fingerprints = {str(p.relative_to(args.build)): hashlib.sha256(p.read_bytes()).hexdigest() for p in binaries}
    (args.out / "run.json").write_text(json.dumps({"started_utc": started.isoformat(),
                                                 "binary_sha256": fingerprints}, indent=2))
    cases = [
        ("debug-clean", ("Debug", "Debug"), 0, 0, 0, None),
        ("release-clean", ("Release", "Release"), 0, 0, 0, None),
        ("mixed-80ms", ("Debug", "Release"), 80, 20, .01, None),
        ("mixed-160ms", ("Release", "Debug"), 160, 20, .01, None),
        ("terminal-loss", ("Debug", "Release"), 80, 20, 0, "terminal-loss"),
        ("withheld-frame", ("Debug", "Release"), 0, 0, 0, "delayed-frame"),
        ("lost-tick-ack", ("Debug", "Release"), 0, 0, 0, "lost-ack"),
        ("reject-content", ("Debug", "Release"), 0, 0, 0, "content"),
        ("reject-protocol", ("Debug", "Release"), 0, 0, 0, "protocol"),
        ("detect-desync", ("Debug", "Release"), 0, 0, 0, "desync"),
        ("detect-disconnect", ("Debug", "Release"), 0, 0, 0, "disconnect"),
    ]
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = [pool.submit(run_case, c, args.build, args.out, args.ticks) for c in cases]
        results = [future.result() for future in futures]
    baseline = read_trace(args.out / "release-clean/peer0.trace", args.ticks)
    for case in cases[:4]:
        compare(baseline, read_trace(args.out / case[0] / "peer0.trace", args.ticks),
                f"all configurations/impairments vs {case[0]}")
    replay_exe = args.build / "sim/Release/voidfront_headless.exe"
    record = args.out / "release-clean/peer0.vfr"
    for repeat in range(10):
        target = args.out / f"repeat-{repeat}.trace"
        replay(replay_exe, record, target)
        compare(baseline, read_trace(target, args.ticks), f"repeat {repeat}")
    # VFR2 compatibility metadata and truncation are not advisory.
    golden = record.read_bytes()
    if golden[:4] != b"VFR\x02":
        raise AssertionError("network recording is missing content identity")
    malformed = {"content": golden[:24] + bytes([golden[24] ^ 1]) + golden[25:],
                 "short-content": golden[:31], "trailing": golden + b"\0"}
    for name, data in malformed.items():
        path = args.out / f"invalid-{name}.vfr"
        path.write_bytes(data)
        test = subprocess.run([str(replay_exe), "--replay", str(path)], capture_output=True,
                              text=True, timeout=15, creationflags=HIDDEN)
        if test.returncode == 0:
            raise AssertionError(f"invalid network replay accepted: {name}")
    summary = {"verified": True, "started_utc": started.isoformat(),
               "completed_utc": datetime.now(timezone.utc).isoformat(), "binary_sha256": fingerprints,
               "ticks_per_successful_case": args.ticks, "cases": results,
               "ten_replays_equal": True, "malformed_replays_rejected": list(malformed),
               "trace_sha256": hashlib.sha256((args.out / "release-clean/peer0.trace").read_bytes()).hexdigest(),
               "scope": "loopback UDP headless skirmish, accelerated ticks, no client network or 30-minute soak"}
    (args.out / "summary.json").write_text(json.dumps(summary, indent=2))
    print(f"PASS network suite: {len(cases)} cases, cross-configuration traces and ten replays agree", flush=True)


if __name__ == "__main__":
    main()

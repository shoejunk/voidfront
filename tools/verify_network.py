"""Exercise independent Windows UDP peers through a measured impairment relay.

All impairment and clocks live outside authoritative simulation. This is a
loopback protocol and paced command-execution regression. Command timing starts
at generated canonical input, not human input or presentation; no soak claim.
"""
import argparse
import concurrent.futures
from dataclasses import dataclass
from datetime import datetime, timezone
import hashlib
import heapq
import json
import math
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


@dataclass(frozen=True)
class Case:
    name: str
    configs: tuple
    rtt: int
    jitter: int
    loss: float
    fault: str | None = None
    input_delay: int = 2


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


def verify_timing(report):
    """Recompute metrics from measured events; never infer latency from ticks."""
    def close(actual, expected, label):
        if not math.isfinite(actual) or abs(actual - expected) > .01:
            raise AssertionError(f"{label}: {actual} != {expected}")

    times = report["tick_times_ms"]
    intervals = report["tick_interval_samples_ms"]
    deadlines = report["tick_deadline_samples_ms"]
    lateness = report["tick_lateness_samples_ms"]
    latency = report["command_latency_samples_ms"]
    commands = report["command_timings"]
    ready = report["session_ready_ms"]
    if ready is None:
        if times or commands or report["startup_duration_ms"] is not None:
            raise AssertionError("session executed or sampled input before readiness")
    else:
        close(report["startup_duration_ms"], ready, "visible startup duration")
        if ready < 0 or (times and times[0] < ready):
            raise AssertionError("execution precedes initial buffer readiness")
    if len(times) != report["ticks"] or len(intervals) != max(0, len(times) - 1):
        raise AssertionError("missing per-tick timing observations")
    if len(deadlines) != len(times) or len(lateness) != len(times) or report["timer_period_ms"] != 1:
        raise AssertionError("missing paced deadlines or timer resolution")
    for actual, due, late in zip(times, deadlines, lateness):
        close(late, actual - due, "tick deadline lateness")
        if late < 0:
            raise AssertionError("tick executed before its pacing deadline")
    if len(latency) != report["command_count"] or len(commands) != len(latency):
        raise AssertionError("missing measured command events")
    for index, interval in enumerate(intervals):
        close(interval, times[index + 1] - times[index], "tick interval")
        if interval <= 0:
            raise AssertionError("tick timestamps are not increasing")
    if len(times) > 1:
        elapsed = times[-1] - times[0]
        close(report["pacing_elapsed_ms"], elapsed, "paced duration")
        close(report["pacing_hz"], (len(times) - 1) * 1000 / elapsed, "paced rate")
    for event, measured in zip(commands, latency):
        if event["execution_tick"] != event["source_tick"] + report["input_delay_ticks"]:
            raise AssertionError("command execution differs from scheduled source tick")
        if not 0 <= event["source_tick"] < event["execution_tick"] < len(times):
            raise AssertionError("command timing refers to an unexecuted tick")
        close(event["executed_ms"], times[event["execution_tick"]], "command application time")
        close(event["latency_ms"], event["executed_ms"] - event["generated_ms"], "command elapsed time")
        close(measured, event["latency_ms"], "command sample")
        if measured < 0 or event["generated_ms"] > times[event["source_tick"]]:
            raise AssertionError("invalid command generation timestamp")
        if ready is None or event["generated_ms"] < ready:
            raise AssertionError("canonical input accepted before session readiness")
    for prefix, values in (("command_latency", latency), ("tick_interval", intervals)):
        ordered = sorted(values)
        for suffix, percentile in (("p95", .95), ("p99", .99), ("max", 1)):
            expected = ordered[math.ceil(len(ordered) * percentile) - 1] if ordered else 0
            close(report[f"{prefix}_{suffix}_ms"], expected, f"{prefix} {suffix}")


def verify_command_coverage(report, recording, player):
    """Every applied local command must have one unfiltered timing event."""
    data = recording.read_bytes()
    if len(data) < 32 or data[:4] != b"VFR\x02":
        raise AssertionError("missing VFR2 command coverage evidence")
    u32 = lambda offset: int.from_bytes(data[offset:offset + 4], "little")
    if u32(16) != report["ticks"]:
        raise AssertionError("recorded prefix differs from timed execution")
    expected, offset = [], 32
    for _ in range(u32(20)):
        if offset + 4 > len(data):
            raise AssertionError("truncated recorded command size")
        size = u32(offset)
        offset += 4
        if size < 26 or offset + size > len(data) or data[offset:offset + 4] != b"VFC\x01":
            raise AssertionError("invalid recorded command for timing coverage")
        tick, sequence, owner = u32(offset + 4), u32(offset + 8), data[offset + 12]
        if tick >= report["ticks"] or owner not in (0, 1):
            raise AssertionError("recorded command outside applied prefix")
        if owner == player:
            expected.append((tick, sequence))
        offset += size
    if offset != len(data):
        raise AssertionError("trailing command coverage data")
    actual = [(event["execution_tick"], event["sequence"]) for event in report["command_timings"]]
    if actual != expected:
        raise AssertionError(f"timing events omit, duplicate or reorder applied player {player} commands")


def run_case(case, build, out, ticks):
    allowed()
    name, configs, rtt, jitter, loss, fault = (
        case.name, case.configs, case.rtt, case.jitter, case.loss, case.fault)
    input_delay = case.input_delay
    positive = fault in (None, "terminal-loss", "delayed-frame", "lost-ack",
                         "delayed-checksum", "lost-checksum-ack", "startup-ack")
    if positive and fault:
        ticks = min(ticks, 100)
    if fault == "desync-final":
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
    execution_during_hold = []
    held_ack_released = False
    executed_before_held_ack = False
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
                    "--input-delay-ticks", str(input_delay),
                    "--timeout-ms", "2000", "--trace", str(prefix.with_suffix(".trace")),
                    "--record", str(prefix.with_suffix(".vfr")), "--report", str(prefix.with_suffix(".json"))]
            if player == 1:
                if fault == "content":
                    args += ["--content-id", "1"]
                elif fault == "protocol":
                    args += ["--protocol-version", "999"]
                elif fault == "desync":
                    args += ["--desync-tick", "25"]
                elif fault == "desync-final":
                    args += ["--desync-tick", str(ticks)]
                elif fault == "disconnect":
                    args += ["--exit-at-tick", "25"]
                elif fault == "input-delay":
                    args[args.index("--input-delay-ticks") + 1] = str(input_delay % 16 + 1)
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
                    state_tick = int.from_bytes(data[44:48], "little") if kind == 6 else None
                    if fault == "lost-ack" and player == 0 and state_tick is not None and state_tick >= 26 and not held_ack_released:
                        executed_before_held_ack = True
                    forced = False
                    if fault == "terminal-loss" and kind in (4, 5):
                        terminal_seen[player][kind - 4] += 1
                        forced = terminal_seen[player][kind - 4] <= 3
                    elif fault == "startup-ack":
                        if kind == 3 and player == 1 and int.from_bytes(data[44:48], "little") == 0:
                            held_since = held_since or now
                            forced = now - held_since < .5
                        if player == 0 and held_since and now - held_since < .5:
                            if state_tick is not None or (frame_tick is not None and frame_tick >= input_delay):
                                raise AssertionError("peer sampled or executed input before initial ACK readiness")
                    elif fault == "lost-ack" and kind == 3 and player == 1:
                        ack_tick = int.from_bytes(data[44:48], "little")
                        # Withhold ACKs long enough for the other peer's next
                        # frame to overtake them, then permit retry recovery.
                        forced = ack_tick == 25 and counts["forced_drops"][player] < 3
                        if ack_tick == 25 and not forced:
                            held_ack_released = True
                    elif fault == "delayed-frame":
                        if player == 1 and frame_tick == 25:
                            held_since = held_since or now
                            forced = now - held_since < .5
                        # Future inputs now intentionally precede execution.
                        # Executed-state checksums, not future Frame packets,
                        # establish whether the held authoritative turn ran.
                        if player == 0 and state_tick is not None and held_since and now - held_since < .5:
                            execution_during_hold.append(state_tick)
                        if player == 0 and state_tick is not None and state_tick > 25 and held_since and now - held_since < .5:
                            raise AssertionError("peer advanced while the remote tick frame was withheld")
                    elif fault == "delayed-checksum":
                        if player == 1 and state_tick == 25:
                            held_since = held_since or now
                            forced = now - held_since < 1.1
                        if player == 0 and state_tick is not None and held_since and now - held_since < 1.1:
                            execution_during_hold.append(state_tick)
                            if state_tick > 24 + 16:
                                raise AssertionError("peer exceeded verification lag while checksum was withheld")
                    elif fault == "lost-checksum-ack" and kind == 7 and player == 1:
                        ack_tick = int.from_bytes(data[44:48], "little")
                        forced = ack_tick == 25 and counts["forced_drops"][player] < 3
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
        for player, report in enumerate(reports):
            verify_timing(report)
            verify_command_coverage(report, folder / f"peer{player}.vfr", player)
        codes = [p.returncode for p in children]
        if positive:
            if codes != [0, 0] or any(r["status"] != "complete" for r in reports):
                raise AssertionError(f"{name}: peers did not complete: {codes} {reports}")
            for r in reports:
                if r["confirmed_ticks"] != ticks:
                    raise AssertionError(f"{name}: completed without confirming every executed checksum")
                if r["max_verification_lag_ticks"] > r["verification_lag_limit"]:
                    raise AssertionError(f"{name}: exceeded declared verification bound")
                if r["input_delay_ticks"] != input_delay or r["verification_lag_limit"] != 16:
                    raise AssertionError(f"{name}: unexpected scheduling/bound contract")
                if r["max_unacked_checksums"] > 16 or r["max_unacked_frames"] > 16 + input_delay + 1:
                    raise AssertionError(f"{name}: retry backlog exceeded the pipeline window")
                if r["command_count"] < 1 or r["command_latency_max_ms"] < r["command_latency_p95_ms"]:
                    raise AssertionError(f"{name}: missing or invalid command-response measurement")
                if r["command_timings"][0]["source_tick"] != 0 or r["command_timings"][0]["sequence"] != 1:
                    raise AssertionError(f"{name}: source-zero input timing was discarded")
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
            if fault == "delayed-frame" and reports[0]["stall_ms"] < 250:
                raise AssertionError("withheld turn did not produce measured stalls")
            if fault == "delayed-checksum" and (reports[0]["max_verification_lag_ticks"] != 16 or reports[0]["stall_ms"] < 150):
                raise AssertionError("withheld checksum did not exercise verification backpressure")
            if fault == "lost-ack" and (reports[0]["advanced_without_ack"] < 1 or not executed_before_held_ack):
                raise AssertionError("lost ACK fixture did not demonstrate ACK-independent execution")
            if fault == "startup-ack" and reports[0]["startup_duration_ms"] < 500:
                raise AssertionError("withheld initial ACK did not delay explicit readiness")
        else:
            if any(code == 0 for code in codes):
                raise AssertionError(f"{name}: fault incorrectly succeeded: {codes}")
            diagnostic = " ".join((folder / f"peer{p}.log").read_text() for p in range(2)).lower()
            expected = {"content": "incompat", "protocol": "incompat", "input-delay": "incompat", "desync": "desync", "desync-final": "desync",
                        "disconnect": "timeout"}[fault]
            if expected not in diagnostic:
                raise AssertionError(f"{name}: missing {expected} diagnostic: {diagnostic}")
            for p in range(2):
                executed = (folder / f"peer{p}.trace").read_text().splitlines()
                expected_tick = reports[p]["ticks"]
                if len(executed) != expected_tick:
                    raise AssertionError(f"{name}: report and applied trace disagree: {reports[p]}")
                if fault in ("content", "protocol", "input-delay") and expected_tick != 0:
                    raise AssertionError(f"{name}: incompatible session advanced")
                if fault == "desync" and not 25 <= expected_tick <= 25 + 16:
                    raise AssertionError(f"{name}: desync detection exceeded verification window: {reports[p]}")
                if fault == "desync-final" and expected_tick != ticks:
                    raise AssertionError(f"{name}: final checksum fault did not reach final state")
                if fault == "disconnect" and not 25 <= expected_tick <= 25 + input_delay:
                    raise AssertionError(f"{name}: disconnected peer exceeded scheduled input window: {reports[p]}")
                if expected_tick:
                    target = folder / f"prefix-replayed{p}.trace"
                    executable = build / "sim" / configs[1 - p] / "voidfront_headless.exe"
                    replay(executable, folder / f"peer{p}.vfr", target)
                    compare(executed, read_trace(target, expected_tick), f"{name}: failure prefix {p}")
            if fault == "desync" and not any(r["first_divergent_tick"] == 25 for r in reports):
                raise AssertionError("desync report did not identify the first mismatched state tick")
            if fault == "desync-final" and not any(r["first_divergent_tick"] == ticks for r in reports):
                raise AssertionError("desync report did not identify mismatched terminal state")
            if fault in ("desync", "desync-final", "disconnect"):
                prefixes = [(folder / f"peer{p}.trace").read_text().splitlines() for p in range(2)]
                shared = min(map(len, prefixes))
                compare(prefixes[0][:shared], prefixes[1][:shared], f"{name}: shared applied prefix")
        delays = counts.pop("delays_ms")
        counts["delay_ms"] = []
        for values in delays:
            values.sort()
            counts["delay_ms"].append({"min": min(values, default=0), "max": max(values, default=0),
                                       "p50": values[len(values) // 2] if values else 0})
        result = {"case": name, "ticks": ticks, "configurations": configs, "rtt_ms": rtt,
                  "input_delay_ticks": input_delay, "fault": fault,
                  "one_way_jitter_ms": jitter, "loss_probability": loss,
                  "elapsed_seconds": time.monotonic() - start, "pids": [p.pid for p in children],
                  "executed_state_ticks_observed_during_hold": sorted(set(execution_during_hold)),
                  "executed_before_held_input_ack": executed_before_held_ack,
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


def matched_delay_comparisons(results):
    """Observed deltas only; a faster profile does not waive a SPEC budget."""
    by_name = {r["case"]: r for r in results}
    pairs = (("mixed-clean-80ms-control", "mixed-80ms"),
             ("mixed-clean-delay2", "mixed-160ms"),
             ("mixed-clean-delay4", "mixed-160ms-delay4"))
    comparisons = []
    metrics = ("pacing_hz", "stall_count", "stall_ms", "command_latency_p95_ms",
               "startup_duration_ms", "tick_interval_p99_ms")
    for clean_name, impaired_name in pairs:
        clean, impaired = by_name[clean_name], by_name[impaired_name]
        for key in ("ticks", "configurations", "input_delay_ticks"):
            if clean[key] != impaired[key]:
                raise AssertionError(f"unmatched {key}: {clean_name} / {impaired_name}")
        if clean["rtt_ms"] or clean["one_way_jitter_ms"] or clean["loss_probability"] or clean["fault"]:
            raise AssertionError(f"invalid clean control: {clean_name}")
        comparisons.append({
            "baseline": clean_name, "impaired": impaired_name,
            "input_delay_ticks": clean["input_delay_ticks"],
            "configurations": clean["configurations"],
            "peers": [{metric: {"baseline": a[metric], "impaired": b[metric],
                                "impaired_minus_baseline": b[metric] - a[metric]}
                       for metric in metrics}
                      for a, b in zip(clean["peers"], impaired["peers"])]})
    return comparisons


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=ROOT / "build/windows")
    parser.add_argument("--out", type=Path, help="new evidence directory (must not exist)")
    parser.add_argument("--ticks", type=int, default=1000)
    parser.add_argument("--jobs", type=int, choices=range(1, 4), default=1)
    parser.add_argument("--suite", choices=("full", "delay-study"), default="full",
                        help="full fault regression or isolated matched input-delay profiles")
    args = parser.parse_args()
    if not 50 <= args.ticks <= 100000:
        parser.error("ticks must be 50..100000")
    if args.suite == "delay-study" and args.jobs != 1:
        parser.error("delay-study requires --jobs 1 for isolated timing")
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
    cases = [Case(*c) for c in [
        ("debug-clean", ("Debug", "Debug"), 0, 0, 0, None),
        ("release-clean", ("Release", "Release"), 0, 0, 0, None),
        ("mixed-80ms", ("Debug", "Release"), 80, 20, .01, None),
        ("mixed-160ms", ("Release", "Debug"), 160, 20, .01, None),
        ("terminal-loss", ("Debug", "Release"), 80, 20, 0, "terminal-loss"),
        ("withheld-frame", ("Debug", "Release"), 0, 0, 0, "delayed-frame"),
        ("lost-tick-ack", ("Debug", "Release"), 0, 0, 0, "lost-ack"),
        ("withheld-startup-ack", ("Debug", "Release"), 0, 0, 0, "startup-ack"),
        ("withheld-checksum", ("Debug", "Release"), 0, 0, 0, "delayed-checksum"),
        ("lost-checksum-ack", ("Debug", "Release"), 0, 0, 0, "lost-checksum-ack"),
        ("reject-content", ("Debug", "Release"), 0, 0, 0, "content"),
        ("reject-protocol", ("Debug", "Release"), 0, 0, 0, "protocol"),
        ("reject-input-delay", ("Debug", "Release"), 0, 0, 0, "input-delay"),
        ("detect-desync", ("Debug", "Release"), 0, 0, 0, "desync"),
        ("detect-terminal-desync", ("Debug", "Release"), 0, 0, 0, "desync-final"),
        ("detect-disconnect", ("Debug", "Release"), 0, 0, 0, "disconnect"),
    ]]
    # Delay changes AI sampling and hence canonical commands. Each impairment
    # needs the same delay AND player/build assignment in its clean control.
    cases += [Case("mixed-clean-delay2", ("Release", "Debug"), 0, 0, 0),
              Case("mixed-clean-delay4", ("Release", "Debug"), 0, 0, 0, input_delay=4),
              Case("mixed-160ms-delay4", ("Release", "Debug"), 160, 20, .01, input_delay=4),
              Case("mixed-clean-80ms-control", ("Debug", "Release"), 0, 0, 0),
              Case("withheld-checksum-delay4", ("Debug", "Release"), 0, 0, 0,
                   "delayed-checksum", input_delay=4),
              Case("detect-disconnect-delay4", ("Debug", "Release"), 0, 0, 0,
                   "disconnect", input_delay=4)]
    if args.suite == "delay-study":
        names = {"mixed-clean-delay2", "mixed-clean-delay4", "mixed-160ms-delay4",
                 "mixed-clean-80ms-control", "mixed-80ms", "mixed-160ms"}
        cases = [c for c in cases if c.name in names]
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = [pool.submit(run_case, c, args.build, args.out, args.ticks) for c in cases]
        results = [future.result() for future in futures]
    ordinary = [c for c in cases if c.fault is None]
    baselines = {}
    for case in ordinary:
        if case.rtt == 0:
            baselines.setdefault(case.input_delay, case.name)
    for case in ordinary:
        baseline = read_trace(args.out / baselines[case.input_delay] / "peer0.trace", args.ticks)
        compare(baseline, read_trace(args.out / case.name / "peer0.trace", args.ticks),
                f"same-delay configurations/impairments vs {case.name}")
    replay_exe = args.build / "sim/Release/voidfront_headless.exe"
    trace_hashes = {}
    for delay, name in baselines.items():
        record = args.out / name / "peer0.vfr"
        baseline_path = args.out / name / "peer0.trace"
        baseline = read_trace(baseline_path, args.ticks)
        trace_hashes[str(delay)] = hashlib.sha256(baseline_path.read_bytes()).hexdigest()
        for repeat in range(10):
            target = args.out / f"repeat-delay{delay}-{repeat}.trace"
            replay(replay_exe, record, target)
            compare(baseline, read_trace(target, args.ticks), f"delay {delay} repeat {repeat}")
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
               "suite": args.suite, "ticks_per_successful_case": args.ticks, "cases": results,
               "ten_replays_equal": True, "malformed_replays_rejected": list(malformed),
               "trace_sha256": trace_hashes["2"], "trace_sha256_by_input_delay": trace_hashes,
               "scope": "loopback UDP paced headless skirmish; generated canonical command timing, no human/client latency or 30-minute soak"}
    # Functional protocol success is distinct from the strict SPEC targets.
    # No rate tolerance silently turns a target miss into acceptance.
    summary["timing_assessment"] = {
        "isolated_cases": args.jobs == 1,
        "pacing_target_hz": 20,
        "command_p95_target_at_80ms_rtt_ms": 150,
        "cases": [{"case": r["case"],
                   "input_delay_ticks": r["input_delay_ticks"],
                   "pacing_hz": [p["pacing_hz"] for p in r["peers"]],
                   "at_least_20hz": all(p["pacing_hz"] >= 20 for p in r["peers"]),
                   "command_p95_ms": [p["command_latency_p95_ms"] for p in r["peers"]],
                   "tick_interval_min_ms": [min(p["tick_interval_samples_ms"]) for p in r["peers"]],
                   "tick_lateness_max_ms": [max(p["tick_lateness_samples_ms"]) for p in r["peers"]],
                   "startup_duration_ms": [p["startup_duration_ms"] for p in r["peers"]],
                   "generated_command_80ms_budget_met":
                       all(p["command_latency_p95_ms"] <= 150 for p in r["peers"]) if r["rtt_ms"] == 80 else None}
                  for r in results if r["fault"] is None],
        "limits": "Sparse tick-aligned skirmish AI commands; excludes human polling phase, client input, presentation and physical network. Strict target booleans do not include scheduling tolerance."}
    summary["matched_delay_comparisons"] = matched_delay_comparisons(results)
    (args.out / "summary.json").write_text(json.dumps(summary, indent=2))
    print(f"PASS network suite: {len(cases)} cases, cross-configuration traces and ten replays agree", flush=True)
    for assessment in summary["timing_assessment"]["cases"]:
        print(f"TIMING {assessment}", flush=True)


if __name__ == "__main__":
    main()

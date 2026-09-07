"""Run two packaged, rendered Godot clients over an isolated UDP relay.

Synthetic InputEvents measure software event handling, not physical human input.
Every executed trace is replayed in both MSVC configurations. Runs never reuse
evidence directories. No LAN, Internet, full-match or shipping claim is implied.
"""
import argparse
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

from verify_network import allowed, replay, read_trace, compare

ROOT = Path(__file__).resolve().parents[1]
HIDDEN = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def percentile(values, fraction=.95):
    return sorted(values)[math.ceil(len(values) * fraction) - 1] if values else None


def run_case(out, name, ticks, rtt=0, delay=2, fault=None):
    allowed()
    folder = out / name
    folder.mkdir()
    relays, children, logs = [], [], []
    selector = selectors.DefaultSelector()
    reserved = []
    queue, serial = [], 0
    rng = random.Random(409073)
    counts = dict(received=0, forwarded=0, dropped=0, forced=0, resets=0)
    held_since = None
    start = time.monotonic()
    try:
        for player in range(2):
            relay = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            relay.bind(("127.0.0.1", 0))
            relay.setblocking(False)
            relays.append(relay)
            selector.register(relay, selectors.EVENT_READ, player)
            reservation = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            reservation.bind(("127.0.0.1", 0))
            reserved.append(reservation)
        ports = [s.getsockname()[1] for s in reserved]
        for s in reserved:
            s.close()
        env = os.environ.copy()
        env["APPDATA"] = str(ROOT / "artifacts/godot-profile")
        for player in range(2):
            prefix = folder / f"peer{player}"
            args = [str(ROOT / "artifacts/package/Voidfront.exe"),
                    "--resolution", "1280x720", "--max-fps", "120",
                    "--log-file", str(prefix.with_suffix(".engine.log")), "--",
                    "--network", "--network-smoke", f"--player={player}",
                    f"--port={ports[player]}", f"--remote-port={relays[player].getsockname()[1]}",
                    "--session=409073", f"--delay={4 if fault == 'delay-mismatch' and player else delay}",
                    f"--ticks={ticks}", f"--report={prefix.with_suffix('.json')}",
                    f"--capture={prefix.with_suffix('.png')}"]
            log = prefix.with_suffix(".log").open("w")
            logs.append(log)
            allowed()
            startup = subprocess.STARTUPINFO()
            startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            startup.wShowWindow = 0
            children.append(subprocess.Popen(args, env=env, stdout=log,
                                             stderr=subprocess.STDOUT,
                                             startupinfo=startup, creationflags=HIDDEN))
        deadline = start + max(45, ticks / 20 + 30)
        while any(p.poll() is None for p in children):
            allowed()
            now = time.monotonic()
            if now > deadline:
                raise TimeoutError(f"{name}: owned client watchdog expired")
            for key, _ in selector.select(.002):
                player = key.data
                for _ in range(256):
                    try:
                        data, source = key.fileobj.recvfrom(65536)
                    except BlockingIOError:
                        break
                    except ConnectionResetError:
                        counts["resets"] += 1
                        break
                    if source != ("127.0.0.1", ports[player]):
                        raise AssertionError("Unexpected sender at isolated relay")
                    counts["received"] += 1
                    kind = int.from_bytes(data[40:44], "little")
                    frame = int.from_bytes(data[60:64], "little") if kind == 2 else None
                    state = int.from_bytes(data[44:48], "little") if kind == 6 else None
                    if fault == "disconnect" and state is not None and state >= 50:
                        held_since = held_since or now
                    forced = fault == "disconnect" and held_since is not None
                    if fault == "held-frame" and player == 1 and frame == 25:
                        held_since = held_since or now
                        forced = now - held_since < .6
                    if fault == "held-frame" and player == 0 and held_since and now - held_since < .6:
                        if state is not None and state > 25:
                            raise AssertionError("Client advanced past withheld canonical frame")
                    if forced:
                        counts["forced"] += 1
                        continue
                    if rtt and rng.random() < .01:
                        counts["dropped"] += 1
                        continue
                    latency = max(0, rtt / 2 + (rng.uniform(-20, 20) if rtt else 0)) / 1000
                    serial += 1
                    heapq.heappush(queue, (now + latency, serial, player, data))
            now = time.monotonic()
            while queue and queue[0][0] <= now:
                _, _, source, data = heapq.heappop(queue)
                destination = 1 - source
                relays[destination].sendto(data, ("127.0.0.1", ports[destination]))
                counts["forwarded"] += 1
        for log in logs:
            log.flush()
        reports = [json.loads((folder / f"peer{p}.json").read_text()) for p in range(2)]
        result = validate(folder, reports, ticks, fault, [p.returncode for p in children])
        if rtt and counts["dropped"] == 0:
            raise AssertionError("Configured loss never exercised")
        if fault in ("held-frame", "disconnect") and counts["forced"] == 0:
            raise AssertionError("Configured fault never exercised")
        result.update(name=name, rtt_ms=rtt, jitter_ms=20 if rtt else 0,
                      loss=.01 if rtt else 0, input_delay=delay, counts=counts,
                      pids=[p.pid for p in children], wall_seconds=time.monotonic() - start)
        (folder / "verification.json").write_text(json.dumps(result, indent=2))
        return result
    finally:
        for child in children:
            if child.poll() is None:
                child.kill()
            child.wait(timeout=10)
        for log in logs:
            log.close()
        for s in relays + reserved:
            s.close()
        selector.close()


def validate(folder, reports, ticks, fault, codes):
    # Schema-specific assertions are kept here so reports remain independently
    # auditable instead of trusting the clients' own success boolean.
    negative = fault in ("delay-mismatch", "disconnect")
    if codes != ([3, 3] if negative else [0, 0]):
        raise AssertionError(f"Unexpected client exit codes: {codes}")
    traces, metrics = [], []
    for player, report in enumerate(reports):
        net = report["network"]
        if not net["evidence_complete"]:
            raise AssertionError("Client applied-prefix evidence is incomplete")
        expected = "error" if negative else "complete"
        if net["state"] != expected or bool(report["ok"]) == negative:
            raise AssertionError(f"Client {player} state/result disagrees: {net['state']}")
        if report["player"] != player or net["player"] != player:
            raise AssertionError("Wrong local player")
        trace = [f"{int(row['tick'])} {int(row['hash'], 16)}" for row in net["trace"]]
        if len(trace) != net["tick"]:
            raise AssertionError("Applied trace missing a tick")
        traces.append(trace)
        if not negative and (len(trace) != ticks or net["confirmed_ticks"] != ticks):
            raise AssertionError("Successful client did not confirm every tick")
        if trace and int(net["hash"], 16) != int(trace[-1].split()[1]):
            raise AssertionError("Terminal state differs from applied trace")
        if fault == "delay-mismatch" and (trace or not any(word in net["error"] for word in ("incompatible input delay", "timeout/disconnect waiting for handshake"))):
            raise AssertionError("Mismatched delay must reject before gameplay")
        if fault == "disconnect" and (not 50 <= len(trace) <= 68 or "timeout" not in net["error"]):
            raise AssertionError("Disconnect did not preserve a bounded timeout prefix")
        prefix = folder / f"peer{player}"
        for config in ("Debug", "Release"):
            if not trace:
                continue  # VFR2 reader intentionally requires a nonzero prefix.
            destination = folder / f"replay{player}-{config}.trace"
            replay(ROOT / "build/windows/sim" / config / "voidfront_headless.exe",
                   prefix.with_suffix(".vfr"), destination)
            compare(trace, read_trace(destination, len(trace)), f"{folder.name}: {player} {config} replay")
        commands = recorded_commands(prefix.with_suffix(".vfr"), len(trace))
        local = [c for c in commands if c["player"] == player]
        applied_inputs = [event for event in net["inputs"] if event["executed_usec"] > 0]
        if [(c["tick"], c["sequence"]) for c in local] != [(e["execution_tick"], e["sequence"]) for e in applied_inputs]:
            raise AssertionError("Timing coverage omits, duplicates or reorders an applied local input")
        accepted, feedback, displayed = report["accepted_inputs"], report["feedback_samples"], report["execution_display_samples"]
        if [e["sequence"] for e in net["inputs"]] != [e["sequence"] for e in accepted]:
            raise AssertionError("Bridge accepted-input coverage differs from event ingress")
        if [e["sequence"] for e in feedback] != [e["sequence"] for e in accepted]:
            raise AssertionError("Feedback omits or duplicates accepted input")
        if [e["sequence"] for e in displayed] != [e["sequence"] for e in applied_inputs]:
            raise AssertionError("Display omits or duplicates executed input")
        times, visual, local_feedback = [], [], []
        for event, original, feedback_event in zip(net["inputs"], accepted, feedback):
            for field in ("sequence", "input_usec", "order", "x", "z"):
                if event[field] != original[field]:
                    raise AssertionError(f"Event identity changed: {field}")
            if event["units"] != sorted(set(original["units"])):
                raise AssertionError("Event unit selection changed")
            if not 0 <= event["input_usec"] <= event["accepted_usec"]:
                raise AssertionError("Acceptance timestamp precedes input")
            if feedback_event["input_usec"] != event["input_usec"] or feedback_event["feedback_rendered_usec"] < event["accepted_usec"]:
                raise AssertionError("Feedback timestamp not joined to input")
            local_feedback.append((feedback_event["feedback_rendered_usec"] - event["input_usec"]) / 1000)
        for command, event, display in zip(local, applied_inputs, displayed):
            for field in ("sequence", "order", "x", "z", "units"):
                if command[field] != event[field]:
                    raise AssertionError(f"Canonical recording differs from original input: {field}")
            if event["execution_tick"] != event["source_tick"] + net["input_delay"]:
                raise AssertionError("Input executed outside agreed schedule")
            if not event["accepted_usec"] <= event["sampled_usec"] <= event["executed_usec"] <= display["displayed_usec"]:
                raise AssertionError("Input timing stages reversed")
            if display["display_tick"] <= event["execution_tick"]:
                raise AssertionError("Display precedes applied snapshot")
            if not any(row["tick"] == display["display_tick"] and row["rendered_usec"] == display["displayed_usec"] for row in report["rendered_snapshots"]):
                raise AssertionError("Display has no matching post-draw observation")
            times.append((event["executed_usec"] - event["input_usec"]) / 1000)
            visual.append((display["displayed_usec"] - event["input_usec"]) / 1000)
        if not negative and (len(local) != 3 or not report["selection_own_player"] or set(report["accepted_orders"]) != {0, 1, 2}):
            raise AssertionError("Missing move/stop/attack-move or own-player selection evidence")
        states = [row["state"] for row in report["status_history"]]
        if states[-1] != expected or (not negative and "running" not in states):
            raise AssertionError("Client lifecycle observations missing")
        if fault == "held-frame" and player == 0 and "stalled" not in states:
            raise AssertionError("Held frame did not expose stalled state")
        for line in prefix.with_suffix(".log").read_text(errors="replace").splitlines():
            if line.startswith(("SCRIPT ERROR:", "ERROR:")) and not line.startswith("ERROR: Failed to read the root certificate store."):
                raise AssertionError(f"Unexpected runtime error: {line}")
        if not prefix.with_suffix(".png").is_file():
            raise AssertionError("Missing terminal screenshot")
        metrics.append(dict(player=player, ticks=len(trace), state=net["state"],
                            applied_inputs=len(local), accepted_inputs=len(accepted),
                            event_to_execution_ms=times, event_to_display_ms=visual,
                            event_to_feedback_ms=local_feedback,
                            execution_p95_ms=percentile(times), display_p95_ms=percentile(visual),
                            feedback_p95_ms=percentile(local_feedback), states=states))
    length = min(map(len, traces))
    compare(traces[0][:length], traces[1][:length], f"{folder.name}: common client prefix")
    if fault == "delay-mismatch" and not any("incompatible input delay" in report["network"]["error"] for report in reports):
        raise AssertionError("Neither client detected the incompatible input delay")
    return dict(ok=True, peers=metrics, trace_sha256=[hashlib.sha256("\n".join(t).encode()).hexdigest() for t in traces])


def recorded_commands(path, ticks):
    data = path.read_bytes()
    u32 = lambda offset: int.from_bytes(data[offset:offset + 4], "little")
    if data[:4] != b"VFR\x02" or len(data) < 32 or u32(16) != ticks:
        raise AssertionError("Invalid applied-prefix replay header")
    result, offset = [], 32
    for _ in range(u32(20)):
        size = u32(offset)
        offset += 4
        if size < 26 or offset + size > len(data) or data[offset:offset + 4] != b"VFC\x01":
            raise AssertionError("Invalid canonical replay command")
        count = u32(offset + 22)
        if size != 26 + count * 4:
            raise AssertionError("Invalid canonical unit count")
        result.append(dict(tick=u32(offset + 4), sequence=u32(offset + 8),
                           player=data[offset + 12], order=data[offset + 13],
                           x=u32(offset + 14), z=u32(offset + 18),
                           units=[u32(offset + 26 + i * 4) for i in range(count)]))
        offset += size
    if offset != len(data):
        raise AssertionError("Trailing replay bytes")
    return result


def verify_repeats(out, summary):
    clean = next((case for case in summary["cases"] if case["name"] == "clean"), None)
    if clean is None:
        return
    expected = read_trace(out / "clean/replay0-Release.trace", clean["peers"][0]["ticks"])
    for index in range(10):
        target = out / f"repeat-{index:02d}.trace"
        replay(ROOT / "build/windows/sim/Release/voidfront_headless.exe", out / "clean/peer0.vfr", target)
        compare(expected, read_trace(target, len(expected)), f"clean repeat {index}")
    summary["clean_replay_repeats"] = 10
    (out / "summary.json").write_text(json.dumps(summary, indent=2))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=Path)
    parser.add_argument("--ticks", type=int, default=240)
    parser.add_argument("--case", choices=("clean", "rtt80", "rtt160", "held-frame", "delay-mismatch", "disconnect"))
    args = parser.parse_args()
    allowed()
    out = (args.out or ROOT / "artifacts" / datetime.now(timezone.utc).strftime("client-network-%Y%m%dT%H%M%S%fZ")).resolve()
    out.mkdir(parents=True, exist_ok=False)
    files = [ROOT / "artifacts/package" / f for f in ("Voidfront.exe", "Voidfront.pck")]
    files += list((ROOT / "artifacts/package").rglob("*.dll"))
    files += [ROOT / "build/windows/sim" / cfg / "voidfront_headless.exe" for cfg in ("Debug", "Release")]
    summary = dict(started_utc=datetime.now(timezone.utc).isoformat(),
                   fingerprints={str(p): digest(p) for p in files}, cases=[])
    (out / "run.json").write_text(json.dumps(summary, indent=2))
    cases = [("clean", 0, 2, None), ("rtt80", 80, 2, None), ("rtt160", 160, 4, None),
             ("held-frame", 0, 2, "held-frame"), ("delay-mismatch", 0, 2, "delay-mismatch"),
             ("disconnect", 0, 2, "disconnect")]
    for name, rtt, delay, fault in cases:
        if args.case and args.case != name:
            continue
        print(f"START {name}", flush=True)
        result = run_case(out, name, args.ticks, rtt, delay, fault)
        summary["cases"].append(result)
        summary["completed_utc"] = datetime.now(timezone.utc).isoformat()
        (out / "summary.json").write_text(json.dumps(summary, indent=2))
        print(f"PASS {name}", flush=True)
    verify_repeats(out, summary)
    for path, sha in summary["fingerprints"].items():
        if digest(Path(path)) != sha:
            raise AssertionError(f"Executable evidence changed during suite: {path}")


if __name__ == "__main__":
    main()

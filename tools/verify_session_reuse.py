"""Two actual peer pairs reuse UDP endpoints; replay prior-session wire traffic.

This verifies transport isolation with externally agreed identities, not coordinated
rematch negotiation, packaged presentation, physical networking or human play.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import selectors
import socket
import subprocess
import time

from verify_network import (ROOT, HIDDEN, allowed, compare, read_trace, replay,
                            verify_command_coverage, verify_timing)


def run_round(build, folder, ports, relays, selector, identity, seed, stale):
    allowed()
    folder.mkdir()
    children, logs, captured = [], [], [{}, {}]
    injected = [0, 0]
    started = time.monotonic()
    try:
        for player, config in enumerate(("Debug", "Release")):
            prefix = folder / f"peer{player}"
            args = [str(build / config / "voidfront_peer.exe"),
                    "--player", str(player), "--port", str(ports[player]),
                    "--remote-port", str(relays[player].getsockname()[1]),
                    "--session", str(identity), "--seed", str(seed),
                    "--ticks", "100", "--units-per-team", "6", "--timeout-ms", "2000",
                    "--trace", str(prefix.with_suffix(".trace")),
                    "--record", str(prefix.with_suffix(".vfr")),
                    "--report", str(prefix.with_suffix(".json"))]
            log = prefix.with_suffix(".log").open("w")
            logs.append(log)
            children.append(subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT,
                                             creationflags=HIDDEN))
        while any(child.poll() is None for child in children):
            allowed()
            if time.monotonic() - started > 20:
                raise TimeoutError("owned session-reuse peer watchdog")
            for key, _ in selector.select(.002):
                source = key.data
                try:
                    data, address = key.fileobj.recvfrom(65536)
                except (BlockingIOError, ConnectionResetError):
                    continue
                if address != ("127.0.0.1", ports[source]):
                    raise AssertionError("unexpected relay sender")
                kind = int.from_bytes(data[40:44], "little")
                captured[source].setdefault(kind, data)
                # Respond from the exact configured remote endpoint. Inject all
                # captured kinds before current traffic, repeatedly through play.
                for old in reversed(list(stale[1 - source].values())):
                    relays[source].sendto(old, address)
                    injected[source] += 1
                destination = 1 - source
                relays[destination].sendto(data, ("127.0.0.1", ports[destination]))
        for log in logs:
            log.flush()
        reports = [json.loads((folder / f"peer{p}.json").read_text()) for p in range(2)]
        if [p.returncode for p in children] != [0, 0]:
            raise AssertionError(f"peer failure: {reports}")
        traces = [read_trace(folder / f"peer{p}.trace", 100) for p in range(2)]
        compare(*traces, "shared session traces")
        for player, report in enumerate(reports):
            if report["status"] != "complete" or report["confirmed_ticks"] != 100:
                raise AssertionError("session did not confirm its full prefix")
            verify_timing(report)
            verify_command_coverage(report, folder / f"peer{player}.vfr", player)
            target = folder / f"replay{player}.trace"
            replay(build / "sim" / ("Release" if player == 0 else "Debug") /
                   "voidfront_headless.exe", folder / f"peer{player}.vfr", target)
            compare(traces[player], read_trace(target, 100), "opposite-build replay")
        for player in range(2):
            if set(captured[player]) != set(range(1, 8)):
                raise AssertionError("did not capture all seven transport packet kinds")
            if stale[player] and injected[player] < 7:
                raise AssertionError("stale traffic was not exercised")
            for kind, data in captured[player].items():
                (folder / f"wire-p{player}-kind{kind}.bin").write_bytes(data)
        result = {"identity": identity, "seed": seed, "pids": [p.pid for p in children],
                  "ports": ports, "injected_stale_packets": injected,
                  "captured_kinds": [sorted(c) for c in captured], "peers": reports}
        (folder / "result.json").write_text(json.dumps(result, indent=2))
        return captured, result
    finally:
        for child in children:
            if child.poll() is None:
                child.kill()
            child.wait(timeout=5)
        for log in logs:
            log.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=ROOT / "build/windows")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    allowed()
    build, out = args.build.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    binaries = [build / cfg / "voidfront_peer.exe" for cfg in ("Debug", "Release")]
    binaries += [build / "sim" / cfg / "voidfront_headless.exe" for cfg in ("Debug", "Release")]
    fingerprints = {str(p.relative_to(build)): hashlib.sha256(p.read_bytes()).hexdigest() for p in binaries}
    (out / "run.json").write_text(json.dumps({"started_utc": datetime.now(timezone.utc).isoformat(),
                                             "binary_sha256": fingerprints}, indent=2))
    relays, reservations = [], []
    selector = selectors.DefaultSelector()
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
        for reserve in reservations:
            reserve.close()
        old, first = run_round(build, out / "first", ports, relays, selector, 64040903, 42, [{}, {}])
        # Drain late first-round traffic into the retained packet collection.
        for relay in relays:
            while True:
                try:
                    relay.recvfrom(65536)
                except (BlockingIOError, ConnectionResetError):
                    break
        _, second = run_round(build, out / "fresh", ports, relays, selector, 64040904, 99, old)
        if (out / "first/peer0.trace").read_bytes() == (out / "fresh/peer0.trace").read_bytes():
            raise AssertionError("fresh seed did not establish a distinct state stream")
        summary = {"verified": True, "binary_sha256": fingerprints, "rounds": [first, second],
                   "scope": __doc__}
        (out / "summary.json").write_text(json.dumps(summary, indent=2))
        print("PASS: two separate-process pairs, reused ports, seven stale packet kinds, cross-build replays")
    finally:
        selector.close()
        for sock in relays + reservations:
            sock.close()


if __name__ == "__main__":
    main()

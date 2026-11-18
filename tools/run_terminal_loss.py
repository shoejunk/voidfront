"""Reproduce TCP loss before final UDP checksum confirmation in two packaged clients."""
import argparse
import json
from pathlib import Path
import subprocess

from run_rematch import REPO, check_stop, digest, run_pair
from verify_net_match import hash_value, parse_replay, require, trace_equal
from verify_rematch import capture_evidence, read_json


def audit_events(report):
    player = report["player"]
    expected = ["archived-1", "finishing",
                "disconnecting-tcp" if player == 0 else "observed-eof", "archived-2"]
    events = report["events"]
    require([e["event"] for e in events] == expected, "unexpected lifecycle event order")
    times = [e["usec"] for e in events]
    require(all(t > 0 for t in times) and times == sorted(set(times)), "unordered local event times")
    for event, round_number in zip(events, [0, 1, 1, 1], strict=True):
        row = report["rounds"][round_number]
        require(event["session"] == row["lobby"]["session"] and event["hash"] == row["network"]["hash"]
                and event["tick"] == row["lobby"]["ticks"], "event and final native endpoint disagree")
    for row, event, connected in zip(report["rounds"], [events[0], events[-1]], [True, False], strict=True):
        require(event["state"] == "complete" and event["confirmed_ticks"] == event["tick"]
                and event["lobby_present"] is connected and event["archive"] == row["archive"],
                "bad archive event")
    for event in events[1:3]:
        require(event["state"] == "finishing" and 0 <= event["confirmed_ticks"] < event["tick"]
                and event["archive"] == "", "no outstanding final checksum at fault")
    require(events[1]["lobby_present"] is True and events[1]["notice"] == "", "lobby already detached")
    if player == 0:
        require(events[2]["lobby_present"] is True, "host TCP was already closed")
    else:
        require(events[2]["lobby_present"] is False and events[2]["notice"].startswith("Peer left"),
                "guest did not observe the production TCP disconnect path")


def audit(out, pids, ports):
    reports = [read_json(out / f"p{p}.json") for p in (0, 1)]
    require(len(set(pids)) == 2 and all(p > 0 for p in pids), "expected separate process IDs")
    for p, report in enumerate(reports):
        require(report["pid"] == pids[p] and report["player"] == p and len(report["rounds"]) == 2,
                "launcher and client identities differ")
        audit_events(report)

    archives, sessions, rounds = set(), set(), []
    for i, (seed, delay, cap) in enumerate([(7, 2, 32), (8, 3, 64)], 1):
        pair = [rep["rounds"][i - 1] for rep in reports]
        setup = pair[0]["lobby"]
        session = setup["session"]
        require(session.isdecimal() and 0 < int(session) < 2**63 and session not in sessions, "bad session")
        sessions.add(session)
        require(setup == pair[1]["lobby"] == dict(session=session, host_port=ports[0], guest_port=ports[1],
                seed=seed, delay=delay, ticks=cap, map_id=2), "peers disagreed on setup")
        require(pair[0]["network"]["trace"] == pair[1]["network"]["trace"], "peer traces differ")
        expected = [(r["tick"], hash_value(r["hash"])) for r in pair[0]["network"]["trace"]]
        require([t for t, _ in expected] == list(range(1, cap + 1)), "native trace has missing rows")
        replays = [out / f"r{i}-p{p}.vfr" for p in (0, 1)]
        require(replays[0].read_bytes() == replays[1].read_bytes(), "different peer recordings")
        native_runs = []
        for p, (row, replay) in enumerate(zip(pair, replays, strict=True)):
            net = row["network"]
            require(net["state"] == "complete" and not net["error"] and net["evidence_complete"]
                    and net["confirmed_ticks"] == net["recorded_ticks"] == net["tick"] == cap
                    and net["requested_ticks"] == cap and net["session_id"] == session
                    and net["ready"] and net["player"] == p and net["input_delay"] == delay
                    and net["seed"] == seed and hash_value(net["hash"]) == expected[-1][1]
                    and row["winner"] == -1, "uncorroborated final endpoint")
            archive = Path(row["archive"]).resolve(strict=True)
            require(archive.name == f"{session}-p{p}.vfr" and archive not in archives
                    and not archive.samefile(replay) and archive.read_bytes() == replay.read_bytes(),
                    "missing, reused, or mismatched production replay archive")
            archives.add(archive)
            commands = parse_replay(replay.read_bytes(), row)
            require(len(commands) == 2 and sorted(c["player"] for c in commands) == [0, 1],
                    "missing peer commands")
            local = next(c for c in commands if c["player"] == p)
            require(len(net["inputs"]) == 1 and net["inputs"][0]["disposition"] == "executed",
                    "expected one executed local input")
            event = net["inputs"][0]
            workers = list(range(1 + p * 3, 4 + p * 3))
            require(row["selected"] == workers and event["units"] == local["units"] == workers
                    and event["order"] == local["order"] == 0 and event["player"] == p
                    and event["sequence"] == local["sequence"] == 1
                    and event["source_tick"] + delay == event["execution_tick"] == local["tick"],
                    "Stop button input and canonical command disagree")
            for config in ("Debug", "Release"):
                check_stop()
                exe = REPO / f"build/windows/sim/{config}/voidfront_headless.exe"
                path = out / f"r{i}-p{p}-{config}.trace"
                result = subprocess.run([str(exe), "--replay", str(archive), "--hash-mode", "state", "--trace", str(path)],
                                        capture_output=True, text=True, timeout=30, creationflags=subprocess.CREATE_NO_WINDOW)
                require(result.returncode == 0, f"{config}: {result.stderr}")
                actual = [tuple(map(int, line.split())) for line in path.read_text().splitlines()]
                trace_equal(actual, expected, path.name)
                native_runs.append({"player": p, "config": config, "exe_sha256": digest(exe),
                                    "trace_sha256": digest(path)})
        rounds.append({"setup": setup, "winner": -1, "hash": pair[0]["network"]["hash"],
                       "commands": commands, "replay_sha256": digest(replays[0]), "native_runs": native_runs})
    captures = [capture_evidence(out / name) for name in ["p1-setup-offer.png", "p1-rematch-offer.png",
                                                           "p0-terminal.png", "p1-terminal.png"]]
    return {"rounds": rounds, "archives": sorted(map(str, archives)), "captures": captures,
            "events": [rep["events"] for rep in reports]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("out", type=Path)
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--resolution", default="1280x720")
    args = parser.parse_args()
    check_stop()
    out = args.out.resolve()
    out.mkdir(parents=True)
    package = args.package.resolve(strict=True)
    files = [package, package.with_suffix(".pck"), package.with_name("voidfront_bridge.dll")]
    fingerprints = {str(file): digest(file) for file in files}
    pids, ports, elapsed = run_pair(package, out, "--net-terminal-loss-smoke", 100, args.resolution)
    host = dict(package=fingerprints, pids=pids, ports=ports, elapsed_seconds=elapsed)
    (out / "host.json").write_text(json.dumps(host, indent=2) + "\n", encoding="utf-8")
    summary = audit(out, pids, ports)
    require(fingerprints == {str(file): digest(file) for file in files}, "package changed during run")
    summary.update(ok=True, **host)
    (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(f"Verified two capped rounds and eight archive replays; guest EOF at {summary['events'][1][2]}")


if __name__ == "__main__":
    main()

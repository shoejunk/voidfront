"""Run two packaged loopback matches using the client lobby and normal input paths."""
import argparse
from contextlib import ExitStack
import hashlib
import json
import os
from pathlib import Path
import secrets
import socket
import subprocess
import time


REPO = Path(__file__).resolve().parents[1]


def check_stop():
    if (REPO / ".voidfront-agent/STOP").exists():
        raise RuntimeError("Voidfront STOP is present")


def choose_port():
    for _ in range(100):
        host = 40000 + secrets.randbelow(20000)
        with ExitStack() as stack:
            try:
                for kind, port in ((socket.SOCK_STREAM, host), (socket.SOCK_DGRAM, host),
                                   (socket.SOCK_DGRAM, host + 1)):
                    sock = stack.enter_context(socket.socket(socket.AF_INET, kind))
                    sock.bind(("127.0.0.1", port))
                return host
            except OSError:
                continue
    raise RuntimeError("No free adjacent loopback ports")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("out", type=Path)
    parser.add_argument("--package", type=Path, required=True)
    args = parser.parse_args()
    check_stop()
    out, package = args.out.resolve(), args.package.resolve(strict=True)
    fingerprints = dict(package_path=str(package), package_sha256=digest(package),
                        pck_sha256=digest(package.with_suffix(".pck")),
                        bridge_sha256=digest(package.with_name("voidfront_bridge.dll")))
    out.mkdir(parents=True)
    rounds = [out / "round1", out / "round2"]
    for folder in rounds:
        folder.mkdir()
    host = choose_port()
    env = dict(os.environ, APPDATA=str(REPO / "artifacts/godot-profile"))
    start = time.monotonic()
    children = []
    window = subprocess.STARTUPINFO()
    window.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    window.wShowWindow = subprocess.SW_HIDE
    with ExitStack() as stack:
        try:
            for player in range(2):
                stdout = stack.enter_context((out / f"p{player}.out").open("wb"))
                stderr = stack.enter_context((out / f"p{player}.err").open("wb"))
                # Guest deliberately launches with different local values.
                child = subprocess.Popen(
                    [str(package), "--log-file", str(out / f"p{player}-engine.log"),
                     "--resolution", "1280x720", "--", "--net-rematch-smoke", f"--player={player}",
                     f"--lobby-port={host}", f"--report={out}", f"--seed={7 if player == 0 else 99}",
                     f"--delay={2 if player == 0 else 11}"],
                    env=env, stdout=stdout, stderr=stderr, startupinfo=window,
                    creationflags=subprocess.CREATE_NO_WINDOW)
                children.append(child)
                print(f"Player {player}: PID {child.pid}, UDP {host + player}", flush=True)
            while any(child.poll() is None for child in children):
                check_stop()
                if time.monotonic() - start > 1350:
                    raise RuntimeError("Pair exceeded watchdog; logs retained")
                failed = [child.returncode for child in children if child.returncode not in (None, 0)]
                if failed:
                    raise RuntimeError(f"Paired client exited {failed}; logs retained")
                time.sleep(0.5)
            if any(child.returncode != 0 for child in children):
                raise RuntimeError("Paired client failed; logs retained")
            for player in range(2):
                errors = (out / f"p{player}.err").read_text(encoding="utf-8", errors="replace").splitlines()
                if any(line.startswith(("SCRIPT ERROR:", "ERROR:")) and
                       not line.startswith("ERROR: Failed to read the root certificate store.") for line in errors):
                    raise RuntimeError(f"Player {player} engine error; logs retained")
            for i, folder in enumerate(rounds):
                reports = [json.loads((folder / f"p{p}.json").read_text(encoding="utf-8")) for p in range(2)]
                lobby = reports[0]["lobby"]
                assert all(rep["ok"] and rep["lobby"] == lobby for rep in reports)
                assert (lobby["seed"], lobby["delay"]) == (7 + i, 2 + i)
                manifest = dict(fingerprints, pids=[x.pid for x in children], ports=[host, host + 1],
                                **{k: lobby[k] for k in ("session", "seed", "delay", "ticks")},
                                contested=False, success_return=False)
                (folder / "host.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
                print(f"Round {i + 1}: winner {reports[0]['winner']}, tick {reports[0]['final_tick']}", flush=True)
            print(f"Pair finished in {time.monotonic() - start:.2f}s", flush=True)
        finally:
            for child in children:
                if child.poll() is None:
                    child.terminate()
                child.wait()


if __name__ == "__main__":
    main()

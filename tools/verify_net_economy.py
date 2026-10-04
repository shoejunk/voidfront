"""Verify a two-client networked economy run (tools/run_net_economy.sh output).

Checks both reports pass with complete evidence, the clients' per-tick state
hashes agree, both applied-command recordings replay in Debug and Release
headless builds to exactly that trace, and each client's own inputs executed.
"""
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main(folder):
    folder = Path(folder)
    reports = [json.loads((folder / f"p{p}.json").read_text()) for p in range(2)]
    for p, r in enumerate(reports):
        assert r["ok"], f"player {p} report not ok: {r['fixture']['errors']} {r['network'].get('error')}"
        assert r["network"]["evidence_complete"], f"player {p} evidence incomplete"
        assert not r["fixture"]["errors"], r["fixture"]["errors"]
        executed = [i for i in r["network"]["inputs"] if i["player"] == p]
        assert executed and all(i["disposition"] == "executed" for i in executed), f"player {p} unexecuted inputs"
    traces = [[(row["tick"], row["hash"]) for row in r["network"]["trace"]] for r in reports]
    assert traces[0] == traces[1], "clients diverged"
    ticks = len(traces[0])
    assert ticks == reports[0]["network"]["recorded_ticks"] > 0
    expected = [f"{t} {int(h, 16)}" for t, h in traces[0]]
    orders = sorted({i["order"] for r in reports for i in r["network"]["inputs"]})
    for config in ("Debug", "Release"):
        exe = ROOT / "build/windows/sim" / config / "voidfront_headless.exe"
        for p in range(2):
            trace = folder / f"replay-{config}-p{p}.trace"
            out = subprocess.run([str(exe), "--replay", str(folder / f"p{p}.vfr"), "--hash-mode", "state",
                                  "--trace", str(trace)], capture_output=True, text=True, timeout=120)
            assert out.returncode == 0, out.stderr
            got = trace.read_text().split("\n")[:-1]
            if got != expected:
                first = next(i for i, (a, b) in enumerate(zip(got, expected)) if a != b)
                raise AssertionError(f"{config} p{p} replay diverges at tick {first + 1}")
    print(json.dumps({"ok": True, "ticks": ticks, "final_hash": traces[0][-1][1], "orders_seen": orders,
                      "replays_matched": 4, "winner": reports[0]["winner"]}))


if __name__ == "__main__":
    main(sys.argv[1])

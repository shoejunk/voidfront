"""Check a --worker-route-smoke capture against native replay."""
import argparse
import json
from pathlib import Path
import statistics
import struct
import subprocess

from verify_net_match import ROOT, digest, parse_replay, prefix, require, trace_equal
from verify_rematch import capture_evidence


def check_capture(folder, name, event):
    path = folder / (name + ".png")
    metadata = capture_evidence(path)
    require(metadata["sha256"] == event["image_sha256"], f"{name}: screenshot digest mismatch")
    require((metadata["width"], metadata["height"]) == (1280, 720), f"{name}: expected 720p PNG")
    require(path.read_bytes().endswith(bytes.fromhex("0000000049454e44ae426082")), f"{name}: incomplete PNG")
    # Decode the whole PNG, then sample luminance across the viewport. A
    # colored cursor on an otherwise empty frame is insufficient evidence.
    decode = subprocess.run(["ffmpeg", "-v", "error", "-xerror", "-err_detect", "crccheck+explode",
                             "-i", path, "-frames:v", "1", "-vf", "scale=160:90:flags=area",
                             "-pix_fmt", "gray", "-f", "rawvideo", "pipe:1"], capture_output=True)
    require(decode.returncode == 0 and len(decode.stdout) == 160 * 90, f"{name}: unreadable screenshot")
    require(statistics.pstdev(decode.stdout) >= 8, f"{name}: nearly uniform screenshot")
    return metadata


def verify(folder, out):
    recording = folder / "route.vfr"
    raw = recording.read_bytes()
    report = json.loads((folder / "report.json").read_text(encoding="utf-8"))
    protocol, seed, _, ticks, _, _, _ = struct.unpack_from("<5IQI", raw, 4)
    expected = [(int(t), int(h, 16)) for t, h in
                (line.split() for line in (folder / "route.trace").read_text().splitlines())]
    require(not report["error"] and ticks == report["tick"] and
            [t for t, _ in expected] == list(range(1, ticks + 1)) and
            expected[-1][1] == int(report["hash"], 16), "incomplete scene trace")
    commands = parse_replay(raw, dict(fixture=dict(initial_snapshot=dict(protocol=protocol,
        content_id=report["content_id"], width=64, height=48)), network=dict(seed=seed,
        recorded_ticks=ticks, replay_command_count=3)))
    require([(c["player"], c["order"], c["units"]) for c in commands] ==
            [(0, 1, [3]), (0, 5, [2]), (0, 4, [2])], "wrong observer, recall or Gather")
    require((commands[2]["x"], commands[2]["z"]) == (14464, 10368), "wrong Gather target")
    names = ("home", "at-flux", "past-flux", "mining", "returning", "paid")
    events = report["events"]
    sampled = [events[name]["tick"] for name in names]
    require(sampled == sorted(set(sampled)) and sampled[0] == commands[2]["tick"]
            and sampled[-1] == ticks, "captures out of order")
    out.mkdir(parents=True, exist_ok=False)
    images = {name: check_capture(folder, name, events[name]) for name in names}
    require(len({image["sha256"] for image in images.values()}) == len(names), "duplicate screenshots")
    target = " → Salvage (56, 40)"
    loads = {"home": "Empty cargo", "at-flux": "Empty cargo" + target,
             "past-flux": "Empty cargo" + target, "mining": "1 salvage" + target,
             "returning": "Returning 10 salvage to anchor", "paid": "Empty cargo" + target}
    for name, load in loads.items():
        heading = " ".join(events[name]["card"].split())
        require(heading == "Worker · 100/100 HP · " + load, f"{name}: incorrect selected worker card")
    # The fixture captures immediately on crossing each x threshold. The
    # 32-subunit maximum step bounds the overshoot. Both samples must remain
    # local to the central flux corner, within one cell vertically.
    for name, min_x, z in (("at-flux", 7700, 6079), ("past-flux", 8001, 6250)):
        actual_x, actual_z = events[name]["position"]
        require(min_x <= actual_x < min_x + 32 and abs(actual_z - z) < 256,
                f"{name}: outside flux crossing")
    native_ticks = set(sampled + [events[name]["tick"] - 1 for name in names[1:]])
    require(len(native_ticks) == 11, "crossing and transition samples must be distinct")
    binaries = {}
    # Reconstruct the tick before each crossing, ore or bank change.
    for cfg in ("Debug", "Release"):
        exe = ROOT / "build/windows/sim" / cfg / "voidfront_headless.exe"
        binaries[cfg] = digest(exe)
        full = out / (cfg + ".trace")
        subprocess.run([exe, "--replay", recording, "--trace", full], check=True, capture_output=True)
        actual = [tuple(map(int, line.split())) for line in full.read_text().splitlines()]
        trace_equal(actual, expected, cfg)
        snapshots = {}
        for tick in sorted(native_ticks):
            path = out / f"{cfg}-{tick}.vfr"
            path.write_bytes(prefix(raw, commands, tick))
            metrics = path.with_suffix(".json")
            trace = path.with_suffix(".trace")
            subprocess.run([exe, "--replay", path, "--metrics", metrics, "--trace", trace],
                           check=True, capture_output=True)
            trace_equal((tuple(map(int, line.split())) for line in trace.read_text().splitlines()),
                        expected[:tick], f"{cfg} prefix {tick}")
            snapshots[tick] = json.loads(metrics.read_text())
        for name in names:
            event = events[name]
            state = snapshots[event["tick"]]
            miner = state["units"][1]
            native = [miner["end"], miner["order"], miner["resource_id"], miner["returning"],
                      miner["hp"], miner["cargo"], state["salvage"][0], state["deposits"][1]["remaining"]]
            require(native == [event[k] for k in ("position", "order", "deposit", "returning",
                    "hp", "cargo", "bank", "reserve")] and event["selection"] == [2], name)
            if name != "home":
                require(miner["hp"] == 100 and miner["order"] == 4 and miner["resource_id"] == 2,
                        f"{name}: worker lost health or its Gather target")
        for name, threshold in (("at-flux", 7700), ("past-flux", 8001)):
            tick = events[name]["tick"]
            require(snapshots[tick-1]["units"][1]["end"][0] < threshold,
                    f"{cfg} {name}: crossing was already observed")
        for name, before, after in (("mining", (0, 2000, 0), (1, 1999, 0)),
                                    ("returning", (9, 1991, 0), (10, 1990, 0)),
                                    ("paid", (10, 1990, 0), (0, 1990, 10))):
            tick = events[name]["tick"]
            for state, transition in ((snapshots[tick-1], before), (snapshots[tick], after)):
                require((state["units"][1]["cargo"], state["deposits"][1]["remaining"],
                         state["salvage"][0]) == transition, f"{cfg} {name} transition")
        require(events["mining"]["position"] == [14464, 10144] and
                events["paid"]["position"] == events["home"]["position"] == [1152, 2848],
                "crossing, mining or payment position")
    summary = dict(ticks=ticks, events=events, commands=commands, images=images, binaries=binaries,
                   replay_sha256=digest(recording), prefixes_per_build=len(snapshots))
    (out / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(f"Matched {ticks} scene ticks, three commands and {len(snapshots)} prefixes in both builds.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    verify(args.folder, args.out)

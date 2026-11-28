"""Verify a packaged --held-route-smoke capture against the original stalled Move."""
import argparse
import json
import math
import re
from pathlib import Path
import struct
import subprocess

from verify_net_match import ROOT, digest, parse_replay, prefix, require, trace_equal
from verify_worker_route import check_capture


# The HUD draws full-health friendly bars in #64e5df. Allow five levels per
# channel for image conversion. Exclude UI strips, minimap dots, short model
# highlights and long UI rules by matching the shape of the 720p bar.
CYAN_RUN = re.compile(rb"(?:[\x5f-\x69][\xe0-\xea][\xda-\xe4]){22,}")
WIDTH, HEIGHT = 1280, 720
TOP, BOTTOM = 114, 578


def visible_bars(path):
    result = subprocess.run(["ffmpeg", "-v", "error", "-xerror", "-i", str(path),
                             "-frames:v", "1", "-pix_fmt", "rgb24", "-f", "rawvideo", "pipe:1"],
                            check=True, capture_output=True)
    raw = result.stdout
    if len(raw) != WIDTH * HEIGHT * 3:
        raise ValueError(f"{path.name}: expected 1280 by 720 RGB frame")
    spans = []
    for y in range(TOP, BOTTOM):
        start = y * WIDTH * 3
        for run in CYAN_RUN.finditer(raw, start, start + WIDTH * 3):
            left = (run.start() - start) // 3
            width = (run.end() - run.start()) // 3
            if not (run.start() - start) % 3 and width <= 60:
                spans.append((left, left + width - 1, y))
    return spans


def check_phase_bars(path, event, units):
    name = path.stem
    reason = f"{name}: image or projected bars disagree with native workers"
    camera_x, camera_z = event["camera_target"]
    # Glass Reach, KEEP_HEIGHT size 27, 1280x720. The ordinary minimap pan is
    # centered at (30,24) for three phases and clamped near the arrival goal.
    height_per_z = 25 / math.hypot(25, 21)
    expected_camera = (40, 48 - 27 / height_per_z / 2) if name == "arrival" else (30, 24)
    if not (math.isfinite(camera_x) and math.isfinite(camera_z)):
        raise ValueError(reason + ": invalid camera focus")
    if max(abs(camera_x - expected_camera[0]), abs(camera_z - expected_camera[1])) > 0.125:
        raise ValueError(reason + ": unexpected camera focus")
    centers = event["friendly_bars"]
    if len(centers) != 3:
        raise ValueError(reason + ": expected three friendly actors")
    pixels_per_tile = HEIGHT / 27
    expected = []
    for (x, y), unit in zip(centers, units[:3]):
        if unit["player"] != 0 or not (math.isfinite(x) and math.isfinite(y)):
            raise ValueError(reason + ": unexpected owner or projection")
        ux, uz = unit["end"]
        projected_x = WIDTH / 2 + (ux / 256 - camera_x) * pixels_per_tile
        ground_y = HEIGHT / 2 + (uz / 256 - camera_z) * height_per_z * pixels_per_tile
        # The fixture renders this snapshot at alpha one. Only the vertical
        # bar placement can move substantially to clear a world label.
        if abs(projected_x - x) > 1 or abs(ground_y - y) > 95:
            raise ValueError(reason + ": native world projection")
        if 20 <= x < WIDTH - 20 and TOP + 3 <= y < BOTTOM - 3:
            expected.append((x, y))
    spans = visible_bars(path)
    # A full 36-unit fill covers 28 or 29 pixels at this resolution. Bind
    # every visible worker to a complete fill and require a clear row between
    # horizontally overlapping bars. A merged or partially painted bar fails.
    fills = []
    for x, y in expected:
        rows = [(left, right, row) for left, right, row in spans if
                right - left + 1 in (28, 29) and abs((left + right + 1) / 2 - x) <= 1
                and abs(row + 0.5 - y) <= 2]
        if len(rows) not in (2, 3) or rows[-1][2] - rows[0][2] != len(rows) - 1:
            raise ValueError(f"{reason}: missing full-height bar at {(x, y)}")
        fills.append(rows)
    if sorted(span for rows in fills for span in rows) != sorted(spans):
        raise ValueError(reason + ": extra or merged cyan spans")
    for i, rows in enumerate(fills):
        left, right, top = rows[0]
        bottom = rows[-1][2]
        for other in fills[:i]:
            x0, x1, y0 = other[0]
            if x0 <= right and left <= x1 and top <= other[-1][2] + 1 and y0 <= bottom + 1:
                raise ValueError(reason + ": full bars touch")


def verify(folder, out):
    report = json.loads((folder / "report.json").read_text(encoding="utf-8"))
    require(not report["error"], report["error"])
    recording = folder / "route.vfr"
    raw = recording.read_bytes()
    original = (ROOT / ".voidfront-agent/replays/2026-11-25-two-holders-stall-p0.vfr").read_bytes()
    protocol, seed, _, ticks, count, _, _ = struct.unpack_from("<5IQI", raw, 4)
    require(raw[:16] == original[:16] and raw[32:] == original[32:] and count == 7,
            "different setup or canonical inputs from the original stall")
    commands = parse_replay(raw, dict(fixture=dict(initial_snapshot=dict(
        protocol=protocol, content_id=report["content_id"], width=64, height=48)),
        network=dict(seed=seed, recorded_ticks=ticks, replay_command_count=count)))
    events = report["events"]
    names = ("launch", "contact", "passage", "arrival")
    require(set(events) == set(names), "missing or extra route samples")
    require([events[name]["tick"] for name in names] == [commands[-1]["tick"], 662, 710, ticks]
            and 710 < ticks <= 1442, "wrong capture ticks or incomplete Move")
    expected = [(int(t), int(h, 16)) for t, h in
                (line.split() for line in (folder / "route.trace").read_text().splitlines())]
    require([t for t, _ in expected] == list(range(1, ticks + 1)) and ticks == report["tick"]
            and expected[-1][1] == int(report["hash"], 16), "incomplete scene trace")
    images = {name: check_capture(folder, name, events[name]) for name in names}
    require(len({image["sha256"] for image in images.values()}) == len(names), "duplicate frames")
    require(events["launch"]["position"] == [1152, 2848] and events["arrival"]["position"] == [14464, 10144],
            "wrong start or destination")
    require(events["contact"]["position"] != events["passage"]["position"], "stationary at the posts")
    # The first three stills share a camera. Passing the two held units must
    # leave each stationary unit's own bar at its original screen position.
    for name in ("contact", "passage"):
        for index in (0, 2):
            require(math.dist(events["launch"]["friendly_bars"][index],
                              events[name]["friendly_bars"][index]) <= 0.5,
                    f"{name}: passing worker displaced a holder's health bar")
    out.mkdir(parents=True, exist_ok=False)
    sample_ticks = sorted({events[name]["tick"] for name in names} | {ticks - 1})
    binaries = {}
    for config in ("Debug", "Release"):
        exe = ROOT / "build/windows/sim" / config / "voidfront_headless.exe"
        binaries[config] = digest(exe)
        snapshots = {}
        for tick in sample_ticks:
            stem = out / f"{config}-{tick}"
            replay = stem.with_suffix(".vfr")
            replay.write_bytes(prefix(raw, commands, tick))
            trace = stem.with_suffix(".trace")
            metrics = stem.with_suffix(".json")
            subprocess.run([exe, "--replay", replay, "--trace", trace, "--metrics", metrics],
                           capture_output=True, check=True)
            trace_equal((tuple(map(int, line.split())) for line in trace.read_text().splitlines()),
                        expected[:tick], f"{config} to {tick}")
            snapshots[tick] = json.loads(metrics.read_text())
        for name in names:
            event = events[name]
            state = snapshots[event["tick"]]
            units = state["units"]
            middle = units[1]
            if config == "Release":
                check_phase_bars(folder / (name + ".png"), event, units)
            require(all(u["hp"] == u["max_hp"] for u in units) and len(units) == 6,
                    f"{name}: worker lost or damaged")
            require(event["selection"] == [2] and middle["id"] == 2 and middle["player"] == 0
                    and middle["end"] == event["position"] and middle["order"] == event["order"]
                    and middle["hp"] == event["hp"] == 100, f"{name}: wrong selected worker or state")
            require([event[key] for key in ("deposit", "returning", "cargo", "bank", "reserve")] ==
                    [middle["resource_id"], middle["returning"], middle["cargo"], state["salvage"][0],
                     state["deposits"][1]["remaining"]] == [0, False, 0, 0, 2000],
                    f"{name}: incorrect worker load, deposit or resources")
            require(" ".join(event["card"].split()) == "Worker · 100/100 HP · Empty cargo",
                    f"{name}: incorrect selected worker card")
            require(units[0]["end"] == [7488, 5888] and units[2]["end"] == [7616, 5760],
                    f"{name}: wrong post positions")
            if name != "launch":
                require(units[0]["order"] == units[2]["order"] == 3 and middle["goal"] == [14464, 10144],
                        f"{name}: Hold or original goal lost")
        before = snapshots[ticks - 1]["units"][1]
        require(before["order"] == 1 and snapshots[ticks]["units"][1]["order"] == 0,
                "final snapshot did not capture the Move-to-Stop transition")
    result = dict(ticks=ticks, commands=commands, images=images, events=events,
                  replay_sha256=digest(recording), native_sha256=binaries, prefixes_per_build=len(sample_ticks))
    (out / "summary.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(f"Seven original commands, {ticks} matching client ticks and {2 * len(sample_ticks)} native prefixes.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    verify(args.folder, args.out)

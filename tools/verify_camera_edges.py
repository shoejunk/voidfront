"""Check the camera's reported ground rays and replay the worker's edge clicks."""
import argparse
import json
import math
from pathlib import Path
import subprocess

from verify_net_match import ROOT, digest, hash_value, parse_replay, prefix, require, trace_equal
from verify_rematch import capture_evidence, read_json


SIDES = ("west", "north", "east", "south")
CORNERS = ("southwest", "southeast")


def check_footprints(report):
    width, height = report["map"]
    vw, vh = report["viewport"]
    rows = {row["label"]: row for row in report["footprints"]}
    labels = {"opening", "default_home", "opposing_miner", "near_after_default_home",
              "near_after_far_opposing_spawn"} | {s + "_orders" for s in SIDES + CORNERS}
    labels.update(f"{zoom}_{view}" for zoom in ("near", "far")
                  for view in ("home", "nw", "ne", "sw", "se", "opposing_spawn"))
    require(set(rows) == labels and len(rows) == len(report["footprints"]), "missing camera locations")
    sample_ticks = [r["tick"] for r in report["footprints"]]
    require(sample_ticks == sorted(set(sample_ticks)) and 0 <= sample_ticks[0] <= sample_ticks[-1] <= report["ticks"],
            "out-of-sequence camera samples")
    for label, row in rows.items():
        cx, cz = row["target"]
        size = row["size"]
        if label.startswith("near_after_"):
            require(abs(size - 14) < 0.002, f"{label}: failed to zoom in")
        half_x = size * vw / vh / 2
        half_z = size * math.hypot(25, 21) / 25 / 2
        expected = [(cx-half_x, cz-half_z), (cx+half_x, cz-half_z),
                    (cx+half_x, cz+half_z), (cx-half_x, cz+half_z)]
        for (x, z), (ex, ez) in zip(row["corners"], expected, strict=True):
            require(-0.002 <= x <= width+0.002 and -0.002 <= z <= height+0.002
                    and math.dist((x, z), (ex, ez)) < 0.003, f"{label}: incorrect ground footprint")
        if label == "opening" or label.endswith(("_home", "_opposing_spawn")):
            fx, fz = (59.5, 35.5) if label.endswith("_opposing_spawn") else (10.0, 12.5)
            center = (max(half_x, min(fx, width-half_x)), max(half_z, min(fz, height-half_z)))
            require(math.dist(center, (cx, cz)) < 0.003, f"{label}: framed the wrong spawn")
    for zoom, size in (("near", 14), ("far", min(36, width * vh/vw, height * 25/math.hypot(25, 21)))):
        for direction, index, boundary in (("nw", 0, (0, 0)), ("ne", 1, (width, 0)),
                                          ("se", 2, (width, height)), ("sw", 3, (0, height))):
            row = rows[f"{zoom}_{direction}"]
            require(abs(row["size"] - size) < 0.002 and math.dist(row["corners"][index], boundary) < 0.003,
                    f"{zoom}_{direction}: minimap or zoom did not reach the map limit")
    return rows


def audit(report_path, out):
    report = read_json(report_path)
    require(report["ok"] and not report["errors"] and report["mode"] == "camera_edge"
            and report["map"] == [64, 48] and report["setup"]["enemy_ai"] is False, "fixture failed")
    host_path = report_path.with_name(report_path.stem + "-host.json")
    package = {}
    if host_path.is_file():
        host = read_json(host_path)
        if "requested_resolution" in host:
            require(report["window"] == list(map(int, host["requested_resolution"].split("x"))),
                    "window size differs from launcher request")
            package = host.get("package", {})
            require(all(digest(path) == sha for path, sha in package.items()), "package differs from captured build")
    rows = check_footprints(report)
    expected = [(r["tick"], hash_value(r["hash"])) for r in report["trace"]]
    ticks = report["ticks"]
    require([t for t, _ in expected] == list(range(1, ticks+1))
            and expected[-1][1] == hash_value(report["final_hash"]), "missing simulation ticks")
    setup = report["setup"] | {"width": 64, "height": 48}
    replay = report_path.with_suffix(".vfr")
    raw = replay.read_bytes()
    network = dict(seed=setup["seed"], recorded_ticks=ticks, replay_command_count=len(report["inputs"]))
    commands = parse_replay(raw, dict(fixture=dict(initial_snapshot=setup), network=network))
    labels = [f"{s}_{leg}" for s in SIDES + CORNERS for leg in ("stage", "move")] + ["deposit_approach", "deposit_gather"]
    require([i["label"] for i in report["inputs"]] == labels, "missing edge or gathering command")
    for i, command in zip(report["inputs"], commands, strict=True):
        require(i["accepted"] and command["tick"] == i["event_tick"] and command["player"] == 0
                and i["units"] == [1] and i["order"] == (4 if i["label"] == "deposit_gather" else 1)
                and all(command[k] == i[k] for k in ("x", "z", "units", "order")), "replay/input mismatch")

    def run(config, recording, count, name):
        require(not (ROOT / ".voidfront-agent/STOP").exists(), "STOP present")
        trace, metrics = out / (name + ".trace"), out / (name + ".json")
        exe = ROOT / f"build/windows/sim/{config}/voidfront_headless.exe"
        subprocess.run([str(exe), "--replay", str(recording), "--trace", str(trace), "--metrics", str(metrics)],
                       check=True, capture_output=True, timeout=120)
        trace_equal([tuple(map(int, l.split())) for l in trace.read_text().splitlines()], expected[:count], name)
        return read_json(metrics)

    for config in ("Debug", "Release"):
        final = run(config, replay, ticks, config)
    require([e["edge"] for e in report["edges"]] == list(SIDES), "missing edge selection")
    require([e["edge"] for e in report["corners"]] == list(CORNERS), "missing corner selection")
    for corner, panel in zip(report["corners"], ("command_card", "minimap"), strict=True):
        require(corner["occluder"] == panel and corner["blocked_input_count"] == 0
                and corner["clear_selected"] == [1] and corner["hud_restored"] is True, "clear view transition failed")
    capture_windows = {
        "home": (rows["default_home"]["tick"], rows["near_after_default_home"]["tick"] - 1),
        "far-opposing-spawn": (rows["far_opposing_spawn"]["tick"], rows["near_after_far_opposing_spawn"]["tick"] - 1),
        "near-opposing-spawn": (rows["near_opposing_spawn"]["tick"], rows["west_orders"]["tick"] - 1),
        "opposing-miner": (ticks, ticks),
    }
    for index, edge in enumerate(report["edges"] + report["corners"]):
        side = edge["edge"]
        capture_windows[side + ("-clear" if side in CORNERS else "-selected")] = (
            edge["arrival_tick"], commands[index*2+1]["tick"])
        require(edge["actor"] == 1 and edge["selected_before"] == [] and edge["selected_after"] == [1],
                f"{side}: worker not reacquired")
        corners = rows[side + "_orders"]["corners"]
        x0, z0 = corners[0]
        x1, z1 = corners[2]
        for leg, tick_key, at_key, screen_key in (("stage", "arrival_tick", "from", "selection_screen"),
                                                  ("goal", "final_tick", "to", "order_screen")):
            tick = edge[tick_key]
            click = commands[index*2 + (leg == "goal")]
            require(click["tick"] < tick <= commands[index*2 + (leg == "goal") + 1]["tick"],
                    f"{side}: arrival outside command interval")
            x, z = edge[leg]
            proximity = {"west": x, "north": z, "east": 64-x, "south": 48-z,
                         "southwest": max(x, 48-z), "southeast": max(64-x, 48-z)}[side]
            limit = 3.25 if side in CORNERS else 2.5
            require(0 <= proximity <= limit and max(abs(x*256-click["x"]), abs(z*256-click["z"])) <= 1,
                    f"{side}: click too far from edge or wrong destination")
            height = 0.5 if leg == "stage" else 0
            vw, vh = report["viewport"]
            sx, sy = edge[screen_key]
            require(0 <= sx < vw and 0 <= sy < vh and
                    math.dist((sx, sy), ((x-x0)*vw/(x1-x0), (z-height*21/25-z0)*vh/(z1-z0))) < 1,
                    f"{side}: projected screen click differs from world position")
            path = out / f"{side}-{leg}.vfr"
            path.write_bytes(prefix(raw, commands, tick))
            worker = run("Release", path, tick, path.stem)["units"][0]
            require(worker["id"] == 1 and worker["hp"] > 0 and worker["order"] == 0
                    and worker["end"] == edge[at_key] == [click["x"], click["z"]], f"{side}: native arrival mismatch")
    miner = report["mining"]
    worker, site = final["units"][0], final["deposits"][1]
    require(miner["tick"] == ticks and worker["id"] == miner["worker"]["id"] == 1
            and worker["resource_id"] == site["id"] == 2 and worker["order"] == 4 and worker["cargo_kind"] == 0
            and worker["cargo"] == miner["worker"]["cargo"] == setup["start_deposit_remaining"] - site["remaining"]
            and worker["cargo"] > 0 and site["remaining"] == miner["deposit"]["remaining"], "mining state mismatch")
    names = ({"home", "far-opposing-spawn", "near-opposing-spawn", "opposing-miner"}
             | {s+"-selected" for s in SIDES} | {s+"-clear" for s in CORNERS})
    require([c["label"] for c in report["captures"]][-1] == "opposing-miner"
            and report["captures"][-1]["tick"] == ticks and {c["label"] for c in report["captures"]} == names
            and len(report["captures"]) == len(names), "missing or mistimed screenshots")
    captures = {}
    # Canvas-items stretch keeps the logical aspect. Viewport PNGs omit the
    # letterbox bars that the window adds around the rendered play area.
    vw, vh = report["viewport"]
    scale = min(report["window"][0]/vw, report["window"][1]/vh)
    for c in report["captures"]:
        path = report_path.with_name(report_path.stem + "-" + c["label"] + ".png")
        require(c["error"] == 0 and Path(c["path"]).samefile(path), "capture provenance mismatch")
        first, last = capture_windows[c["label"]]
        require(0 <= first <= c["tick"] <= last <= ticks, f"{c['label']}: capture outside action interval")
        image = capture_evidence(path)
        require(abs(image["width"] - vw*scale) < 1 and abs(image["height"] - vh*scale) < 1,
                "capture dimensions differ from fitted viewport")
        captures[c["label"]] = image
    return dict(ticks=ticks, hash=report["final_hash"], replay_sha256=digest(replay), package=package,
                window=report["window"], footprint_samples=len(rows),
                edge_and_corner_arrivals=12, native_full_runs=2, commands=len(commands), mined=worker["cargo"], captures=captures)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    summary = audit(args.report.resolve(), args.out)
    (args.out / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(f"Verified {summary['ticks']} ticks, all four edges and opposing salvage extraction.")

"""Render selected Glass Reach replay ticks through the Godot project presentation.

Example: render_replay_samples.py match.vfr --player 1 --frame 13858:6
         --frame 13900:6 --out artifacts/guard-contact
Each frame follows the specified friendly unit at the normal gameplay zoom.
"""
import argparse
import json
import os
from pathlib import Path
import struct
import subprocess

from verify_net_match import ROOT, SCALARS, UNIT_FIELDS, digest, parse_replay, prefix, require
from verify_rematch import capture_evidence


def read_ticks(path):
    return [tuple(map(int, line.split())) for line in path.read_text().splitlines()]


def native_frame(native, raw, commands, tick, trace, out):
    stem = out / f"tick-{tick}"
    path = stem.with_suffix(".vfr")
    path.write_bytes(prefix(raw, commands, tick))
    metrics, short_trace = stem.with_suffix(".json"), stem.with_suffix(".trace")
    subprocess.run([native, "--replay", path, "--metrics", metrics, "--trace", short_trace],
                   check=True, stdout=subprocess.DEVNULL)
    require(read_ticks(short_trace) == trace[:tick], f"prefix diverged through tick {tick}")
    data = json.loads(metrics.read_text())
    require(data["replay"] and data["ticks"] == tick and data["map"] == 2, "wrong native sample")
    frame = {key: data[key] for key in SCALARS}
    frame.update(tick=tick, hash=f"{trace[tick-1][1]:016x}", units=[])
    for unit in data["units"]:
        row = {key: unit[key] for key in UNIT_FIELDS}
        row.update(x=unit["end"][0], z=unit["end"][1], goal_x=unit["goal"][0],
                   goal_z=unit["goal"][1])
        frame["units"].append(row)
    return frame


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("replay", type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--player", type=int, choices=(0, 1), required=True)
    parser.add_argument("--frame", required=True, action="append", metavar="TICK:FRIENDLY_UNIT_ID")
    parser.add_argument("--native", type=Path, default=ROOT/"build/windows/sim/Release/voidfront_headless.exe")
    parser.add_argument("--project", type=Path, default=ROOT/"client")
    args = parser.parse_args()
    require(not (ROOT/".voidfront-agent/STOP").exists(), "stop switch present")
    executable = Path(json.loads((ROOT/"tools/toolchain.json").read_text())["godot_console"])
    version = subprocess.check_output([executable, "--version"], text=True).strip()
    require(version.startswith("4.7.2.stable."), "pinned engine unavailable")
    raw = args.replay.read_bytes()
    protocol, seed, _, ticks, count, content, _ = struct.unpack_from("<5IQI", raw, 4)
    setup = dict(width=64, height=48, protocol=protocol, content_id=f"{content:016x}")
    commands = parse_replay(raw, dict(fixture=dict(initial_snapshot=setup),
        network=dict(seed=seed, recorded_ticks=ticks, replay_command_count=count)))
    choices = [tuple(map(int, frame.split(":"))) for frame in args.frame]
    require(all(len(choice) == 2 and 2 <= choice[0] <= ticks and choice[1] >= 1 for choice in choices)
            and len(set(choices)) == len(choices), "invalid frame tick or actor")
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    full = out/"full.trace"
    subprocess.run([args.native, "--replay", args.replay, "--trace", full],
                   check=True, stdout=subprocess.DEVNULL)
    trace = read_ticks(full)
    require([tick for tick, _ in trace] == list(range(1, ticks+1)), "incomplete full native replay")
    snapshots = {tick: native_frame(args.native, raw, commands, tick, trace, out)
                 for tick in sorted({t for tick, _ in choices for t in (tick-1, tick)})}
    for tick, actor in choices:
        units = snapshots[tick]["units"]
        require(actor <= len(units) and units[actor-1]["id"] == actor
                and units[actor-1]["player"] == args.player, "camera actor belongs to another player")
    plan = dict(player=args.player, seed=seed, protocol=protocol, content_id=setup["content_id"],
                frames=choices, snapshots=snapshots)
    (out/"plan.json").write_text(json.dumps(plan, separators=(",", ":")))
    command = [executable, "--path", args.project.resolve(), "--resolution", "1280x720", "--script",
               ROOT/"tools/render_replay_samples.gd", "--log-file", out/"engine.log"]
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = subprocess.SW_HIDE
    with (out/"stdout.log").open("w") as stdout, (out/"stderr.log").open("w") as stderr:
        subprocess.run(command, stdout=stdout, stderr=stderr, check=True, timeout=60,
                       env=dict(os.environ, APPDATA=str(out), VOIDFRONT_REPLAY_SAMPLES=str(out)), startupinfo=startup)
    errors = (out/"stderr.log").read_text(errors="replace").splitlines()
    require(not any(line.startswith(("SCRIPT ERROR:", "ERROR:"))
                    and "Failed to read the root certificate store" not in line for line in errors),
            "Godot errors in stderr.log")
    report = json.loads((out/"frames.json").read_text())
    require(len(report["frames"]) == len(choices), "missing rendered frame")
    for (tick, actor), frame in zip(choices, report["frames"], strict=True):
        snapshot = snapshots[tick]
        unit = snapshot["units"][actor-1]
        require((frame["tick"], frame["actor"], frame["hash"]) ==
                (tick, actor, snapshot["hash"]), "rendered wrong tick or actor")
        require(frame["position"] == [unit["x"], unit["z"]] and frame["hp"] == unit["hp"]
                and frame["selected"] == (unit["hp"] > 0), "scene position, health or selection mismatch")
        frame.update(cargo=unit["cargo"], returning=unit["returning"], order=unit["order"])
        vision = snapshot["vision"][args.player]
        enemies = [u["id"] for u in snapshot["units"] if u["player"] != args.player and u["hp"] > 0
                   and vision[(u["z"] // 256) * setup["width"] + u["x"] // 256] == 2]
        require(frame["visible_enemies"] == enemies, "scene actors disagree with the player's replay vision")
        frame["image"] = capture_evidence(out/f"tick-{tick}-unit-{actor}.png")
        require((frame["image"]["width"], frame["image"]["height"]) == (1280, 720), "wrong render size")
    report.update(player=args.player, replay_sha256=digest(args.replay), native_sha256=digest(args.native),
                  trace_sha256=digest(full), total_ticks=ticks, commands=len(commands), engine=version,
                  scene_sha256=digest(args.project/"main.gd"), hud_sha256=digest(args.project/"hud.gd"))
    (out/"summary.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"{len(choices)} verified samples from {ticks} ticks: {out}")


if __name__ == "__main__":
    main()

"""Independently check retained presentation timing evidence; not a performance gate."""
import argparse
import copy
import hashlib
import json
import math
import re
from pathlib import Path


STAGES = {"bridge", "snapshot", "present", "main_process", "hud_process", "hud_draw"}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def number(value):
    require(type(value) in (int, float) and math.isfinite(value) and value >= 0,
            f"Invalid nonnegative finite number: {value!r}")
    return value


def integer(value):
    number(value)
    require(int(value) == value, f"Nonintegral count: {value!r}")
    return int(value)


def statistics(values):
    if not values:
        return {"count": 0}
    ordered = sorted(values)
    return {"count": len(values), "mean": sum(values) / len(values),
            **{f"p{p}": ordered[math.ceil(len(values) * p / 100) - 1] for p in (50, 95, 99)},
            "maximum": ordered[-1], "total": sum(values)}


def summarize(rows):
    stages = {}
    for row in rows:
        for stage, calls in row["stage_calls_usec"].items():
            stages.setdefault(stage, []).extend(calls)
    return {"frames": len(rows), "focused_frames": sum(row["focused"] for row in rows),
            "wall_interval_usec": statistics([r["wall_interval_usec"] for r in rows]),
            "process_delta_usec": statistics([r["process_delta_usec"] for r in rows]),
            "stage_calls_usec": {s: statistics(c) for s, c in stages.items()}}


def equivalent(actual, expected, label):
    if isinstance(expected, dict):
        require(isinstance(actual, dict) and actual.keys() == expected.keys(), f"{label}: keys differ")
        for key, value in expected.items():
            equivalent(actual[key], value, f"{label}.{key}")
    else:
        number(actual)
        require(math.isclose(actual, expected, rel_tol=1e-9, abs_tol=1e-6),
                f"{label}: {actual} != {expected}")


def verify(report):
    require(report["schema"] == 1 and report["ok"] is True and report["errors"] == [], "Failed/schema report")
    require(report["mode"] == "ordinary_offline_presentation" and report["ai_enabled"] is True
            and report["smoke_fixture"] is False, "Not ordinary offline AI presentation")
    warmup, measured = report["warmup_samples"], report["measured_samples"]
    require(len(warmup) == integer(report["requested_warmup_frames"]), "Warmup count differs")
    require(len(measured) == integer(report["requested_measured_frames"]) and measured, "Measured count differs")
    start, boundary, end = (integer(report[k]) for k in ("started_usec", "measurement_started_usec", "ended_usec"))
    require(start <= boundary < end, "Invalid measurement boundaries")
    tick = integer(report["initial_snapshot"]["tick"])
    elapsed = 0
    all_rows = warmup + measured
    for frame, row in enumerate(all_rows):
        require(integer(row["frame"]) == frame, "Missing/duplicate frame index")
        interval = integer(row["wall_interval_usec"])
        elapsed += interval
        require(integer(row["elapsed_usec"]) == elapsed, "Wall intervals do not telescope")
        number(row["process_delta_usec"])
        require(type(row["focused"]) is bool, "Focus is not boolean")
        require(type(row["minimized"]) is bool, "Minimized state is not boolean")
        require(re.fullmatch(r"[0-9a-f]{16}", row["state_hash"]) is not None, "Invalid state hash")
        next_tick = integer(row["tick"])
        calls = row["stage_calls_usec"]
        require(isinstance(calls, dict) and not calls.keys() - STAGES, "Unknown stages")
        for stage, values in calls.items():
            require(isinstance(values, list), f"{stage}: calls not array")
            for value in values:
                integer(value)
        # Redraw is asynchronous to deferred sampling; its row is not claimed
        # to identify the matching process frame. It must still be retained.
        for stage in ("main_process", "present", "hud_process"):
            require(len(calls.get(stage, [])) == 1, f"Frame {frame}: missing/duplicate {stage}")
        require(0 <= next_tick - tick <= 8, "Tick regressed or exceeds production catch-up cap")
        require(len(calls.get("bridge", [])) == len(calls.get("snapshot", [])) == next_tick - tick,
                "Bridge/snapshot calls do not match tick advancement")
        require(integer(row["bridge_calls_this_frame"]) == next_tick - tick, "Reported catch-up count differs")
        nested = sum(sum(calls.get(s, [])) for s in ("bridge", "snapshot", "present"))
        require(nested <= calls["main_process"][0], "Nested stages exceed main callback")
        for key in ("units", "actors", "selected"):
            integer(row[key])
        require(row["actors"] == row["units"] and row["selected"] <= row["units"], "Actor/selection counts disagree")
        require(len(row["alive_by_player"]) == 2 and sum(integer(n) for n in row["alive_by_player"]) <= row["units"],
                "Invalid alive population")
        require(number(row["camera_size"]) > 0, "Invalid camera size")
        require(len(row["camera_position"]) == 3 and all(type(n) in (int, float) and math.isfinite(n)
                for n in row["camera_position"]), "Invalid camera position")
        for value in row["performance"].values():
            number(value)
        tick = next_tick
    initial_tick = integer(report["measurement_initial_snapshot"]["tick"])
    require(initial_tick == (warmup[-1]["tick"] if warmup else report["initial_snapshot"]["tick"]),
            "Measurement initial snapshot does not match boundary")
    require(integer(report["final_snapshot"]["tick"]) == tick and tick > initial_tick, "Final tick differs/no measured advance")
    require(report["final_snapshot"]["hash"] == measured[-1]["state_hash"], "Final state hash differs")
    require(report["measurement_initial_snapshot"]["hash"] ==
            (warmup[-1]["state_hash"] if warmup else report["initial_snapshot"]["hash"]), "Boundary state hash differs")
    require(boundary - start == sum(r["wall_interval_usec"] for r in warmup), "Warmup boundary differs")
    require(end - start == elapsed == integer(report["total_wall_usec"]), "Total wall accounting differs")
    require(end - boundary == sum(r["wall_interval_usec"] for r in measured)
            == integer(report["measurement_wall_usec"]), "Measured wall accounting differs")
    for name, rows in (("warmup", warmup), ("measurement", measured)):
        equivalent(report[f"{name}_summary"], summarize(rows), f"{name}_summary")
    totals = summarize(measured)["stage_calls_usec"]
    require(all(totals.get(s, {}).get("count", 0) > 0 for s in STAGES), "Missing measured stage")
    require(report["foreground_entire_measurement"] is all(r["focused"] for r in measured), "Foreground claim differs")
    return {"frames": len(measured), "ticks": tick - initial_tick,
            "foreground_entire_measurement": report["foreground_entire_measurement"],
            "measurement_summary": summarize(measured)}


def corruptions(report):
    # Recompute summary fields after raw-data mutations so the rejection tests
    # relational integrity, not only a stale percentile copied from the source.
    mutations = {
        "missing_frame": lambda r: r["measured_samples"].pop(),
        "negative_stage": lambda r: r["measured_samples"][0]["stage_calls_usec"]["present"].__setitem__(0, -1),
        "missing_present": lambda r: r["measured_samples"][0]["stage_calls_usec"].pop("present"),
        "missing_snapshot": lambda r: next(x for x in r["measured_samples"] if x["stage_calls_usec"].get("snapshot"))["stage_calls_usec"]["snapshot"].pop(),
        "bad_tick": lambda r: r["measured_samples"][0].__setitem__("tick", 999999),
        "bad_elapsed": lambda r: r["measured_samples"][0].__setitem__("elapsed_usec", 0),
        "bad_containment": lambda r: r["measured_samples"][0]["stage_calls_usec"]["main_process"].__setitem__(0, 0),
        "no_draw_evidence": lambda r: [x["stage_calls_usec"].pop("hud_draw", None) for x in r["measured_samples"]],
        "false_focus_claim": lambda r: r.__setitem__("foreground_entire_measurement", not r["foreground_entire_measurement"]),
    }
    rejected = []
    for name, mutate in mutations.items():
        changed = copy.deepcopy(report)
        mutate(changed)
        changed["measurement_summary"] = summarize(changed["measured_samples"])
        try:
            verify(changed)
        except (ValueError, KeyError, TypeError):
            rejected.append(name)
        else:
            raise ValueError(f"Corruption accepted: {name}")
    changed = copy.deepcopy(report)
    changed["measurement_summary"]["wall_interval_usec"]["p95"] += 1
    try:
        verify(changed)
    except ValueError:
        rejected.append("false_percentile")
    else:
        raise ValueError("Corruption accepted: false_percentile")
    return rejected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reports", nargs="+", type=Path)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    results = []
    for path in args.reports:
        raw = path.read_bytes()
        report = json.loads(raw)
        result = verify(report)
        result.update(path=str(path.resolve()), sha256=hashlib.sha256(raw).hexdigest(),
                      rejected_corruptions=corruptions(report))
        results.append(result)
    output = {"ok": True, "reports": results,
              "limits": "Integrity of retained measurements only; no CPU/GPU cause, matched-frame redraw, unoccluded foreground, gameplay or budget acceptance."}
    text = json.dumps(output, indent=2)
    if args.out:
        require(not args.out.exists(), "Output exists; preserve prior evidence")
        args.out.write_text(text + "\n", encoding="utf-8")
    print(text)


if __name__ == "__main__":
    main()

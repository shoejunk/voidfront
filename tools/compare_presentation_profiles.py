"""Check controlled-profile comparability before showing retained timing deltas.

This is diagnostic evidence, never a performance or visual-quality gate.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path

from verify_presentation_profile import require, statistics, verify


def payload(host):
    require(host["packaged"] is True and host["failure"] is None and host["exit_code"] == 0,
            "Comparison requires successful packaged captures")
    result = {}
    for item in host["prelaunch_fingerprints"]:
        name = item["path"].replace("\\", "/").rsplit("/", 1)[-1]
        if name in ("Voidfront.exe", "Voidfront.pck", "voidfront_bridge.dll"):
            require(name not in result, f"Duplicate payload fingerprint: {name}")
            result[name] = item["sha256"]
    require(set(result) == {"Voidfront.exe", "Voidfront.pck", "voidfront_bridge.dll"}, "Missing payload fingerprint")
    return result


def audit_package(directory, expected):
    result = {}
    for name, digest in expected.items():
        path = directory / name
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        require(actual.lower() == digest.lower(), f"Retained package fingerprint differs: {path}")
        result[name] = {"path": str(path.resolve()), "sha256": actual, "bytes": path.stat().st_size}
    return result


def compare(left, right, left_host, right_host, comparison):
    verify(left)
    verify(right)
    require(left["mode"] == right["mode"] == "controlled_pose_resubmission", "Requires controlled captures")
    for key in ("seed", "map", "requested_units_per_team", "ai_enabled", "engine", "renderer", "display_server",
                "rendering_method", "window_size", "viewport_size", "vsync_mode", "max_fps", "msaa_3d",
                "requested_warmup_frames", "requested_measured_frames", "initial_snapshot", "initial_context", "static_geometry"):
        require(left[key] == right[key], f"Unmatched {key}")
    for key in ("tick", "camera", "clip_phase_seconds", "animation_manifest", "initial_pose_hash", "final_pose_hash"):
        require(left["controlled"][key] == right["controlled"][key], f"Unmatched controlled {key}")
    for report, host in ((left, left_host), (right, right_host)):
        require(report["process_id"] == host["runtime_process_id"], "Host report belongs to another runtime")
        require(host["controlled"] == report["controlled"]["variant"] and host["controlled_tick"] == report["controlled"]["tick"]
                and host["controlled_camera"] == report["controlled"]["camera"], "Host fixture options differ")
        require(host["map"] == report["map"] and host["requested_units_per_team"] == report["requested_units_per_team"], "Host map differs")
        require(not any(row["minimized"] for row in report["measured_samples"]), "Minimized measurement")
    for key in ("cpu", "logical_processors", "windows", "visible_requested", "screenshot_after_measurement"):
        require(left_host[key] == right_host[key], f"Unmatched host {key}")
    require(left["foreground_entire_measurement"] == right["foreground_entire_measurement"], "Unmatched focus coverage")
    a, b = payload(left_host), payload(right_host)
    if comparison == "pose":
        require(a == b, "Pose comparison requires identical executable/PCK/DLL")
        require({left["controlled"]["variant"], right["controlled"]["variant"]} == {"pose-refresh", "pose-frozen"}, "Pose variants not paired")
        require(left["initial_scene_node_classes"] == right["initial_scene_node_classes"], "Pose comparison scene topology differs")
    else:
        require(left["controlled"]["variant"] == right["controlled"]["variant"], "Implementation comparison variants differ")
        require(a["Voidfront.exe"] == b["Voidfront.exe"] and a["voidfront_bridge.dll"] == b["voidfront_bridge.dll"],
                "Implementation comparison changed native payload")
    metrics = {}
    for metric in ("wall_interval_usec", "process_delta_usec"):
        lstat, rstat = (r["measurement_summary"][metric] for r in (left, right))
        metrics[metric] = {"left": lstat, "right": rstat,
                           "right_minus_left": {key: rstat[key] - lstat[key] for key in ("mean", "p50", "p95", "p99")}}
    for metric in ("draw_calls", "render_objects", "primitives"):
        metrics[metric] = {name: statistics([r["performance"][metric] for r in report["measured_samples"]])
                           for name, report in (("left", left), ("right", right))}
    return {"comparison": comparison, "left_variant": left["controlled"]["variant"],
            "right_variant": right["controlled"]["variant"], "payloads": {"left": a, "right": b}, "metrics": metrics}


def corruption_checks(left, right, left_host, right_host, comparison):
    mutations = {
        "different_snapshot": lambda r, h: r["initial_snapshot"].__setitem__("hash", "0" * 16),
        "different_static_geometry": lambda r, h: r["static_geometry"].__setitem__("sha256", "0" * 64),
        "different_renderer": lambda r, h: r.__setitem__("renderer", "unmatched"),
        "different_native_payload": lambda r, h: next(x for x in h["prelaunch_fingerprints"] if x["path"].endswith("voidfront_bridge.dll")).__setitem__("sha256", "0" * 64),
        "wrong_runtime_host": lambda r, h: h.__setitem__("runtime_process_id", -1),
        "different_camera": lambda r, h: r["controlled"].__setitem__("camera", "overview" if r["controlled"]["camera"] == "standard" else "standard"),
    }
    rejected = []
    for name, mutate in mutations.items():
        changed, host = copy.deepcopy(right), copy.deepcopy(right_host)
        mutate(changed, host)
        try:
            compare(left, changed, left_host, host, comparison)
        except (ValueError, KeyError, TypeError):
            rejected.append(name)
        else:
            raise ValueError(f"Comparison corruption accepted: {name}")
    return rejected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("left", type=Path)
    parser.add_argument("right", type=Path)
    parser.add_argument("--comparison", choices=("pose", "implementation"), required=True)
    parser.add_argument("--left-package", type=Path, help="Optionally audit retained executable/PCK/DLL against left host fingerprints")
    parser.add_argument("--right-package", type=Path, help="Optionally audit retained executable/PCK/DLL against right host fingerprints")
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    paths = [args.left, args.right]
    reports = [json.loads(p.read_bytes()) for p in paths]
    host_paths = [p.with_name(p.stem + "-host.json") for p in paths]
    hosts = [json.loads(p.read_bytes()) for p in host_paths]
    result = compare(*reports, *hosts, args.comparison)
    result["retained_package_audits"] = {
        side: audit_package(directory, result["payloads"][side])
        for side, directory in (("left", args.left_package), ("right", args.right_package)) if directory is not None}
    result.update(ok=True, inputs=[{"path": str(p.resolve()), "sha256": hashlib.sha256(p.read_bytes()).hexdigest()} for p in paths + host_paths],
                  rejected_corruptions=corruption_checks(*reports, *hosts, args.comparison),
                  limits="Same fixed simulation/pose/settings diagnostic. Manual repeated seek is not normal animation playback. Timing deltas include downstream engine/render effects and host variation; no isolated GPU/skeleton cost, human foreground, gameplay or budget acceptance. Inspect paired screenshots separately.")
    text = json.dumps(result, indent=2)
    if args.out:
        require(not args.out.exists(), "Output exists; preserve prior evidence")
        args.out.write_text(text + "\n", encoding="utf-8")
    print(text)


if __name__ == "__main__":
    main()

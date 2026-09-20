extends RefCounted
# Bounded observation of the ordinary game. No fixture, orders, AI switches,
# quality changes or capture work are performed inside the measurement window.
const REQUIRED_STAGES := ["bridge", "snapshot", "present", "hud_process", "main_process"]
var game
var warmup_frames := 120
var measured_frames := 600
var active := false
var pending := false
var started_usec := 0
var previous_usec := 0
var measurement_started_usec := 0
var ended_usec := 0
var initial_snapshot: Dictionary = {}
var measurement_initial_snapshot: Dictionary = {}
var final_snapshot: Dictionary = {}
var warmup: Array[Dictionary] = []
var samples: Array[Dictionary] = []
var stage_calls: Dictionary = {}
var errors: Array[String] = []
var initial_scene_classes: Dictionary = {}
var initial_context: Dictionary = {}
var final_context: Dictionary = {}

func _init(owner) -> void:
	game = owner
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--profile-frames="):
			measured_frames = game._integer_option(argument, 1, 3600)
		elif argument.begins_with("--profile-warmup="):
			warmup_frames = game._integer_option(argument, 0, 1200)

func start() -> void:
	initial_snapshot = game.current.duplicate(true)
	_count_scene(game, initial_scene_classes)
	initial_context = _context()
	if warmup_frames == 0: measurement_initial_snapshot = initial_snapshot.duplicate(true)
	started_usec = Time.get_ticks_usec()
	previous_usec = started_usec
	if warmup_frames == 0: measurement_started_usec = started_usec
	active = true

func _count_scene(node: Node, counts: Dictionary) -> void:
	var kind := node.get_class()
	counts[kind] = int(counts.get(kind, 0)) + 1
	for child in node.get_children(): _count_scene(child, counts)

func _context() -> Dictionary:
	var alive_by_player := [0, 0]
	for unit in game.current.units:
		if int(unit.hp) > 0: alive_by_player[int(unit.player)] += 1
	return {"alive_by_player": alive_by_player, "selected": game.selected.size(),
		"camera_position": [game.camera.position.x, game.camera.position.y, game.camera.position.z],
		"camera_size": game.camera.size, "actors": game.actors.size()}

func record(stage: String, elapsed_usec: int) -> void:
	if not active: return
	if not stage_calls.has(stage): stage_calls[stage] = []
	stage_calls[stage].append(elapsed_usec)

func frame(delta: float) -> void:
	if not active or pending: return
	pending = true
	# Child HUD _process runs after the parent's callback. Collect afterward.
	_sample.call_deferred(delta)

func _sample(delta: float) -> void:
	pending = false
	if not active: return
	var now := Time.get_ticks_usec()
	var row := {
		"frame": warmup.size() + samples.size(),
		"elapsed_usec": now - started_usec,
		"wall_interval_usec": now - previous_usec,
		"process_delta_usec": delta * 1000000.0,
		"focused": DisplayServer.window_is_focused(),
		"minimized": DisplayServer.window_get_mode() == DisplayServer.WINDOW_MODE_MINIMIZED,
		"tick": int(game.current.tick),
		"state_hash": str(game.current.hash),
		"units": game.current.units.size(),
		"actors": game.actors.size(),
		"selected": game.selected.size(),
		"camera_position": [game.camera.position.x, game.camera.position.y, game.camera.position.z],
		"camera_size": game.camera.size,
		"alive_by_player": _context().alive_by_player,
		"bridge_calls_this_frame": stage_calls.get("bridge", []).size(),
		"stage_calls_usec": stage_calls,
		"performance": {
			"process_seconds": Performance.get_monitor(Performance.TIME_PROCESS),
			"physics_process_seconds": Performance.get_monitor(Performance.TIME_PHYSICS_PROCESS),
			"fps": Performance.get_monitor(Performance.TIME_FPS),
			"draw_calls": Performance.get_monitor(Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME),
			"render_objects": Performance.get_monitor(Performance.RENDER_TOTAL_OBJECTS_IN_FRAME),
			"primitives": Performance.get_monitor(Performance.RENDER_TOTAL_PRIMITIVES_IN_FRAME),
			"nodes": Performance.get_monitor(Performance.OBJECT_NODE_COUNT),
			"objects": Performance.get_monitor(Performance.OBJECT_COUNT),
			"video_memory_bytes": Performance.get_monitor(Performance.RENDER_VIDEO_MEM_USED),
			"static_memory_bytes": Performance.get_monitor(Performance.MEMORY_STATIC)
		}
	}
	stage_calls = {}
	previous_usec = now
	if warmup.size() < warmup_frames:
		warmup.append(row)
		if warmup.size() == warmup_frames:
			measurement_initial_snapshot = game.current.duplicate(true)
			measurement_started_usec = now
	else:
		samples.append(row)
	if samples.size() == measured_frames:
		ended_usec = now
		final_snapshot = game.current.duplicate(true)
		final_context = _context()
		active = false
		# Terminate only after the complete bounded measurement has been retained.
		game.completed = true
		_finish.call_deferred()

func _statistics(values: Array) -> Dictionary:
	if values.is_empty(): return {"count": 0}
	var sorted := values.duplicate()
	sorted.sort()
	var total := 0.0
	for value in values: total += float(value)
	return {"count": values.size(), "mean": total / values.size(),
		"p50": sorted[maxi(0, int(ceil(values.size() * 0.50)) - 1)],
		"p95": sorted[maxi(0, int(ceil(values.size() * 0.95)) - 1)],
		"p99": sorted[maxi(0, int(ceil(values.size() * 0.99)) - 1)],
		"maximum": sorted.back(), "total": total}

func _summarize(rows: Array[Dictionary]) -> Dictionary:
	var wall: Array = []
	var delta: Array = []
	var calls: Dictionary = {}
	var focused := 0
	for row in rows:
		wall.append(row.wall_interval_usec)
		delta.append(row.process_delta_usec)
		if row.focused: focused += 1
		for stage in row.stage_calls_usec:
			if not calls.has(stage): calls[stage] = []
			calls[stage].append_array(row.stage_calls_usec[stage])
	var stages: Dictionary = {}
	for stage in calls: stages[stage] = _statistics(calls[stage])
	return {"frames": rows.size(), "focused_frames": focused,
		"wall_interval_usec": _statistics(wall), "process_delta_usec": _statistics(delta),
		"stage_calls_usec": stages}

func _finish() -> void:
	var measured_summary := _summarize(samples)
	if samples.is_empty(): errors.append("No measured samples")
	if samples.size() != measured_frames: errors.append("Incomplete measured frame count")
	if warmup.size() != warmup_frames: errors.append("Incomplete warmup frame count")
	for stage in REQUIRED_STAGES:
		if measured_summary.stage_calls_usec.get(stage, {}).get("count", 0) == 0:
			errors.append("No measured stage calls: " + stage)
	if int(final_snapshot.tick) <= int(measurement_initial_snapshot.tick):
		errors.append("Simulation did not advance during measurement")
	var capture_error := OK
	if not game.capture_path.is_empty():
		# GPU readback and PNG encoding occur strictly after ended_usec.
		await RenderingServer.frame_post_draw
		capture_error = game.get_viewport().get_texture().get_image().save_png(game.capture_path)
		if capture_error != OK: errors.append("Post-measurement capture failed: %d" % capture_error)
	var report := {
		"schema": 1, "ok": errors.is_empty(), "errors": errors,
		"process_id": OS.get_process_id(),
		"mode": "ordinary_offline_presentation", "seed": 1,
		"map": "Scale128" if game.scale128 else "Foundry",
		"requested_units_per_team": game.scale_count if game.scale128 else 6,
		"ai_enabled": true, "smoke_fixture": false,
		"engine": Engine.get_version_info(),
		"renderer": RenderingServer.get_video_adapter_name(),
		"display_server": DisplayServer.get_name(),
		"rendering_method": RenderingServer.get_current_rendering_method(),
		"window_size": [DisplayServer.window_get_size().x, DisplayServer.window_get_size().y],
		"viewport_size": [game.get_viewport().size.x, game.get_viewport().size.y],
		"vsync_mode": DisplayServer.window_get_vsync_mode(),
		"max_fps": Engine.max_fps,
		"msaa_3d": game.get_viewport().msaa_3d,
		"started_usec": started_usec, "measurement_started_usec": measurement_started_usec,
		"ended_usec": ended_usec, "total_wall_usec": ended_usec - started_usec,
		"measurement_wall_usec": ended_usec - measurement_started_usec,
		"requested_warmup_frames": warmup_frames, "requested_measured_frames": measured_frames,
		"warmup_summary": _summarize(warmup), "measurement_summary": measured_summary,
		"warmup_samples": warmup, "measured_samples": samples,
		"initial_snapshot": initial_snapshot,
		"initial_context": initial_context, "final_context": final_context,
		"initial_scene_node_classes": initial_scene_classes,
		"measurement_initial_snapshot": measurement_initial_snapshot,
		"final_snapshot": final_snapshot,
		"foreground_entire_measurement": measured_summary.focused_frames == samples.size(),
		"capture_path": game.capture_path, "capture_after_measurement": not game.capture_path.is_empty(),
		"notes": [
			"Warmup and measured raw samples are retained without outlier or focus filtering; nearest-rank percentiles.",
			"Wall intervals are deferred sample-to-sample; first warmup interval starts after setup. Process delta is the engine-supplied value and may be capped.",
			"Stages are CPU call durations. Main process encloses bridge/snapshot/present and is not additive. Present excludes asynchronous skeleton/renderer/GPU work. Snapshot includes authoritative hashing and marshaling.",
			"HUD process and draw cover instrumented callbacks, not GPU UI rendering. Deferred sampling includes current child process; hud_draw may describe a prior submitted draw, and draw counts are observational, not required per frame.",
			"Performance monitors are engine counters with their own update cadence, usually describing prior work; they are not same-frame GPU timings.",
			"Profiling overhead remains in frame intervals. Warmup boundary includes snapshot copying in the next measured interval.",
			"A focused flag does not establish unoccluded foreground, frame presentation latency, human playability, or reference-hardware budget acceptance."
		]
	}
	var file := FileAccess.open(game.report_path, FileAccess.WRITE)
	if file == null:
		push_error("Cannot write presentation profile: " + game.report_path)
		game.get_tree().quit(2)
		return
	file.store_string(JSON.stringify(report, "\t"))
	file.close()
	print("VOIDFRONT_PROFILE ok=", report.ok, " frames=", samples.size(), " focused=", measured_summary.focused_frames)
	game.get_tree().quit(0 if report.ok else 2)

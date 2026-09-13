extends RefCounted
# Real InputEvents exercise production selection, groups and commands on Scale128.
# Tick hashes are captured by the production loop immediately after each advance.
var game
var checks: Array[String] = []
var errors: Array[String] = []
var inputs: Array[Dictionary] = []
var trace: Array[Dictionary] = []
var captures: Array[Dictionary] = []
var initial_snapshot: Dictionary
var stop_positions: Dictionary = {}
var stop_baseline: Array[Dictionary] = []
var stop_checks: Array[Dictionary] = []
var stop_tick := -1
var stop_samples := 0
var stop_drift := false
var moved_before_stop := false
var moved_after_resume := false
var label := ""
var stage := "setup"
var selected_ids: Array[int] = []

func _init(owner) -> void:
	game = owner
	initial_snapshot = game.current.duplicate(true)
	trace.append({"tick": int(game.current.tick), "hash": str(game.current.hash)})

func check(ok: bool, description: String) -> void:
	if ok: checks.append(description)
	else: errors.append(description)
	print("VOIDFRONT_SCALE_CHECK tick=", game.current.tick, " ok=", ok, " ", description)

func key(code: Key, control := false) -> void:
	var event := InputEventKey.new()
	event.physical_keycode = code
	event.pressed = true
	event.ctrl_pressed = control
	Input.parse_input_event(event)
	var release := InputEventKey.new()
	release.physical_keycode = code
	Input.parse_input_event(release)
	await game.get_tree().process_frame
	await game.get_tree().process_frame

func move_to_destination(next_label: String) -> void:
	label = next_label
	# Both axes exceed the old Foundry map. Project through the production camera.
	var screen: Vector2 = game.camera.unproject_position(Vector3(108.5, 0, 64.5))
	check(game.get_viewport().get_visible_rect().has_point(screen), next_label + " click is inside the viewport")
	var press := InputEventMouseButton.new()
	press.button_index = MOUSE_BUTTON_RIGHT
	press.position = screen
	press.pressed = true
	Input.parse_input_event(press)
	var release := InputEventMouseButton.new()
	release.button_index = MOUSE_BUTTON_RIGHT
	release.position = screen
	Input.parse_input_event(release)
	await game.get_tree().process_frame
	await game.get_tree().process_frame

func record_input(accepted: bool, order: int, at: Vector3) -> void:
	var event_tick: int = game.current.tick
	inputs.append({"label": label, "accepted": accepted, "order": order,
		"units": game.selected.duplicate(), "x": int(at.x * 256), "z": int(at.z * 256),
		"event_tick": event_tick, "application_tick": event_tick + 1})
	if not accepted: errors.append("Rejected input: " + label)
	if accepted and label == "stop":
		stop_tick = event_tick + 1
		# Baseline precedes the first application tick; first-tick drift cannot hide.
		for unit in game.current.units:
			if unit.id in selected_ids:
				stop_positions[int(unit.id)] = Vector2i(unit.x, unit.z)
				stop_baseline.append({"id": int(unit.id), "x": int(unit.x), "z": int(unit.z)})
		stage = "stopped"
	elif accepted and label == "resume": stage = "resumed"
	elif accepted and label == "move": stage = "moving"

func tick() -> void:
	var current: Dictionary = game.current
	if int(current.tick) != int(trace.back().tick) + 1:
		errors.append("Missing or repeated simulation tick: %d" % current.tick)
	trace.append({"tick": int(current.tick), "hash": str(current.hash)})
	if stage == "moving":
		for unit in current.units:
			if unit.id in selected_ids and Vector2(unit.x, unit.z) != game.initial_positions[unit.id]:
				moved_before_stop = true
	elif stage == "stopped" and current.tick >= stop_tick:
		stop_samples += 1
		var checked := 0
		var drift_ids: Array[int] = []
		for unit in current.units:
			if unit.id in selected_ids:
				checked += 1
				if Vector2i(unit.x, unit.z) != stop_positions[unit.id] or unit.moving or unit.order != 0:
					stop_drift = true
					drift_ids.append(int(unit.id))
		stop_checks.append({"tick": int(current.tick), "checked_units": checked, "drift_ids": drift_ids})
	elif stage == "resumed":
		for unit in current.units:
			if unit.id in selected_ids and stop_positions.has(unit.id) and Vector2i(unit.x, unit.z) != stop_positions[unit.id]:
				moved_after_resume = true

func capture(capture_label: String, path: String) -> void:
	await RenderingServer.frame_post_draw
	var screenshot: Image = game.get_viewport().get_texture().get_image()
	var result: int = screenshot.save_png(path)
	captures.append({"label": capture_label, "tick": int(game.current.tick), "path": path,
		"error": result, "width": screenshot.get_width(), "height": screenshot.get_height(),
		"camera_size": game.camera.size, "camera_target_x": game.camera_target.x,
		"camera_target_z": game.camera_target.z})
	check(result == OK and screenshot.get_width() > 0 and screenshot.get_height() > 0,
		capture_label + " screenshot saved")

func run() -> void:
	check(initial_snapshot.tick == 0, "Fixture attached before simulation advancement")
	check(game.scale128 and game.map_size == Vector2i(128, 128), "Scale mode has 128x128 presentation bounds")
	check(initial_snapshot.width == 128 and initial_snapshot.height == 128, "Authoritative map is 128x128")
	check(initial_snapshot.units.size() == 2 * game.scale_count and game.actors.size() == 2 * game.scale_count,
		"Snapshot and rendered actor counts equal twice the requested army count")
	check(initial_snapshot.has("protocol") and initial_snapshot.has("content_id") and initial_snapshot.has("map_id"),
		"Snapshot includes replay compatibility metadata")
	await key(KEY_F2)
	selected_ids.assign(game.selected)
	var expected: Array[int] = []
	for unit in game.current.units:
		if unit.player == game.local_player and unit.hp > 0: expected.append(int(unit.id))
	check(selected_ids == expected and selected_ids.size() == game.scale_count, "F2 selects every own unit and no foreign units")
	await key(KEY_1, true)
	check(game.control_groups.get(1, []) == selected_ids, "Ctrl1 saves the entire scale army")
	# Clear presentation selection, then require production keyboard recall to restore it.
	game.selected.clear()
	await key(KEY_1)
	check(game.selected == selected_ids, "1 recalls the entire saved scale army")
	await move_to_destination("move")
	check(inputs.size() == 1 and inputs[0].accepted and inputs[0].x > 32 * 256 and inputs[0].z > 24 * 256,
		"Production right click accepts destination beyond both former map bounds")
	while game.current.tick < 25 and game.current.tick < game.finish_tick:
		await game.get_tree().process_frame
	check(moved_before_stop, "Selected army moved before Stop")
	label = "stop"
	await key(KEY_S)
	while stop_samples < 9 and game.current.tick < 60:
		await game.get_tree().process_frame
	check(stop_tick > 0 and stop_positions.size() == game.scale_count and stop_samples >= 9 and not stop_drift,
		"Stop holds every selected unit from first application through nine ticks")
	await move_to_destination("resume")
	check(inputs.size() == 3 and inputs.back().accepted and inputs.back().label == "resume", "Production right click resumes the scale army")
	if not game.capture_path.is_empty():
		await capture("wide", game.capture_path.get_basename() + "-wide.png")
	while game.current.tick < game.finish_tick:
		await game.get_tree().process_frame
	game.completed = true
	var final_snapshot: Dictionary = game.current.duplicate(true)
	check(moved_after_resume, "Selected army moved after resume")
	check(trace.size() == game.finish_tick + 1 and trace.back().tick == game.finish_tick,
		"Full tick hash trace includes tick zero through the requested finish")
	check(game.actors.size() == 2 * game.scale_count, "All scale actors remain instantiated at finish")
	game.camera_target = Vector3(64, 0, 64)
	game.camera.size = 27
	game._update_camera()
	await game.get_tree().process_frame
	if not game.capture_path.is_empty(): await capture("choke", game.capture_path)
	else: check(false, "Screenshot output path is required")
	var frame_p95: float = game._percentile(game.frame_times, 0.95)
	var frame_p99: float = game._percentile(game.frame_times, 0.99)
	var sim_p95: float = game._percentile(game.sim_times, 0.95)
	var sim_p99: float = game._percentile(game.sim_times, 0.99)
	await key(KEY_R)
	check(game.current.tick == 0 and game.current.width == 128 and game.current.height == 128 and game.map_size == Vector2i(128, 128),
		"R restarts with the same 128x128 map")
	check(game.current.units.size() == 2 * game.scale_count and game.actors.size() == 2 * game.scale_count,
		"R preserves the configured scale army count")
	check(game.control_groups.is_empty() and game.selected.is_empty(), "R clears saved groups and selection")
	check(str(game.current.hash) == str(initial_snapshot.hash), "R restores the exact initial deterministic state")
	var report := {"ok": errors.is_empty(), "mode": "scale", "seed": 1, "count": game.scale_count,
		"units_per_team": game.scale_count,
		"setup": {"map_id": initial_snapshot.get("map_id"), "width": initial_snapshot.width,
			"height": initial_snapshot.height, "protocol": initial_snapshot.get("protocol"),
			"content_id": initial_snapshot.get("content_id"), "ai_enabled": false},
		"initial_snapshot": initial_snapshot, "final_snapshot": final_snapshot,
		"tick": final_snapshot.tick, "hash": final_snapshot.hash, "inputs": inputs, "trace": trace,
		"captures": captures, "checks": checks, "errors": errors,
		"stop_tick": stop_tick, "stop_samples": stop_samples, "stop_drift": stop_drift,
		"stop_baseline": stop_baseline, "stop_checks": stop_checks,
		"frame_interval_ms_p95": frame_p95, "frame_interval_ms_p99": frame_p99,
		"sim_ms_p95": sim_p95, "sim_ms_p99": sim_p99,
		"frame_times_ms": game.frame_times, "bridge_step_times_ms": game.sim_times,
		"frame_samples": game.frame_times.size(), "sim_samples": game.sim_times.size(),
		"renderer": RenderingServer.get_video_adapter_name(),
		"note": "Packaged scale InputEvent and tick-hash evidence. Frame intervals include rendering and any capture cost; sim timing covers bridge.advance only. Stop baseline precedes first application. Goals are absent from bridge snapshots; no arrival claim. Screenshots require human or independent visual inspection. No complete-match, crowd-progress, hardware-budget or human responsiveness acceptance."}
	var file := FileAccess.open(game.report_path, FileAccess.WRITE)
	if file: file.store_string(JSON.stringify(report, "\t"))
	else:
		errors.append("Cannot write scale test report: " + game.report_path)
		push_error(errors.back())
	print("VOIDFRONT_SCALE ok=", errors.is_empty(), " count=", game.scale_count, " tick=", final_snapshot.tick)
	game.get_tree().quit(0 if errors.is_empty() else 1)

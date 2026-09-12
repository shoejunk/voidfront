extends RefCounted
# Scripted InputEvents exercise the same selection/order path as normal play.
var game
var initial: Array
var stage := 0
var inputs: Array[Dictionary] = []
var samples: Array[Dictionary] = []
var captures: Array[Dictionary] = []
var errors: Array[String] = []
var swap_tick := -1
var stop_tick := -1
var stop_position := Vector2i.ZERO
var stop_samples := 0
var captured_detour := false

func _init(owner) -> void:
	game = owner
	initial = game.current.units.duplicate(true)

func record_input(accepted: bool, order: int, at: Vector3) -> void:
	inputs.append({"label": game.movement_label, "accepted": accepted, "order": order,
		"units": game.selected.duplicate(), "x": int(at.x * 256), "z": int(at.z * 256), "event_tick": game.current.tick})
	if not accepted: errors.append("Rejected " + game.movement_label)

func last_input(label: String) -> Dictionary:
	for command in inputs:
		if command.label == label and command.accepted: return command
	return {}

func arrived(unit: Dictionary, label: String) -> bool:
	var command := last_input(label)
	return not command.is_empty() and unit.x == command.x and unit.z == command.z and unit.order == 0 and not unit.moving

func select_unit(id: int) -> void:
	var at = game.camera.unproject_position(game.actors[id].root.position + Vector3(0, 0.5, 0))
	game._smoke_mouse(MOUSE_BUTTON_LEFT, at, true)
	game._smoke_mouse(MOUSE_BUTTON_LEFT, at, false)

func tick() -> void:
	var current = game.current
	var unit: Dictionary = current.units[0]
	samples.append({"tick": current.tick, "units": current.units.duplicate(true)})
	if stage == 0 and current.tick == 1: select_unit(1)
	if stage == 0 and current.tick >= 3 and game.selected == [1]:
		game._movement_input("swap_one", Vector3(initial[1].x / 256.0, 0, initial[1].z / 256.0))
		stage = 1
	elif stage == 1 and not last_input("swap_one").is_empty() and current.tick >= last_input("swap_one").event_tick + 2:
		select_unit(2)
		stage = 2
	elif stage == 2 and game.selected == [2]:
		game._movement_input("swap_two", Vector3(initial[0].x / 256.0, 0, initial[0].z / 256.0))
		stage = 3
	elif stage == 3:
		if not captured_detour and absi(unit.x - initial[0].x) > 32:
			captured_detour = true
			capture.call_deferred("swap-detour", int(current.tick))
		if arrived(unit, "swap_one") and arrived(current.units[1], "swap_two"):
			swap_tick = current.tick
			capture.call_deferred("swap-arrived", int(current.tick))
			select_unit(1)
			stage = 4
	elif stage == 4 and game.selected == [1]:
		game._movement_input("chain", Vector3(2.5, 0, 16.796875))
		stage = 5
	elif stage == 5 and not last_input("chain").is_empty() and current.tick > last_input("chain").event_tick + 5 and absi(unit.x - last_input("chain").x) > 32:
		game.movement_label = "stop"
		game._smoke_key(KEY_S)
		stage = 6
	elif stage == 6 and not last_input("stop").is_empty() and current.tick > last_input("stop").event_tick:
		if stop_tick < 0:
			stop_tick = current.tick
			stop_position = Vector2i(unit.x, unit.z)
		stop_samples += 1
		if Vector2i(unit.x, unit.z) != stop_position or unit.moving: errors.append("Stop drift")
		if stop_samples >= 9:
			capture.call_deferred("stopped", int(current.tick))
			game._movement_input("resume", Vector3(2.5, 0, 16.796875))
			stage = 7
	elif stage == 7 and arrived(unit, "resume"):
		capture.call_deferred("chain-arrived", int(current.tick))
		stage = 8

func capture(label: String, tick_number: int) -> void:
	await RenderingServer.frame_post_draw
	var path: String = game.capture_path.get_basename() + "-" + label + ".png"
	var error: int = game.get_viewport().get_texture().get_image().save_png(path)
	captures.append({"label": label, "tick": tick_number, "path": path, "error": error})
	if error != OK: errors.append("Capture failed: " + label)

func finish() -> void:
	await RenderingServer.frame_post_draw
	var ok: bool = game.option_error.is_empty() and errors.is_empty() and stage == 8 and inputs.size() == 5 and samples.size() == game.finish_tick and captured_detour and stop_samples >= 9
	if not game.capture_path.is_empty():
		if game.get_viewport().get_texture().get_image().save_png(game.capture_path) != OK: ok = false
	var report := {"ok": ok, "mode": "crowd", "tick": game.current.tick, "hash": game.current.hash,
		"initial": initial, "inputs": inputs, "positions": samples, "captures": captures, "stage": stage,
		"swap_tick": swap_tick, "stop_tick": stop_tick, "stop_samples": stop_samples, "errors": errors,
		"option_error": game.option_error, "frame_interval_ms_p95": game._percentile(game.frame_times, 0.95),
		"frame_interval_ms_p99": game._percentile(game.frame_times, 0.99),
		"sim_ms_p95": game._percentile(game.sim_times, 0.95), "sim_ms_p99": game._percentile(game.sim_times, 0.99),
		"note": "Packaged InputEvent swap, stationary chain, detour Stop/resume. Twelve units, enemy AI disabled. Bounded fixture; no human/crowded-choke/representative-scale acceptance. Movie timings include encoding."}
	var file := FileAccess.open(game.report_path, FileAccess.WRITE)
	if file: file.store_string(JSON.stringify(report, "\t"))
	else: ok = false
	print("VOIDFRONT_CROWD ok=", ok, " stage=", stage, " tick=", game.current.tick)
	game.get_tree().quit(0 if ok else 1)

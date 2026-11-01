extends RefCounted
# Runs only after a real two-client session confirms its terminal state.
var game
var checks: Array[String] = []
var errors: Array[String] = []
var states: Array[Dictionary] = []
var inputs: Array[Dictionary] = []
var captures: Array[Dictionary] = []
var recording_input := false

func _init(owner) -> void:
	game = owner

func check(ok: bool, message: String) -> void:
	if ok: checks.append(message)
	else: errors.append(message)
	print("VOIDFRONT_SUCCESS_RETURN tick=", game.current.tick, " ok=", ok, " ", message)

func frames() -> void:
	await game.get_tree().process_frame
	await game.get_tree().process_frame

func until(predicate: Callable) -> bool:
	var deadline := Time.get_ticks_usec() + 3000000
	while not predicate.call() and Time.get_ticks_usec() < deadline:
		await game.get_tree().process_frame
	return bool(predicate.call())

func state(label: String) -> void:
	states.append({"label": label, "snapshot": game.current.duplicate(true),
		"client_network": game.network, "bridge_status": game.bridge.network_status(),
		"local_player": game.local_player, "result_text": game.hud.result.text,
		"connection_text": game.hud.connection.text})

func record_input(accepted: bool, order: int, at: Vector3, actors: Array) -> void:
	if not recording_input: return
	inputs.append({"label": "offline_move_after_complete", "accepted": accepted,
		"order": order, "units": actors.duplicate(), "x": int(at.x * 256),
		"z": int(at.z * 256), "event_tick": game.current.tick,
		"before_snapshot": game.current.duplicate(true)})

func run() -> Dictionary:
	var original_player: int = game.local_player
	check(game.network and game.network_state.get("state", "") == "complete", "Shared endpoint is complete before R")
	check(game.current.tick == game.network_state.get("confirmed_ticks", -1), "Terminal tick is confirmed before detaching")
	state("terminal")
	game._smoke_key(KEY_R)
	await frames()
	check(not game.network and game.local_player == 0 and game.option_error.is_empty(), "R returns either role to offline player zero")
	check(game.bridge.network_status().get("state", "") == "offline", "R releases the native network session")
	check(game.current.map_id == 2 and game.current.units.size() == 6 and game.current.winner == -1 and game.current.enemy_ai == game.setup_ai, "R restores economy and selected opponent settings")
	check(game.selected.is_empty() and game.control_groups.is_empty() and game.selected_entity.is_empty(), "R clears shared selection and groups")
	check(await until(func(): return game.current.tick >= 1), "Returned economy starts authoritative ticking")
	state("offline_reset")
	var reset_tick: int = game.current.tick
	check(await until(func(): return game.current.tick >= reset_tick + 4), "Offline simulation ticks advance after successful return")
	game._smoke_key(KEY_F1)
	await frames()
	var workers: Array = game.current.units.filter(func(u): return u.player == 0 and u.kind == 1 and u.hp > 0)
	check(game.selected.size() == 3 and workers.all(func(u): return u.id in game.selected), "F1 selects the three offline-owned workers")
	var positions := {}
	for unit in workers: positions[unit.id] = Vector2(unit.x, unit.z)
	recording_input = true
	var screen: Vector2 = game.camera.unproject_position(Vector3(5.5, 0, 14.5))
	game._smoke_mouse(MOUSE_BUTTON_RIGHT, screen, true)
	game._smoke_mouse(MOUSE_BUTTON_RIGHT, screen, false)
	await frames()
	recording_input = false
	check(inputs.size() == 1 and inputs[0].accepted and inputs[0].order == 1 and inputs[0].units.size() == 3, "Offline RMB submits an accepted three-worker canonical Move")
	check(await until(func():
		for unit in game.current.units:
			if positions.has(unit.id) and Vector2(unit.x, unit.z) != positions[unit.id]: return true
		return false), "Returned workers move in authoritative offline state")
	state("offline_movement")
	game.completed = true
	var replay: PackedByteArray = game.bridge.replay_bytes()
	var replay_path: String = game.report_path.get_basename() + "-offline.vfr"
	var file := FileAccess.open(replay_path, FileAccess.WRITE)
	check(not replay.is_empty() and file != null, "Offline command recording can be saved independently")
	if file:
		file.store_buffer(replay)
		file.close()
	if not game.capture_path.is_empty():
		await RenderingServer.frame_post_draw
		var path: String = game.capture_path.get_basename() + "-offline-return.png"
		var result: int = game.get_viewport().get_texture().get_image().save_png(path)
		captures.append({"label": "offline-return", "path": path, "error": result,
			"tick": game.current.tick, "snapshot": game.current.duplicate(true)})
		check(result == OK, "Saved offline-return screenshot")
	return {"ok": errors.is_empty(), "checks": checks, "errors": errors,
		"states": states, "inputs": inputs, "captures": captures,
		"replay_path": replay_path, "original_player": original_player}

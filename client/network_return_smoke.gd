extends RefCounted
# Real absent-peer timeout followed by ordinary R/F1/RMB InputEvents.
# This fixture never substitutes a synthetic network state or simulation state.
var game
var checks: Array[String] = []
var errors: Array[String] = []
var inputs: Array[Dictionary] = []
var captures: Array[Dictionary] = []
var states: Array[Dictionary] = []
var label := ""
var done := false
var started_usec := 0

func _init(owner) -> void:
	game = owner

func check(ok: bool, message: String) -> void:
	if ok: checks.append(message)
	else: errors.append(message)
	print("VOIDFRONT_NETRETURN_CHECK tick=", game.current.tick, " ok=", ok, " ", message)

func record_input(accepted: bool, order: int, at: Vector3, actors: Array) -> void:
	inputs.append({"label": label, "accepted": accepted, "order": order, "units": actors.duplicate(),
		"x": int(at.x * 256), "z": int(at.z * 256), "event_tick": game.current.tick})

func frames() -> void:
	await game.get_tree().process_frame
	await game.get_tree().process_frame

func key(code: Key) -> void:
	game._smoke_key(code)
	await frames()

func until(predicate: Callable, seconds: float) -> bool:
	var deadline: int = Time.get_ticks_usec() + int(seconds * 1000000.0)
	while not predicate.call() and Time.get_ticks_usec() < deadline:
		await game.get_tree().process_frame
	return bool(predicate.call())

func state(label_text: String) -> void:
	states.append({"label": label_text, "snapshot": game.current.duplicate(true),
		"client_network": game.network, "bridge_status": game.bridge.network_status(),
		"local_player": game.local_player, "result_text": game.hud.result.text,
		"connection_text": game.hud.connection.text, "notice": game.network_notice,
		"elapsed_ms": (Time.get_ticks_usec() - started_usec) / 1000.0})

func capture(name: String) -> void:
	if game.capture_path.is_empty(): return
	await RenderingServer.frame_post_draw
	var path: String = game.capture_path.get_basename() + "-" + name + ".png"
	var result: int = game.get_viewport().get_texture().get_image().save_png(path)
	captures.append({"label": name, "tick": game.current.tick, "path": path, "error": result})
	check(result == OK, "Saved " + name + " screenshot")

func run() -> void:
	started_usec = Time.get_ticks_usec()
	await frames()
	check(game.network and game.economy and game.current.map_id == 2, "Starts an actual networked economy client")
	check(str(game.network_state.get("state", "")) == "handshake" and not game.network_state.get("ready", false), "Absent peer leaves handshake pending")
	var handshake_hash: String = game.current.hash
	state("handshake")
	await key(KEY_R)
	check(game.network and game.current.hash == handshake_hash and game.current.tick == 0,
		"R cannot replace a pending shared match")
	check(game.network_notice.contains("Shared match active"), "Rejected early R has explicit shared-match feedback")
	state("early_restart_rejected")
	var timed_out: bool = await until(func(): return str(game.network_state.get("state", "")) == "error", 12.0)
	check(timed_out, "Actual absent-peer session times out within watchdog")
	check(str(game.network_state.get("error", "")).contains("handshake") and game.current.tick == 0,
		"Handshake timeout reports a reason and advances no authoritative ticks")
	await frames()
	check(game.hud.result.text.contains("return to offline skirmish"), "Error HUD offers the actual R action")
	state("timeout")
	await capture("timeout")
	if timed_out:
		await key(KEY_R)
		check(not game.network and game.local_player == 0 and game.option_error.is_empty(), "R returns the client to offline player zero")
		check(str(game.bridge.network_status().get("state", "")) == "offline", "Offline reset detaches the native session")
		check(game.current.map_id == 2 and game.current.units.size() == 6 and game.current.winner == -1,
			"Offline reset recreates economy workers and an unfinished match")
		state("offline_reset")
		var reset_tick: int = game.current.tick
		check(await until(func(): return game.current.tick >= reset_tick + 4, 2.0), "Offline authoritative ticks advance after return")
		await key(KEY_F1)
		var own_only: bool = game.selected.size() == 3
		for unit in game.current.units:
			if unit.id in game.selected and (unit.player != 0 or unit.kind != 1 or unit.hp <= 0): own_only = false
		check(own_only, "F1 selects exactly three living offline-owned workers")
		var positions := {}
		for unit in game.current.units:
			if unit.id in game.selected: positions[unit.id] = Vector2(unit.x, unit.z)
		label = "offline_move_after_timeout"
		var screen: Vector2 = game.camera.unproject_position(Vector3(5.5, 0, 14.5))
		game._smoke_mouse(MOUSE_BUTTON_RIGHT, screen, true)
		game._smoke_mouse(MOUSE_BUTTON_RIGHT, screen, false)
		await frames()
		check(inputs.size() == 1 and inputs[0].accepted and inputs[0].order == 1 and inputs[0].units.size() == 3,
			"Offline RMB submits one accepted canonical Move after timeout")
		check(await until(func():
			for unit in game.current.units:
				if positions.has(unit.id) and Vector2(unit.x, unit.z).distance_squared_to(positions[unit.id]) > 0: return true
			return false, 2.0), "Offline workers move authoritatively after the returned client order")
		state("offline_movement")
		await capture("offline")
	done = true
	game.completed = true
	var result := report()
	if not game.report_path.is_empty():
		var file := FileAccess.open(game.report_path, FileAccess.WRITE)
		if file:
			file.store_string(JSON.stringify(result, "\t"))
			file.close()
		else:
			check(false, "Report file could not be opened")
	print("VOIDFRONT_NETRETURN ok=", errors.is_empty(), " tick=", game.current.tick,
		" checks=", checks.size(), " errors=", errors)
	game.get_tree().quit(0 if errors.is_empty() else 1)

func report() -> Dictionary:
	return {"ok": done and errors.is_empty(), "fixture_done": done, "mode": "network_return", "checks": checks,
		"errors": errors, "inputs": inputs, "states": states, "captures": captures,
		"renderer": RenderingServer.get_video_adapter_name(),
		"note": "One client, real absent-peer handshake timeout and synthetic ordinary R/F1/RMB InputEvents. Proves pending-match reset rejection and error return to ticking offline gameplay; launch/package identity comes from the host record. No successful-peer rematch, human input, network victory, performance or shipping claim."}

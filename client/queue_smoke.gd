extends RefCounted
# Passive observation of ordinary InputEvents; authoritative state is never injected.
var game
var initial: Dictionary
var samples: Array[Dictionary] = []
var inputs: Array[Dictionary] = []
var phases: Array[Dictionary] = []
var captures: Array[Dictionary] = []
var checks: Array[String] = []
var errors: Array[String] = []
var label := ""
var last_tick := -1

func _init(owner) -> void:
	game = owner
	initial = game.current.duplicate(true)
	samples.append(initial)
	last_tick = int(initial.tick)

func check(ok: bool, message: String) -> void:
	if ok: checks.append(message)
	else: errors.append(message)
	print("VOIDFRONT_QUEUE_CHECK tick=", game.current.tick, " ok=", ok, " ", message)

func tick() -> void:
	if int(game.current.tick) > last_tick:
		samples.append(game.current.duplicate(true))
		last_tick = int(game.current.tick)

func record_input(accepted: bool, order: int, at: Vector3, actors: Array[int]) -> void:
	inputs.append({"label": label, "accepted": accepted, "order": order,
		"x": int(at.x * 256), "z": int(at.z * 256), "units": actors.duplicate(), "event_tick": game.current.tick})

func worker() -> Dictionary:
	for unit in game.current.units:
		if unit.id == 1: return unit
	return {}

func frames() -> void:
	await game.get_tree().process_frame
	await game.get_tree().process_frame

func key(code: Key, shift := false) -> void:
	for pressed in [true, false]:
		var event := InputEventKey.new()
		event.physical_keycode = code
		event.pressed = pressed
		event.shift_pressed = shift
		Input.parse_input_event(event)
	await frames()

func mouse(button: MouseButton, point: Vector3, shift := false) -> void:
	var screen: Vector2 = game.camera.unproject_position(point)
	for pressed in [true, false]:
		var event := InputEventMouseButton.new()
		event.button_index = button
		event.position = game.get_viewport().get_final_transform() * screen
		event.pressed = pressed
		event.shift_pressed = shift
		Input.parse_input_event(event)
	await frames()

func until(predicate: Callable, limit: int) -> bool:
	while game.current.tick < mini(limit, game.finish_tick) and not predicate.call():
		await game.get_tree().process_frame
	return bool(predicate.call())

func advance(count: int) -> bool:
	var target: int = game.current.tick + count
	return await until(func(): return game.current.tick >= target, target)

func phase(name: String) -> void:
	phases.append({"label": name, "tick": game.current.tick, "selected": game.selected.duplicate(),
		"paths": game._order_paths().duplicate(true)})

func capture(name: String) -> void:
	if game.capture_path.is_empty(): return
	await RenderingServer.frame_post_draw
	var path: String = game.capture_path.get_basename() + "-" + name + ".png"
	var result: int = game.get_viewport().get_texture().get_image().save_png(path)
	captures.append({"label": name, "tick": game.current.tick, "path": path, "error": result})
	check(result == OK, "Saved " + name + " screenshot")

func leg(name: String, point: Vector3, attack := false, shift := true) -> void:
	label = name
	if attack: await key(KEY_A, shift)
	await mouse(MOUSE_BUTTON_LEFT if attack else MOUSE_BUTTON_RIGHT, point, shift)

func exercise() -> void:
	await frames()
	check(initial.map_id == 2 and initial.tick == 0 and initial.units.size() == 6, "Passive economy worker setup")
	var unit: Dictionary = worker()
	await mouse(MOUSE_BUTTON_LEFT, Vector3(unit.x / 256.0, 0.5, unit.z / 256.0))
	check(game.selected == [1], "LMB selects one worker")
	phase("selected_idle")
	await leg("idle_shift_move", Vector3(10.25, 0, 10.25))
	await advance(2)
	check(worker().order == 1 and worker().order_queue.is_empty(), "Idle Shift move starts immediately")
	await leg("fifo_second", Vector3(12.25, 0, 10.25))
	await leg("fifo_attack", Vector3(12.25, 0, 14.25), true)
	await leg("fifo_fourth", Vector3(9.25, 0, 14.25))
	await leg("fifo_fifth", Vector3(9.25, 0, 11.25))
	await advance(2)
	check(worker().order_queue.size() == 4, "Four pending legs fill queue")
	phase("filled")
	await capture("filled")
	await leg("overflow_ignored", Vector3(13.25, 0, 11.25))
	await advance(2)
	check(worker().order_queue.size() == 4, "Excess queued leg is ignored")
	phase("overflow")
	await mouse(MOUSE_BUTTON_LEFT, Vector3(11.25, 0, 16.25))
	check(game.selected.is_empty() and game._order_paths().is_empty(), "Deselection hides queued paths")
	phase("deselected")
	unit = worker()
	await mouse(MOUSE_BUTTON_LEFT, Vector3(unit.x / 256.0, 0.5, unit.z / 256.0))
	check(game.selected == [1] and not game._order_paths().is_empty(), "Selection restores queued paths")
	phase("reselected")
	var drained: bool = await until(func(): return worker().order == 0 and worker().order_queue.is_empty(), game.finish_tick - 220)
	check(drained, "FIFO route reaches final exact goal")
	if not drained: return
	phase("drained")
	await capture("drained")
	await leg("stop_active", Vector3(13.25, 0, 11.25), false, false)
	await leg("stop_tail", Vector3(13.25, 0, 14.25))
	await advance(2)
	phase("before_stop")
	label = "stop_clear"
	await key(KEY_S)
	await advance(5)
	check(worker().order == 0 and worker().order_queue.is_empty(), "Stop clears queue and halts")
	phase("stopped")
	await capture("stopped")
	await leg("hold_active", Vector3(13.25, 0, 14.25), false, false)
	await leg("hold_tail", Vector3(10.25, 0, 14.25))
	await advance(2)
	phase("before_hold")
	label = "hold_clear"
	await key(KEY_H)
	await advance(5)
	check(worker().order == 3 and worker().order_queue.is_empty(), "Hold clears queue and halts")
	phase("held")
	await leg("retarget_active", Vector3(13.25, 0, 14.25), false, false)
	await leg("retarget_tail", Vector3(10.25, 0, 14.25))
	await advance(2)
	phase("before_retarget")
	await leg("plain_retarget", Vector3(10.25, 0, 10.25), false, false)
	await advance(2)
	check(worker().order_queue.is_empty(), "Plain movement replaces queued route")
	phase("retargeted")
	check(await until(func(): return worker().order == 0, game.finish_tick - 40), "Plain retarget arrives exactly")
	phase("retarget_arrived")
	await leg("restart_active", Vector3(13.25, 0, 14.25), false, false)
	await leg("restart_tail", Vector3(10.25, 0, 14.25))
	await advance(2)
	phase("before_restart")

func run() -> void:
	await exercise()
	check(game.current.tick < game.finish_tick, "Fixture completed inside canonical tick bound")
	var final_snapshot: Dictionary = game.current.duplicate(true)
	game.completed = true
	var replay_path: String = game.report_path.get_basename() + ".vfr"
	var replay: PackedByteArray = game.bridge.replay_bytes()
	var replay_file := FileAccess.open(replay_path, FileAccess.WRITE)
	check(replay_file != null and not replay.is_empty(), "Replay available")
	if replay_file:
		replay_file.store_buffer(replay)
		replay_file.close()
	if not game.capture_path.is_empty():
		await RenderingServer.frame_post_draw
		check(game.get_viewport().get_texture().get_image().save_png(game.capture_path) == OK, "Final screenshot saved")
	await key(KEY_R)
	var restart_snapshot: Dictionary = game.current.duplicate(true)
	phase("restarted")
	check(restart_snapshot.tick == 0 and restart_snapshot.hash == initial.hash, "Restart restores canonical initial state")
	check(game.selected.is_empty() and game._order_paths().is_empty(), "Restart clears selected queued presentation")
	var trace: Array = []
	for snapshot in samples: trace.append({"tick": snapshot.tick, "hash": snapshot.hash})
	var report := {"ok": errors.is_empty(), "mode": "queue", "initial_snapshot": initial, "final_snapshot": final_snapshot,
		"restart_snapshot": restart_snapshot, "snapshots": samples, "trace": trace, "inputs": inputs,
		"phases": phases, "checks": checks, "errors": errors, "captures": captures, "replay_path": replay_path,
		"note": "Passive packaged offline software InputEvents. No human play, network, responsiveness, balance or quality acceptance."}
	var output := FileAccess.open(game.report_path, FileAccess.WRITE)
	if output: output.store_string(JSON.stringify(report, "\t"))
	else: errors.append("Cannot save report")
	print("VOIDFRONT_QUEUE ok=", errors.is_empty(), " tick=", final_snapshot.tick, " errors=", errors)
	game.get_tree().quit(0 if errors.is_empty() else 1)

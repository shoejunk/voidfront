extends RefCounted
# Uses the ordinary UI InputEvent path. All recorded state comes from the bridge.
var game
var initial: Dictionary
var samples: Array[Dictionary] = []
var inputs: Array[Dictionary] = []
var checks: Array[String] = []
var errors: Array[String] = []
var captures: Array[Dictionary] = []
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
	print("VOIDFRONT_ECONOMY_CHECK tick=", game.current.tick, " ok=", ok, " ", message)

func tick() -> void:
	if int(game.current.tick) > last_tick:
		samples.append(game.current.duplicate(true))
		last_tick = int(game.current.tick)
	if game.current.tick >= game.finish_tick: game.completed = true

func record_input(accepted: bool, order: int, at: Vector3) -> void:
	inputs.append({"label": label, "accepted": accepted, "order": order, "units": game.selected.duplicate(),
		"x": int(at.x * 256), "z": int(at.z * 256), "event_tick": game.current.tick})

func frames() -> void:
	await game.get_tree().process_frame
	await game.get_tree().process_frame

func key(code: Key) -> void:
	game._smoke_key(code)
	await frames()

func mouse(button: MouseButton, point: Vector3) -> void:
	var screen: Vector2 = game.camera.unproject_position(point)
	game._smoke_mouse(button, screen, true)
	game._smoke_mouse(button, screen, false)
	await frames()

func until(predicate: Callable, limit: int) -> bool:
	while game.current.tick < mini(limit, game.finish_tick) and not predicate.call():
		await game.get_tree().process_frame
	return bool(predicate.call())

func advance_ticks(count: int) -> void:
	var target: int = game.current.tick + count
	await until(func(): return game.current.tick >= target, target)

func carried() -> int:
	var total := 0
	for unit in game.current.units:
		if unit.player == 0: total += int(unit.cargo)
	return total

func own_foundries() -> Array:
	return game.current.structures.filter(func(s): return s.player == 0 and s.kind == 1)

func capture(name: String) -> void:
	if game.capture_path.is_empty(): return
	await RenderingServer.frame_post_draw
	var path: String = game.capture_path.get_basename() + "-" + name + ".png"
	var result: int = game.get_viewport().get_texture().get_image().save_png(path)
	captures.append({"label": name, "tick": game.current.tick, "path": path, "error": result})
	check(result == OK, "Saved " + name + " screenshot")

func run() -> void:
	await frames()
	check(initial.map_id == 2 and initial.units.size() == 6 and initial.structures.size() == 2, "Economy map initialized with anchors and workers")
	var deposit: Dictionary = initial.deposits[0]
	var anchor: Dictionary = initial.structures[0]
	await mouse(MOUSE_BUTTON_LEFT, Vector3(deposit.x / 256.0, 0.5, deposit.z / 256.0))
	check(game.selected_entity.get("category", "") == "deposit", "Deposit click exposes resource readout")
	var worker: Dictionary = initial.units[0]
	await mouse(MOUSE_BUTTON_LEFT, Vector3(worker.x / 256.0, 0.5, worker.z / 256.0))
	check(game.selected == [worker.id] and game.selected_entity.is_empty(), "Worker click selects worker and clears deposit readout")
	await key(KEY_F1)
	check(game.selected.size() == 3 and game._selected_workers() == 3 and game.selected_entity.is_empty(), "Input selects own workers")
	label = "gather_for_return"
	await mouse(MOUSE_BUTTON_RIGHT, Vector3(deposit.x / 256.0, 0.5, deposit.z / 256.0))
	check(await until(func(): return carried() > 0, 250), "Gather fills worker cargo")
	await capture("harvesting")
	label = "explicit_return"
	await mouse(MOUSE_BUTTON_RIGHT, Vector3(anchor.x / 256.0, 0.5, anchor.z / 256.0))
	check(await until(func(): return int(game.current.salvage[0]) > 0 and carried() == 0, 400), "Explicit return deposits carried salvage")
	label = "repeat_gather"
	await mouse(MOUSE_BUTTON_RIGHT, Vector3(deposit.x / 256.0, 0.5, deposit.z / 256.0))
	check(await until(func(): return int(game.current.salvage[0]) >= 100, 1000), "Repeated gathering funds foundry")
	label = "stop_workers"
	await key(KEY_S)
	await advance_ticks(3)
	var stopped: Array = game.current.units.duplicate(true)
	await advance_ticks(8)
	var stable := true
	for index in stopped.size():
		if stopped[index].player == 0:
			var unit: Dictionary = game.current.units[index]
			stable = stable and unit.x == stopped[index].x and unit.z == stopped[index].z and unit.order == 0
	check(stable, "Stop cancels gathering and holds worker positions")
	var balance: int = game.current.salvage[0]
	label = "invalid_anchor_overlap"
	await key(KEY_B)
	await mouse(MOUSE_BUTTON_LEFT, Vector3(anchor.x / 256.0, 0, anchor.z / 256.0))
	await advance_ticks(3)
	check(game.current.salvage[0] == balance and own_foundries().is_empty(), "Invalid placement debits nothing and creates no structure")
	label = "cancel_build"
	await key(KEY_B)
	await key(KEY_ESCAPE)
	check(not game.build_pending, "Escape cancels placement")
	var site := Vector3(9.5, 0, 12.5)
	check(game.bridge.can_build(int(site.x * 256), int(site.z * 256)), "Fixture foundry site is valid")
	label = "valid_foundry"
	await key(KEY_B)
	await mouse(MOUSE_BUTTON_LEFT, site)
	check(await until(func(): return own_foundries().size() == 1, 1050), "Build input creates one foundry")
	check(game.current.salvage[0] == balance - 100, "Foundry debits exactly its displayed cost")
	check(await until(func(): return own_foundries().size() == 1 and own_foundries()[0].build_ticks > 0, 1150), "Worker advances construction")
	await mouse(MOUSE_BUTTON_LEFT, site + Vector3(0, 0.5, 0))
	check(game.selected_entity.get("kind", -1) == 1, "Foundry selection exposes construction readout")
	await capture("construction")
	var builder: Dictionary = game.current.units[0]
	await mouse(MOUSE_BUTTON_LEFT, Vector3(builder.x / 256.0, 0.5, builder.z / 256.0))
	label = "pause_construction"
	await key(KEY_S)
	await advance_ticks(2)
	var paused_progress: int = own_foundries()[0].build_ticks if not own_foundries().is_empty() else -1
	await advance_ticks(8)
	check(not own_foundries().is_empty() and own_foundries()[0].build_ticks == paused_progress, "Stop pauses unfinished construction")
	label = "resume_construction"
	await mouse(MOUSE_BUTTON_RIGHT, site + Vector3(0, 0.5, 0))
	check(await until(func(): return own_foundries().size() == 1 and own_foundries()[0].build_ticks == 100, game.finish_tick), "Foundry construction completes")
	check(game.current.salvage[0] == balance - 100, "Resuming construction charges no second cost")
	await mouse(MOUSE_BUTTON_LEFT, site + Vector3(0, 0.5, 0))
	await capture("completed")
	game.completed = true
	var final_snapshot: Dictionary = game.current.duplicate(true)
	var replay_path: String = game.report_path.get_basename() + ".vfr"
	var replay: PackedByteArray = game.bridge.replay_bytes()
	var replay_file := FileAccess.open(replay_path, FileAccess.WRITE)
	check(replay_file != null and not replay.is_empty(), "Replay bytes available")
	if replay_file:
		replay_file.store_buffer(replay)
		replay_file.close()
	if not game.capture_path.is_empty():
		await RenderingServer.frame_post_draw
		check(game.get_viewport().get_texture().get_image().save_png(game.capture_path) == OK, "Final screenshot saved")
	await key(KEY_R)
	check(game.current.tick == 0 and game.current.hash == initial.hash, "Input restart restores identical economy state")
	check(game.selected.is_empty() and game.selected_entity.is_empty() and not game.build_pending and not game.order_mark.visible, "Restart clears selection placement and order feedback")
	var trace: Array = []
	for snapshot in samples: trace.append({"tick": snapshot.tick, "hash": snapshot.hash})
	var report := {"ok": errors.is_empty(), "mode": "economy", "setup": {"map_id": initial.map_id, "width": initial.width, "height": initial.height, "protocol": initial.get("protocol", 6), "content_id": initial.get("content_id", "")},
		"initial_snapshot": initial, "final_snapshot": final_snapshot, "snapshots": samples, "trace": trace, "inputs": inputs,
		"checks": checks, "errors": errors, "captures": captures, "replay_path": replay_path,
		"note": "Packaged software InputEvents: harvesting, explicit return, repeat gather, stop, invalid/valid construction and restart. Blockout economy; no production, complete match, human usability or performance acceptance."}
	var output := FileAccess.open(game.report_path, FileAccess.WRITE)
	if output: output.store_string(JSON.stringify(report, "\t"))
	else: errors.append("Cannot save report")
	print("VOIDFRONT_ECONOMY ok=", errors.is_empty(), " tick=", final_snapshot.tick, " errors=", errors)
	game.get_tree().quit(0 if errors.is_empty() else 1)

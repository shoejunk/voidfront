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
	print("VOIDFRONT_PRODUCTION_CHECK tick=", game.current.tick, " ok=", ok, " ", message)

func tick() -> void:
	if int(game.current.tick) > last_tick:
		samples.append(game.current.duplicate(true))
		last_tick = int(game.current.tick)
	if game.current.tick >= game.finish_tick: game.completed = true

func record_input(accepted: bool, order: int, at: Vector3, command_actors: Array[int]) -> void:
	inputs.append({"label": label, "accepted": accepted, "order": order, "units": command_actors.duplicate(),
		"x": int(at.x * 256), "z": int(at.z * 256), "event_tick": game.current.tick})

func frames() -> void:
	await game.get_tree().process_frame
	await game.get_tree().process_frame

func key(code: Key) -> void:
	game._smoke_key(code)
	await frames()

func mouse(button: MouseButton, point: Vector3) -> void:
	var screen: Vector2 = game.camera.unproject_position(point)
	var view: Vector2 = game.get_viewport().get_visible_rect().size
	if not Rect2(Vector2(100, 150), view - Vector2(200, 340)).has_point(screen):
		var rect: Rect2 = game.hud.minimap_rect()
		var nav_point := rect.position + Vector2(point.x, point.z) / Vector2(game.map_size) * rect.size
		game._smoke_mouse(MOUSE_BUTTON_LEFT, nav_point, true)
		game._smoke_mouse(MOUSE_BUTTON_LEFT, nav_point, false)
		await frames()
		screen = game.camera.unproject_position(point)
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

func striders() -> Array:
	return game.current.units.filter(func(u): return u.player == 0 and u.kind == 0 and u.hp > 0)

func run() -> void:
	await frames()
	check(initial.map_id == 2 and initial.units.size() == 6, "Production starts from economy workers")
	var deposit: Dictionary = initial.deposits[0]
	await key(KEY_F1)
	label = "gather_for_production"
	await mouse(MOUSE_BUTTON_RIGHT, Vector3(deposit.x / 256.0, 0.5, deposit.z / 256.0))
	check(await until(func(): return int(game.current.salvage[0]) >= 200, game.finish_tick - 450), "Gathering funds Foundry and two Striders")
	label = "stop_workers"
	await key(KEY_S)
	await advance_ticks(3)
	var balance: int = game.current.salvage[0]
	var site := Vector3(9.5, 0, 12.5)
	label = "build_foundry"
	await key(KEY_B)
	await mouse(MOUSE_BUTTON_LEFT, site)
	check(await until(func(): return own_foundries().size() == 1 and own_foundries()[0].build_ticks == game.current.build_duration, game.finish_tick - 240), "Worker completes purchased Foundry")
	check(game.current.salvage[0] == balance - game.current.foundry_cost, "Foundry charges displayed salvage cost")
	await mouse(MOUSE_BUTTON_LEFT, site + Vector3(0, 0.5, 0))
	check(game.selected.is_empty() and game.selected_entity.get("kind", -1) == 1, "Foundry selects without pretending it is a worker")
	label = "train_first"
	await key(KEY_T)
	await advance_ticks(2)
	label = "train_second"
	await key(KEY_T)
	await advance_ticks(5)
	check(own_foundries().size() == 1 and own_foundries()[0].production_queue == 2, "Train hotkey queues two Striders")
	check(game.current.salvage[0] == balance - game.current.foundry_cost - 2 * game.current.strider_cost, "Training charges two displayed unit costs")
	check(game.current.population_reserved[0] == 2, "Queued units reserve population")
	await capture("queued")
	var before_cancel: Dictionary = game.current.duplicate(true)
	label = "cancel_tail"
	await key(KEY_X)
	await advance_ticks(2)
	check(own_foundries().size() == 1 and own_foundries()[0].production_queue == 1, "Cancel removes last queued unit")
	check(game.current.salvage[0] == before_cancel.salvage[0] + game.current.strider_cost, "Cancel refunds full unit cost")
	var previous_foundry: Dictionary = before_cancel.structures.filter(func(s): return s.player == 0 and s.kind == 1)[0] if own_foundries().size() == 1 else {}
	check(not previous_foundry.is_empty() and own_foundries()[0].production_ticks >= previous_foundry.production_ticks, "Cancel preserves active head progress")
	check(game.current.population_reserved[0] == 1, "Cancel releases reserved population")
	await capture("refunded")
	label = "train_replacement"
	await key(KEY_T)
	await advance_ticks(2)
	var before_failure: int = game.current.salvage[0]
	label = "unaffordable_train"
	await key(KEY_T)
	await advance_ticks(2)
	check(game.current.command_results[0] == 3 and game.current.salvage[0] == before_failure, "Unaffordable purchase rejects without debit")
	check(game.economy_notice.begins_with("Not enough salvage"), "Purchase rejection is visible in HUD")
	await capture("purchase-rejected")
	check(await until(func(): return striders().size() >= 2, game.finish_tick - 50), "Paid queue produces two live Striders")
	check(game.current.population_reserved[0] == 0 and game.current.population_used[0] == 5, "Spawning transfers reserved population to five live units")
	await capture("produced")
	if not striders().is_empty():
		var unit: Dictionary = striders()[0]
		await mouse(MOUSE_BUTTON_LEFT, Vector3(unit.x / 256.0, 0.5, unit.z / 256.0))
		check(game.selected == [unit.id] and game.selected_entity.is_empty(), "Produced unit can be selected")
		label = "move_produced"
		await mouse(MOUSE_BUTTON_RIGHT, Vector3(12.5, 0, 14.5))
		check(await until(func():
			for moved in striders():
				if moved.id == unit.id and Vector2(moved.x - unit.x, moved.z - unit.z).length() > 256: return true
			return false, game.finish_tick), "Produced Strider responds to movement input")
		await capture("moved")
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
	check(game.selected.is_empty() and game.selected_entity.is_empty() and not game.build_pending and not game.order_mark.visible, "Restart clears selections and feedback")
	var trace: Array = []
	for snapshot in samples: trace.append({"tick": snapshot.tick, "hash": snapshot.hash})
	var report := {"ok": errors.is_empty(), "mode": "production", "setup": {"map_id": initial.map_id, "width": initial.width, "height": initial.height, "protocol": initial.protocol, "content_id": initial.content_id},
		"initial_snapshot": initial, "final_snapshot": final_snapshot, "snapshots": samples, "trace": trace, "inputs": inputs,
		"checks": checks, "errors": errors, "captures": captures, "replay_path": replay_path,
		"note": "Packaged software InputEvents: gather, build, queue, cancel/refund, purchase rejection, two spawned units, selection/movement and restart. No full match, human usability, balance or performance acceptance."}
	var output := FileAccess.open(game.report_path, FileAccess.WRITE)
	if output: output.store_string(JSON.stringify(report, "\t"))
	else: errors.append("Cannot save report")
	print("VOIDFRONT_PRODUCTION ok=", errors.is_empty(), " tick=", final_snapshot.tick, " errors=", errors)
	game.get_tree().quit(0 if errors.is_empty() else 1)

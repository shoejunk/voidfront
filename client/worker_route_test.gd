extends Node

# Render the normal game and feed its usual mouse and keyboard handlers. Only
# the clock is driven by this fixture so a screenshot cannot skip a transition.
@onready var game = get_parent()
var trace: FileAccess
var events := {}
var folder: String
var capture_errors: Array[String] = []

func _ready() -> void:
	game.set_process(false)
	run.call_deferred()

func click(button: MouseButton, at: Vector2) -> void:
	game._smoke_mouse(button, at, true)
	game._smoke_mouse(button, at, false)
	await get_tree().process_frame
	await get_tree().process_frame

func pan(x: float, z: float) -> void:
	var rect: Rect2 = game.hud.minimap_rect()
	await click(MOUSE_BUTTON_LEFT, rect.position + Vector2(x, z) / Vector2(game.map_size) * rect.size)

func select_worker(id: int) -> bool:
	var screen: Vector2 = game.camera.unproject_position(game.actors[id].root.position + Vector3.UP * 0.5)
	await click(MOUSE_BUTTON_LEFT, screen)
	return game.selected == [id]

func advance_until(condition: Callable, allowance: int) -> bool:
	for i in allowance:
		var miner: Dictionary = game.current.units[1]
		if miner.order == game.Action.GATHER and not miner.returning and miner.x >= 7250 and miner.x <= 8050:
			# Keep the crossing at 20 ticks/s in a 60-fps capture. Let the
			# normal game interpolate and draw all three frames of each tick.
			for frame in 3:
				game._process(game.STEP / 3.0)
				await get_tree().process_frame
		else:
			game._process(game.STEP)
			if i % 8 == 0: await get_tree().process_frame
		var state: Dictionary = game.current
		trace.store_line("%d %s" % [state.tick, state.hash])
		if condition.call(): return true
	return false

func photograph(label: String) -> void:
	await RenderingServer.frame_post_draw
	var path := folder.path_join(label + ".png")
	var result := get_viewport().get_texture().get_image().save_png(path)
	if result != OK: capture_errors.append(label)
	var unit: Dictionary = game.current.units[1]
	events[label] = {"tick": game.current.tick, "position": [unit.x, unit.z], "cargo": unit.cargo,
		"bank": game.current.salvage[0], "reserve": game.current.deposits[1].remaining, "hp": unit.hp,
		"order": unit.order, "deposit": unit.resource_id, "returning": unit.returning,
		"selection": game.selected.duplicate(), "card": game.hud.command_card.heading.text,
		"image_sha256": FileAccess.get_sha256(path)}

func finish(problem := "") -> void:
	trace.close()
	var replay := FileAccess.open(folder.path_join("route.vfr"), FileAccess.WRITE)
	var report := FileAccess.open(folder.path_join("report.json"), FileAccess.WRITE)
	if not replay or not report:
		push_error("Could not write route evidence")
		get_tree().quit(2)
		return
	if not replay.store_buffer(game.bridge.replay_bytes()): problem = "Could not save replay"
	if not capture_errors.is_empty(): problem = "Could not save captures: " + ", ".join(capture_errors)
	report.store_string(JSON.stringify({"error": problem, "events": events, "tick": game.current.tick,
		"hash": game.current.hash, "content_id": game.current.content_id}, "  "))
	replay.close()
	report.close()
	print("WORKER_ROUTE tick=", game.current.tick, " ", problem)
	get_tree().quit(0 if problem.is_empty() else 1)

func run() -> void:
	# The caller supplies a fresh directory for this run's evidence.
	folder = game.report_path
	if folder.is_empty() or DirAccess.make_dir_absolute(folder) != OK:
		push_error("Supply a fresh absolute output directory with --report")
		get_tree().quit(2)
		return
	trace = FileAccess.open(folder.path_join("route.trace"), FileAccess.WRITE)
	if not trace:
		get_tree().quit(2)
		return
	await get_tree().process_frame
	game._smoke_key(KEY_O)
	await get_tree().process_frame
	game._smoke_key(KEY_ENTER)
	await get_tree().process_frame
	await get_tree().process_frame
	if game.setup_open or game.current.enemy_ai or game.current.units.size() != 6:
		finish("Passive six-worker setup did not start")
		return
	# Leave a friendly observer near the distant deposit so the UI can pick it.
	if not await select_worker(3):
		finish("Could not select the observer")
		return
	await pan(53.5, 40.5)
	var staging: Vector2 = game.camera.unproject_position(Vector3(53.5, 0, 40.5))
	await click(MOUSE_BUTTON_RIGHT, staging)
	if not await advance_until(func(): return game.current.units[2].order == 0 and game._entity_visible(game.current.deposits[1]), 800):
		finish("Observer failed to reveal the distant field")
		return
	await pan(4.5, 12.5)
	if not await select_worker(2):
		finish("Could not select the middle worker")
		return
	var recall: Button = game.hud.command_card.buttons[game.Action.RETURN_CARGO]
	await click(MOUSE_BUTTON_LEFT, recall.get_global_rect().get_center())
	if not await advance_until(func(): return game.current.units[1].order == 0 and game.current.units[1].x == 1152 and game.current.units[1].z == 2848, 200):
		finish("Worker did not return to its anchor")
		return
	await photograph("home")
	await pan(56.5, 40.5)
	var patch: Dictionary = game.current.deposits[1]
	var field: Vector2 = game.camera.unproject_position(Vector3(patch.x / game.SCALE, 0.5, patch.z / game.SCALE))
	await click(MOUSE_BUTTON_RIGHT, field)
	await pan(30, 24)
	if not await advance_until(func(): return game.current.units[1].order == game.Action.GATHER and game.current.units[1].resource_id == patch.id and game.current.units[1].x >= 7700, 500):
		finish("Gather did not reach the central flux field")
		return
	await photograph("at-flux")
	if not await advance_until(func(): return game.current.units[1].x > 8000, 50):
		await photograph("stalled")
		finish("Worker did not pass the flux obstacle after one Gather")
		return
	await photograph("past-flux")
	await pan(56.5, 40.5)
	if not await advance_until(func(): return game.current.units[1].cargo == 1, 500):
		finish("Worker did not extract distant salvage")
		return
	await photograph("mining")
	if not await advance_until(func(): return game.current.units[1].cargo == 10 and game.current.units[1].returning, 150):
		finish("Worker failed to collect a full load for the anchor")
		return
	await photograph("returning")
	await pan(4.5, 12.5)
	if not await advance_until(func(): return game.current.salvage[0] == 10 and game.current.units[1].cargo == 0, 700):
		finish("Worker did not deposit its load")
		return
	await photograph("paid")
	if game.current.deposits[1].remaining != patch.remaining - 10 or events.paid.position != events.home.position:
		finish("Payment did not match extraction and arrival at the anchor")
		return
	for label in ["at-flux", "past-flux", "mining"]:
		if not "→  Salvage (56, 40)" in events[label].card or events[label].selection != [2]:
			finish("Selected worker card lost the assigned field during " + label)
			return
	if not "1 salvage" in events.mining.card or not "Returning 10 salvage to anchor" in events.returning.card:
		finish("Worker card lost the current load during mining or return")
		return
	for unit in game.current.units:
		if unit.hp != unit.max_hp:
			finish("Worker took damage during the passive route")
			return
	finish()

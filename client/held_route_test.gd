extends "res://worker_route_test.gd"

# Replay the two-holder case through selection, the Move button and Hold.
# Each target is picked at the center of a fixed-point coordinate so screen
# projection rounds to the same command as the native regression.
const HOME := Vector2i(1152, 2848)
const FIRST_POST := Vector2i(7488, 5888)
const SECOND_POST := Vector2i(7616, 5760)
const DESTINATION := Vector2i(14464, 10144)

func worker_at(id: int, point: Vector2i) -> bool:
	var unit: Dictionary = game.current.units[id - 1]
	return Vector2i(unit.x, unit.z) == point

func photograph(label: String) -> void:
	# Input can change selection or pan the camera while the fixture clock is
	# paused. Draw the selected actor at this tick before saving its frame.
	game._present(1.0, 0.0)
	game.hud._process(0.0)
	game._present_economy()
	await super.photograph(label)
	var view := get_viewport()
	var centers := []
	var transform := view.get_final_transform()
	for id in [1, 2, 3]:
		if not game.hud.drawn_health_bars.has(id):
			capture_errors.append("%s: no rendered bar for worker %d" % [label, id])
			return
		var bar: Rect2 = game.hud.drawn_health_bars[id]
		var pixel := transform * (bar.position + Vector2(bar.size.x / 2, 1.5))
		centers.append([pixel.x, pixel.y])
	events[label].friendly_bars = centers
	events[label].camera_target = [game.camera_target.x, game.camera_target.z]

func move_worker(id: int, point: Vector2i) -> bool:
	var actor: Vector3 = game.actors[id].root.position
	await pan(actor.x, actor.z)
	if not await select_worker(id): return false
	var at := (Vector2(point) + Vector2(0.5, 0.5)) / float(game.SCALE)
	await pan(at.x, at.y)
	var button: Button = game.hud.command_card.buttons[game.Action.MOVE]
	await click(MOUSE_BUTTON_LEFT, button.get_global_rect().get_center())
	if game.target_order != game.Action.MOVE: return false
	var screen: Vector2 = game.camera.unproject_position(Vector3(at.x, 0, at.y))
	await click(MOUSE_BUTTON_LEFT, screen)
	return game.target_order == -1

func place_holder(id: int, point: Vector2i) -> bool:
	if not await move_worker(id, point): return false
	if not await advance_until(func(): return worker_at(id, point), 400): return false
	game._smoke_key(KEY_H)
	await get_tree().process_frame
	await get_tree().process_frame
	return true

func run() -> void:
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
	if not await place_holder(2, HOME) or not await place_holder(1, FIRST_POST) or not await place_holder(3, SECOND_POST):
		finish("Could not move and hold each worker at its recorded post")
		return
	if not await move_worker(2, DESTINATION):
		finish("Could not issue the final Move")
		return
	await pan(30, 24)
	await photograph("launch")
	for i in 1000:
		if game.current.tick >= 640 and game.current.tick < 714:
			for frame in 3:
				game._process(game.STEP / 3.0)
				await get_tree().process_frame
		else:
			game._process(game.STEP)
			if i % 8 == 0: await get_tree().process_frame
		trace.store_line("%d %s" % [game.current.tick, game.current.hash])
		if not worker_at(1, FIRST_POST) or not worker_at(3, SECOND_POST) or game.current.units[0].order != game.Action.HOLD or game.current.units[2].order != game.Action.HOLD:
			finish("A held worker moved or lost its Hold order")
			return
		if game.current.tick == 662: await photograph("contact")
		elif game.current.tick == 710: await photograph("passage")
		if worker_at(2, DESTINATION) and game.current.units[1].order == game.Action.STOP:
			await pan(56.5, 39.5)
			await photograph("arrival")
			finish()
			return
	await photograph("stalled")
	finish("The middle worker did not finish its single Move")

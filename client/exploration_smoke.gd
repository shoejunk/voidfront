extends "res://production_smoke.gd"
# Actual input, active AI, no injected resources/units. Keep compact hash traces.
var trace: Array[Dictionary] = []
var vision_samples: Array[Dictionary] = []

func tick() -> void:
	if int(game.current.tick) > last_tick:
		trace.append({"tick": game.current.tick, "hash": game.current.hash})
		last_tick = int(game.current.tick)
	if game.current.tick >= game.finish_tick: game.completed = true

func minimap(point: Vector2) -> void:
	var rect: Rect2 = game.hud.minimap_rect()
	var screen := rect.position + point / Vector2(game.map_size) * rect.size
	game._smoke_mouse(MOUSE_BUTTON_LEFT, screen, true)
	game._smoke_mouse(MOUSE_BUTTON_LEFT, screen, false)
	await frames()

func observe(name: String) -> void:
	vision_samples.append({"phase": name, "tick": game.current.tick, "camera": [game.camera_target.x, game.camera_target.z], "scout_cell": game._vision_at(22.5, 32.5), "enemy_visible": game._entity_visible(game.current.structures[1])})
	await capture(name)

func run() -> void:
	await frames()
	check(initial.width == 64 and initial.height == 48 and initial.enemy_ai, "Larger 64x48 default economy with active AI")
	check(game.camera_target.x < 16 and game.camera.size == 27, "Camera starts over own economy at playable zoom")
	check(not game._entity_visible(initial.structures[1]) and not game.economy_actors["structures2"].root.visible, "Unscouted enemy anchor hidden")
	check(game.current.units.filter(func(u): return u.player == 1).all(func(u): return not game.actors[u.id].root.visible), "Unscouted enemy workers hidden")
	await observe("home")
	await minimap(Vector2(59.5, 36.5))
	check(game.camera_target.distance_to(Vector3(59.5, 0, 36.5)) < 0.1, "Minimap navigates across larger map")
	check(game._vision_at(59.5, 36.5) == 0, "Camera navigation does not reveal unexplored ground")
	var enemy_screen: Vector2 = game.camera.unproject_position(Vector3(59.5, 0.5, 36.5))
	check(game._economy_entity_at(enemy_screen).is_empty(), "Hidden enemy cannot be picked through fog")
	await observe("unexplored")
	await key(KEY_HOME)
	check(game.camera_target.x < 16, "Home returns camera to own base")
	# Exercise middle drag with real mouse-motion input.
	var before: Vector3 = game.camera_target
	game._smoke_mouse(MOUSE_BUTTON_MIDDLE, Vector2(700, 450), true)
	var motion := InputEventMouseMotion.new()
	motion.position = Vector2(600, 450)
	motion.relative = Vector2(-100, 0)
	Input.parse_input_event(motion)
	await frames()
	game._smoke_mouse(MOUSE_BUTTON_MIDDLE, Vector2(600, 450), false)
	check(game.camera_target.x > before.x + 1, "Middle mouse dragging pans the camera")
	await key(KEY_HOME)
	var arrow := InputEventKey.new()
	arrow.physical_keycode = KEY_RIGHT
	arrow.pressed = true
	Input.parse_input_event(arrow)
	for frame in range(12): await game.get_tree().process_frame
	arrow = InputEventKey.new()
	arrow.physical_keycode = KEY_RIGHT
	arrow.pressed = false
	Input.parse_input_event(arrow)
	check(game.camera_target.x > 10.1, "Arrow key pans camera")
	var zoom_before: float = game.camera.size
	game._smoke_mouse(MOUSE_BUTTON_WHEEL_UP, Vector2(600, 450), true)
	await frames()
	check(game.camera.size < zoom_before, "Mouse wheel zooms camera")
	game._smoke_mouse(MOUSE_BUTTON_WHEEL_DOWN, Vector2(600, 450), true)
	await key(KEY_HOME)
	await key(KEY_F1)
	label = "gather"
	await mouse(MOUSE_BUTTON_RIGHT, Vector3(7.5, 0.5, 7.5))
	check(await until(func(): return game.current.salvage[0] >= 150, 2400), "Workers harvest enough salvage for Foundry and Strider")
	await mouse(MOUSE_BUTTON_LEFT, game.actors[1].root.position + Vector3(0, 0.5, 0))
	await key(KEY_B)
	label = "build"
	await mouse(MOUSE_BUTTON_LEFT, Vector3(9.5, 0, 15.5))
	check(await until(func(): return not own_foundries().is_empty() and own_foundries()[0].build_ticks == game.current.build_duration, 3000), "Existing paid construction remains functional")
	await mouse(MOUSE_BUTTON_LEFT, Vector3(9.5, 0.5, 15.5))
	label = "train"
	await key(KEY_T)
	check(await until(func(): return not striders().is_empty(), 3400), "Foundry produces a paid Strider")
	if striders().is_empty():
		await finish_exploration()
		return
	await key(KEY_F2)
	check(game.selected.size() == 1 and game._selected_workers() == 0, "Army selection leaves miners working")
	check(game._vision_at(22.5, 32.5) == 0, "Scout destination begins unexplored")
	await minimap(Vector2(22.5, 32.5))
	label = "scout"
	await mouse(MOUSE_BUTTON_RIGHT, Vector3(22.5, 0, 32.5))
	check(await until(func(): return striders().any(func(u): return Vector2(u.x / 256.0 - 22.5, u.z / 256.0 - 32.5).length() < 0.3), 4000), "Produced Strider reaches exploration destination")
	check(game._vision_at(22.5, 32.5) == 2, "Scout reveals terrain in authoritative vision")
	await observe("scouted")
	await key(KEY_HOME)
	label = "retreat"
	await mouse(MOUSE_BUTTON_RIGHT, Vector3(10.5, 0, 12.5))
	check(await until(func(): return striders().any(func(u): return u.hp > 0 and Vector2(u.x / 256.0 - 10.5, u.z / 256.0 - 12.5).length() < 0.3), 4500), "Scout survives and returns home")
	check(game._vision_at(22.5, 32.5) == 1, "Departed scout leaves explored terrain under fog")
	await minimap(Vector2(22.5, 32.5))
	await observe("explored")
	check(game.current.structures.any(func(b): return b.player == 1 and b.kind == 1 and b.build_ticks == game.current.build_duration), "AI still harvests and completes production building")
	check(game.current.units.any(func(u): return u.player == 1 and u.kind == 0), "AI still produces combat units on larger map")
	await key(KEY_HOME)
	check(await until(func(): return game.current.units.any(func(u): return u.player == 1 and u.hp > 0 and game._entity_visible(u)), 6000), "AI arrives inside player vision")
	await frames()
	check(game.current.units.filter(func(u): return u.player == 1 and u.hp > 0 and game._entity_visible(u)).all(func(u): return game.actors[u.id].root.visible), "Scouted enemy units appear in scene")
	await observe("enemy-contact")
	check(await until(func(): return game.current.winner != -1, 9500), "Active AI match reaches anchor outcome")
	check(game.current.winner == 1 and game.current.structures[0].hp == 0, "Unreinforced player can still lose to economic AI")
	await frames()
	check(game.hud.result.text.begins_with("DEFEAT"), "Defeat and restart remain visible")
	await observe("defeat")
	await finish_exploration()

func finish_exploration() -> void:
	game.completed = true
	var final_tick: int = game.current.tick
	var replay_path: String = game.report_path.get_basename() + ".vfr"
	var replay: PackedByteArray = game.bridge.replay_bytes()
	var replay_file := FileAccess.open(replay_path, FileAccess.WRITE)
	check(replay_file != null and not replay.is_empty(), "Canonical replay retained")
	if replay_file:
		replay_file.store_buffer(replay)
		replay_file.close()
	await key(KEY_R)
	check(game.current.hash == initial.hash and game._vision_at(22.5, 32.5) == 0, "Restart resets economy and explored fog identically")
	check(game.camera_target.x < 16 and game.selected.is_empty(), "Restart restores base camera and clears selection")
	var report := {"ok": errors.is_empty(), "mode": "exploration", "ticks": final_tick, "checks": checks, "errors": errors, "trace": trace, "inputs": inputs, "vision_samples": vision_samples, "captures": captures, "replay_path": replay_path}
	var output := FileAccess.open(game.report_path, FileAccess.WRITE)
	if output: output.store_string(JSON.stringify(report, "\t"))
	print("VOIDFRONT_EXPLORATION ok=", errors.is_empty(), " errors=", errors)
	game.get_tree().quit(0 if errors.is_empty() else 1)

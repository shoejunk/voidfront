extends "res://production_smoke.gd"

var trace: Array[Dictionary] = []
var footprints: Array[Dictionary] = []
var edges: Array[Dictionary] = []
var corner_orders: Array[Dictionary] = []
var mining := {}

func tick() -> void:
	if int(game.current.tick) > last_tick:
		trace.append({"tick": game.current.tick, "hash": game.current.hash})
		last_tick = int(game.current.tick)
	if game.current.tick >= game.finish_tick: game.completed = true

func pair(point: Vector2) -> Array:
	return [point.x, point.y]

func click(button: MouseButton, screen: Vector2) -> void:
	game._smoke_mouse(button, screen, true)
	game._smoke_mouse(button, screen, false)
	await frames()

func navigate(point: Vector2) -> void:
	var rect: Rect2 = game.hud.minimap_rect()
	await click(MOUSE_BUTTON_LEFT, rect.position + point / Vector2(game.map_size) * rect.size)

func wheel(button: MouseButton, count: int) -> void:
	var screen: Vector2 = game.get_viewport().get_visible_rect().get_center()
	for step in count:
		game._smoke_mouse(button, screen, true)
		game._smoke_mouse(button, screen, false)
	await frames()

func footprint(name: String) -> void:
	# Intersect the renderer's four rays with the arena floor independently
	# of the game's camera clamp and minimap footprint drawing code.
	var rect: Rect2 = game.get_viewport().get_visible_rect()
	var corners: Array = []
	var inside := true
	for screen in [rect.position, Vector2(rect.end.x, rect.position.y), rect.end, Vector2(rect.position.x, rect.end.y)]:
		var origin: Vector3 = game.camera.project_ray_origin(screen)
		var direction: Vector3 = game.camera.project_ray_normal(screen)
		var hit = Plane(Vector3.UP, 0.0).intersects_ray(origin, direction)
		if hit == null:
			inside = false
			continue
		corners.append([hit.x, hit.z])
		inside = inside and hit.x >= -0.002 and hit.x <= game.map_size.x + 0.002 and hit.z >= -0.002 and hit.z <= game.map_size.y + 0.002
	footprints.append({"label": name, "tick": game.current.tick, "size": game.camera.size,
		"target": [game.camera_target.x, game.camera_target.z], "corners": corners})
	check(inside, name + " keeps the four ground corners inside the map")

func corners_at_zoom(name: String) -> void:
	await key(KEY_HOME)
	footprint(name + "_home")
	for item in [["nw", Vector2(0.1, 0.1)], ["ne", Vector2(63.9, 0.1)], ["se", Vector2(63.9, 47.9)], ["sw", Vector2(0.1, 47.9)]]:
		await navigate(item[1])
		footprint(name + "_" + str(item[0]))
	await navigate(Vector2(59.5, 35.5))
	footprint(name + "_opposing_spawn")
	await capture(name + "-opposing-spawn")

func screen_at(point: Vector2) -> Vector2:
	return game.camera.unproject_position(Vector3(point.x, 0, point.y))

func worker() -> Dictionary:
	return game.current.units[0]

func worker_position() -> Array:
	var unit := worker()
	return [unit.x, unit.z]

func reached(point: Vector2) -> bool:
	var unit := worker()
	# One fixed-point unit of rounding is possible at world-to-screen round trip.
	return unit.order == 0 and absi(unit.x - roundi(point.x * 256)) <= 1 and absi(unit.z - roundi(point.y * 256)) <= 1

func move_to(name: String, point: Vector2) -> bool:
	label = name
	var count := inputs.size()
	var screen := screen_at(point)
	check(game.get_viewport().get_visible_rect().has_point(screen), name + " world click is on screen")
	await click(MOUSE_BUTTON_RIGHT, screen)
	var sent := inputs.size() == count + 1
	if sent:
		var input := inputs.back() as Dictionary
		sent = input.accepted and input.order == 1 and input.units == [worker().id] and absi(input.x - roundi(point.x * 256)) <= 1 and absi(input.z - roundi(point.y * 256)) <= 1
	check(sent, name + " right click sends one Move to the requested tile")
	if not sent: return false
	var arrived := await until(func(): return reached(point), int(game.current.tick) + 1200)
	check(arrived, name + " worker arrives in the simulation")
	return arrived

func hold_space(pressed: bool) -> void:
	var event := InputEventKey.new()
	event.physical_keycode = KEY_SPACE
	event.pressed = pressed
	Input.parse_input_event(event)
	await frames()

func exercise_edge(edge: String, pan: Vector2, stage: Vector2, goal: Vector2, occluder := "") -> bool:
	await navigate(pan)
	footprint(edge + "_orders")
	var corner := {}
	if not occluder.is_empty():
		var stage_screen := screen_at(stage)
		var panel: Rect2 = game.hud.minimap_rect() if occluder == "minimap" else game.hud.command_card.get_global_rect()
		check(game.hud.visible and panel.has_point(stage_screen), edge + " target falls under the visible " + occluder)
		var count := inputs.size()
		label = edge + "_covered"
		await click(MOUSE_BUTTON_RIGHT, stage_screen)
		var blocked_count := inputs.size() - count
		check(blocked_count == 0, edge + " covered RMB sends no world command")
		var selected: Array = game.selected.duplicate()
		await hold_space(true)
		check(not game.hud.visible and selected == [worker().id] and game.selected == selected, edge + " holding Space clears the HUD and preserves the worker selection")
		corner = {"occluder": occluder, "blocked_input_count": blocked_count, "clear_selected": game.selected.duplicate()}
	if not await move_to(edge + "_stage", stage): return false
	var arrival_tick: int = game.current.tick
	var start := worker_position()
	# With no army on this passive map F2 clears the old selection. A fresh
	# world click must therefore acquire the worker to issue the second leg.
	await key(KEY_F2)
	var before: Array = game.selected.duplicate()
	var actor: Node3D = game.actors[worker().id].root
	var screen: Vector2 = game.camera.unproject_position(actor.position + Vector3(0, 0.5, 0))
	await click(MOUSE_BUTTON_LEFT, screen)
	var after: Array = game.selected.duplicate()
	check(before.is_empty() and after == [worker().id], edge + " click reacquires the worker after clearing selection")
	if not before.is_empty() or after != [worker().id]: return false
	await capture(edge + ("-selected" if occluder.is_empty() else "-clear"))
	var order_screen := screen_at(goal)
	if not await move_to(edge + "_move", goal): return false
	var row := {"edge": edge, "actor": worker().id, "stage": pair(stage), "goal": pair(goal),
		"arrival_tick": arrival_tick, "from": start, "selected_before": before, "selected_after": after,
		"selection_screen": pair(screen), "order_screen": pair(order_screen), "final_tick": game.current.tick, "to": worker_position()}
	if occluder.is_empty():
		edges.append(row)
	else:
		await hold_space(false)
		check(game.hud.visible and game.selected == after, edge + " releasing Space restores the HUD with the same selection")
		row.merge(corner)
		row.hud_restored = game.hud.visible
		corner_orders.append(row)
	return true

func gather_at_opposing_deposit() -> void:
	await navigate(Vector2(56.5, 40.5))
	if not await move_to("deposit_approach", Vector2(54.5, 42.5)): return
	await wheel(MOUSE_BUTTON_WHEEL_DOWN, 9)
	var deposit: Dictionary = game.current.deposits[1]
	check(game._entity_visible(deposit), "The worker has actual vision of opposing salvage")
	label = "deposit_gather"
	var screen: Vector2 = game.camera.unproject_position(Vector3(deposit.x / 256.0, 0.5, deposit.z / 256.0))
	await click(MOUSE_BUTTON_RIGHT, screen)
	var mined := await until(func(): return worker().cargo > 0 and game.current.deposits[1].remaining < deposit.remaining, int(game.current.tick) + 350)
	check(mined, "Worker mines opposing salvage through the ordinary Gather command")
	if mined:
		# Preserve the exact reserve and cargo readout in the final PNG and VFR.
		game.completed = true
		mining = {"tick": game.current.tick, "worker": worker().duplicate(true), "deposit": game.current.deposits[1].duplicate(true)}
		footprint("opposing_miner")
		await capture("opposing-miner")

func run() -> void:
	await frames()
	check(initial.width == 64 and initial.height == 48 and not initial.enemy_ai, "Passive Glass Reach economy starts from the ordinary six workers")
	footprint("opening")
	await key(KEY_HOME)
	footprint("default_home")
	await capture("home")
	await wheel(MOUSE_BUTTON_WHEEL_UP, 32)
	footprint("near_after_default_home")
	check(game.get_viewport().get_visible_rect().has_point(screen_at(Vector2(4.5, 12.5))), "Zooming in after Home keeps the command anchor in view")
	await wheel(MOUSE_BUTTON_WHEEL_DOWN, 32)
	var far_size: float = game.camera.size
	await wheel(MOUSE_BUTTON_WHEEL_DOWN, 1)
	check(is_equal_approx(game.camera.size, far_size), "Far zoom stops at its limit")
	await corners_at_zoom("far")
	await wheel(MOUSE_BUTTON_WHEEL_UP, 32)
	footprint("near_after_far_opposing_spawn")
	check(game.get_viewport().get_visible_rect().has_point(screen_at(Vector2(59.5, 35.5))), "Zooming in after the opposing minimap click keeps that spawn in view")
	var near_size: float = game.camera.size
	await wheel(MOUSE_BUTTON_WHEEL_UP, 1)
	check(is_equal_approx(game.camera.size, near_size) and near_size < far_size, "Near zoom stops at a distinct limit")
	await corners_at_zoom("near")
	await key(KEY_HOME)
	var unit := worker()
	var first: Vector2 = game.camera.unproject_position(Vector3(unit.x / 256.0, 0.5, unit.z / 256.0))
	await click(MOUSE_BUTTON_LEFT, first)
	check(game.selected == [unit.id], "Initial world click selects the staging worker")
	# North uses the open area right of the status strip. South uses the gap
	# between the command card and minimap. The clicks stay on visible terrain.
	var route := [
		["west", Vector2(0.1, 24), Vector2(2.25, 23.25), Vector2(2.25, 26.25)],
		["north", Vector2(34, 0.1), Vector2(39.25, 2.25), Vector2(42.25, 2.25)],
		["east", Vector2(63.9, 24), Vector2(61.75, 23.25), Vector2(61.75, 26.25)],
		["south", Vector2(39, 47.9), Vector2(38.25, 45.75), Vector2(41.25, 45.75)]]
	for leg in route:
		if not await exercise_edge(leg[0], leg[1], leg[2], leg[3]): break
	if edges.size() == route.size():
		if await exercise_edge("southwest", Vector2(0.1, 47.9), Vector2(2.25, 45.75), Vector2(3.25, 45.75), "command_card"):
			if await exercise_edge("southeast", Vector2(63.9, 47.9), Vector2(61.75, 45.75), Vector2(61.75, 44.75), "minimap"):
				await gather_at_opposing_deposit()
	finish()

func finish() -> void:
	game.completed = true
	var replay_path: String = game.report_path.get_basename() + ".vfr"
	var replay_file := FileAccess.open(replay_path, FileAccess.WRITE)
	var replay: PackedByteArray = game.bridge.replay_bytes()
	check(replay_file != null and not replay.is_empty(), "Canonical replay available")
	if replay_file:
		check(replay_file.store_buffer(replay), "Canonical replay saved")
		replay_file.close()
	var report := {"ok": errors.is_empty(), "mode": "camera_edge", "map": pair(Vector2(game.map_size)),
		"viewport": pair(game.get_viewport().get_visible_rect().size), "window": pair(Vector2(game.get_window().size)),
		"setup": {"protocol": initial.protocol, "content_id": initial.content_id, "seed": game.setup_seed,
			"enemy_ai": initial.enemy_ai, "start_deposit_remaining": initial.deposits[1].remaining},
		"ticks": game.current.tick, "initial_hash": initial.hash, "final_hash": game.current.hash, "trace": trace,
		"checks": checks, "errors": errors, "inputs": inputs, "footprints": footprints, "edges": edges, "corners": corner_orders,
		"mining": mining, "captures": captures, "replay_path": replay_path}
	var output := FileAccess.open(game.report_path, FileAccess.WRITE)
	if output:
		output.store_string(JSON.stringify(report, "\t"))
		output.close()
	else: errors.append("Cannot write camera report")
	print("VOIDFRONT_CAMERA_EDGE ok=", errors.is_empty(), " tick=", game.current.tick, " errors=", errors)
	game.get_tree().quit(0 if errors.is_empty() else 1)

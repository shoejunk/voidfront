extends RefCounted
# Ordinary InputEvents in two real clients. Player 0 attacks; player 1 techs and
# holds a bounded army. All authoritative observations come from bridge snapshots.
var game
var checks: Array[String] = []
var errors: Array[String] = []
var inputs: Array[Dictionary] = []
var phases: Array[Dictionary] = []
var captures: Array[Dictionary] = []
var initial_snapshot: Dictionary
var final_snapshot: Dictionary = {}
var label := ""
var player := 0
var done := false
var stage := "initial"
var next_progress_tick := 500

func _init(owner) -> void:
	game = owner
	player = game.local_player
	initial_snapshot = game.current.duplicate(true)
	phase("initial")

func check(ok: bool, message: String) -> bool:
	if ok: checks.append(message)
	else: errors.append(message)
	print("VOIDFRONT_NETECON_CHECK player=", player, " tick=", game.current.tick, " ok=", ok, " ", message)
	return ok

func record_input(accepted: bool, order: int, point: Vector3, actors: Array) -> void:
	inputs.append({"label": label, "accepted": accepted, "order": order, "units": actors.duplicate(),
		"x": int(point.x * 256), "z": int(point.z * 256), "event_tick": game.current.tick,
		"before_snapshot": game.current.duplicate(true)})
	if not accepted: check(false, "Rejected input: " + label)

func phase(name: String) -> void:
	stage = name
	phases.append({"label": name, "tick": game.current.tick, "snapshot": game.current.duplicate(true),
		"selection": game.selected.duplicate(), "selected_entity": game.selected_entity.duplicate(true),
		"paths": game._order_paths().duplicate(true), "hud": hud_state()})

func hud_state() -> Dictionary:
	return {"selection": game.hud.selection.text, "status": game.hud.status.text,
		"result": game.hud.result.text, "connection": game.hud.connection.text,
		"tip": game.hud.tip.text, "order_mark_visible": game.order_mark.visible,
		"order_mark": {"x": game.order_mark.position.x, "z": game.order_mark.position.z}}

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

func navigate(point: Vector3) -> void:
	var rect: Rect2 = game.hud.minimap_rect()
	var nav_point := rect.position + Vector2(point.x, point.z) / Vector2(game.map_size) * rect.size
	game._smoke_mouse(MOUSE_BUTTON_LEFT, nav_point, true)
	game._smoke_mouse(MOUSE_BUTTON_LEFT, nav_point, false)
	await frames()

func mouse(button: MouseButton, point: Vector3, shift := false) -> void:
	var screen: Vector2 = game.camera.unproject_position(point)
	var view: Vector2 = game.get_viewport().get_visible_rect().size
	if not Rect2(Vector2(100, 150), view - Vector2(200, 340)).has_point(screen):
		await navigate(point)
		screen = game.camera.unproject_position(point)
	for pressed in [true, false]:
		var event := InputEventMouseButton.new()
		event.button_index = button
		event.position = game.get_viewport().get_final_transform() * screen
		event.pressed = pressed
		event.shift_pressed = shift
		Input.parse_input_event(event)
	await frames()

func until(predicate: Callable, limit: int) -> bool:
	while game.current.tick < mini(limit, game.finish_tick) and not predicate.call() and not game.completed:
		if str(game.network_state.get("state", "")) in ["complete", "error"]: break
		if game.current.tick >= next_progress_tick:
			print("VOIDFRONT_NETECON_PROGRESS player=", player, " tick=", game.current.tick,
				" phase=", stage, " input=", label, " waiting_until=", mini(limit, game.finish_tick),
				" salvage=", game.current.salvage[player], " flux=", game.current.flux[player],
				" hash=", game.current.hash)
			next_progress_tick += 500
		await game.get_tree().process_frame
	return bool(predicate.call())

func capture(name: String) -> void:
	if game.capture_path.is_empty(): return
	await RenderingServer.frame_post_draw
	var path: String = game.capture_path.get_basename() + "-" + name + ".png"
	var result: int = game.get_viewport().get_texture().get_image().save_png(path)
	captures.append({"label": name, "tick": game.current.tick, "path": path, "error": result,
		"snapshot": game.current.duplicate(true), "selection": game.selected.duplicate(),
		"paths": game._order_paths().duplicate(true), "hud": hud_state()})
	check(result == OK, "Saved " + name + " screenshot")

func own_structures(kind: int) -> Array:
	return game.current.structures.filter(func(s): return s.player == player and s.kind == kind and s.hp > 0)

func own_units(kind: int) -> Array:
	return game.current.units.filter(func(u): return u.player == player and u.hp > 0 and int(u.get("kind", 0)) == kind)

func army() -> Array:
	return game.current.units.filter(func(u): return u.player == player and u.hp > 0 and int(u.get("kind", 0)) != 1)

func group_arrived(goals: Dictionary) -> bool:
	var units := army()
	if units.size() != goals.size(): return false
	for unit in units:
		if not goals.has(unit.id): return false
		var goal: Dictionary = goals[unit.id]
		if not unit.order_queue.is_empty() or unit.moving or unit.x != goal.x or unit.z != goal.z: return false
	return true

func foundry() -> Dictionary:
	var buildings := own_structures(1)
	return buildings[0] if not buildings.is_empty() else {}

func point_of(entity: Dictionary) -> Vector3:
	return Vector3(entity.x / 256.0, 0.5, entity.z / 256.0)

func nearest_deposit(kind: int, anchor: Dictionary) -> Dictionary:
	var result: Dictionary = {}
	var best := INF
	for candidate in game.current.deposits:
		if int(candidate.kind) != kind: continue
		var distance := Vector2(candidate.x - anchor.x, candidate.z - anchor.z).length_squared()
		if distance < best:
			best = distance
			result = candidate
	return result

func stop_workers(name: String) -> bool:
	await key(KEY_F1)
	var sequence_pre: int = game.current.result_sequences[player]
	label = name
	await key(KEY_S)
	if not check(await until(func(): return game.current.result_sequences[player] > sequence_pre and own_units(1).all(func(u): return u.order == 0), game.current.tick + 200), "Workers stop before exact economic accounting"): return false
	phase(name)
	return true

func select_foundry() -> bool:
	if not check(not foundry().is_empty(), "Own live Foundry exists for selection"): return false
	await mouse(MOUSE_BUTTON_LEFT, point_of(foundry()))
	return check(game.selected_entity.get("kind", -1) == 1 and game.selected_entity.get("player", -1) == player and game.selected.is_empty(), "Click selects the own Foundry")

func economic_key(name: String, code: Key, predicate: Callable, salvage_delta: int, flux_delta: int) -> bool:
	phase(name + "_before")
	var salvage_pre: int = game.current.salvage[player]
	var flux_pre: int = game.current.flux[player]
	var sequence_pre: int = game.current.result_sequences[player]
	label = name
	await key(code)
	if not check(await until(func(): return game.current.result_sequences[player] > sequence_pre and predicate.call(), game.current.tick + 200), name + " executes through lockstep"): return false
	phase(name + "_after")
	if not check(game.current.command_results[player] == 1 and int(game.current.salvage[player]) == salvage_pre + salvage_delta and int(game.current.flux[player]) == flux_pre + flux_delta, name + " has exact authoritative resource delta"): return false
	var building := foundry()
	if not check(game.order_mark.visible and Vector2(game.order_mark.position.x - building.x / 256.0, game.order_mark.position.z - building.z / 256.0).length() < 0.01, "Economic command feedback marks its Foundry"): return false
	return true

func exercise() -> bool:
	await frames()
	if not check(await until(func(): return game.network_state.get("ready", false) and str(game.network_state.get("state", "")) in ["running", "stalled"], 20), "Session ready"): return false
	if not check(initial_snapshot.map_id == 2 and initial_snapshot.tick == 0 and own_units(1).size() == 3 and own_structures(0).size() == 1, "Networked economy map: own anchor and three workers"): return false
	var anchor: Dictionary = own_structures(0)[0]
	var salvage_site := nearest_deposit(0, anchor)
	var flux_site := nearest_deposit(1, anchor)
	if not check(not salvage_site.is_empty() and not flux_site.is_empty(), "Both resource kinds exist"): return false
	check(not game._entity_visible(flux_site), "Contested flux starts hidden by fog")
	phase("flux_hidden")
	await key(KEY_F1)
	if not check(game.selected.size() == 3 and own_units(1).all(func(u): return u.id in game.selected), "F1 selects exactly the local player's workers"): return false
	label = "gather"
	await mouse(MOUSE_BUTTON_RIGHT, point_of(salvage_site))
	var extra_striders := 4 if player == 0 else 0
	var salvage_goal: int = game.current.foundry_cost + (2 + extra_striders) * game.current.strider_cost + game.current.lancer_cost
	# Watchdog for this scripted purchase route, not a gameplay/performance budget.
	# The first retained run needed 1962 ticks for player 1's smaller 275 goal.
	if not check(await until(func(): return int(game.current.salvage[player]) >= salvage_goal, 4000), "Lockstep gathering funds build, cancellation and army"): return false
	phase("harvesting")
	await capture("harvesting")
	if not await stop_workers("miners_stopped"): return false
	var site := Vector3.ZERO
	var valid := false
	for radius in range(3, 9):
		for dz in range(-radius, radius + 1):
			for dx in [-radius, radius]:
				var candidate := Vector3(anchor.x / 256.0 + dx + 0.5, 0, anchor.z / 256.0 + dz + 0.5)
				if not valid and game.bridge.can_build(int(candidate.x * 256), int(candidate.z * 256)):
					site = candidate
					valid = true
	if not check(valid, "Valid Foundry site exists near the local anchor"): return false
	phase("build_before")
	var salvage_before: int = game.current.salvage[player]
	label = "build"
	await key(KEY_B)
	await mouse(MOUSE_BUTTON_LEFT, site)
	if not check(await until(func(): return own_structures(1).size() == 1, game.current.tick + 200), "Build creates one Foundry"): return false
	phase("build_after")
	if not check(int(game.current.salvage[player]) == salvage_before - int(game.current.foundry_cost), "Foundry exact cost is debited with miners stopped"): return false
	if not check(await until(func(): return foundry().build_ticks >= int(game.current.build_duration), game.current.tick + 500), "Foundry construction completes"): return false
	phase("completed")
	await capture("completed")
	if not await select_foundry(): return false
	if not await economic_key("train", KEY_T, func(): return foundry().production_queue == 1, -int(game.current.strider_cost), 0): return false
	if not await economic_key("train_second", KEY_T, func(): return foundry().production_queue == 2, -int(game.current.strider_cost), 0): return false
	await capture("production-queued")
	if not await economic_key("cancel", KEY_X, func(): return foundry().production_queue == 1, int(game.current.strider_cost), 0): return false
	await capture("refunded")
	if not check(await until(func(): return own_units(0).size() == 1 and foundry().production_queue == 0, game.current.tick + 300), "Uncancelled Strider spawns"): return false
	phase("strider_spawned")
	# Clear the building's screen selection target before later purchases.
	await key(KEY_F2)
	label = "strider_rally"
	await mouse(MOUSE_BUTTON_RIGHT, point_of(anchor) + Vector3(0, 0, 5 if player == 0 else -5))
	await key(KEY_F1)
	label = "scout_flux"
	await mouse(MOUSE_BUTTON_RIGHT, point_of(flux_site) + Vector3(-3 if player == 0 else 3, 0, 0))
	if not check(await until(func(): return game._entity_visible(flux_site), game.current.tick + 1000), "Worker scouting reveals contested flux"): return false
	phase("flux_revealed")
	await capture("flux-revealed")
	label = "gather_flux"
	await mouse(MOUSE_BUTTON_RIGHT, point_of(flux_site))
	if not check(await until(func(): return int(game.current.flux[player]) >= int(game.current.research_cost) + int(game.current.lancer_flux_cost), game.current.tick + 2500), "Workers bank flux for research and Lancer"): return false
	phase("flux_gathered")
	await capture("flux-mined")
	if not await stop_workers("miners_stopped_flux"): return false
	if not await select_foundry(): return false
	if not await economic_key("research", KEY_G, func(): return int(game.current.research_ticks[player]) > 0, 0, -int(game.current.research_cost)): return false
	await capture("researching")
	if not check(await until(func(): return game.current.researched[player], game.current.tick + int(game.current.research_total_ticks) + 100), "Hardened Plating completes"): return false
	phase("researched")
	if not await economic_key("lancer", KEY_L, func(): return foundry().production_queue == 1 and foundry().queue_lancers == 1, -int(game.current.lancer_cost), -int(game.current.lancer_flux_cost)): return false
	await capture("lancer-queued")
	if not check("LANCER" in game.hud.selection.text, "Foundry HUD identifies the front queued Lancer"): return false
	# Queue reinforcements while the selected building is still unobscured.
	for n in range(extra_striders):
		var queue_size: int = foundry().production_queue
		if not await economic_key("reinforcement_" + str(n), KEY_T, func(): return foundry().production_queue == queue_size + 1, -int(game.current.strider_cost), 0): return false
	if not check(await until(func(): return own_units(2).size() == 1, game.current.tick + 300), "Lancer spawns through researched production"): return false
	phase("lancer_spawned")
	await capture("lancer")
	if not check(await until(func(): return army().size() == 2 + extra_striders and foundry().production_queue == 0, game.current.tick + 700), "Paid mixed army finishes production"): return false
	phase("army_ready")
	await key(KEY_F2)
	if not check(game.selected.size() == army().size() and army().all(func(u): return u.id in game.selected), "F2 selects all own combat units without workers"): return false
	await frames()
	if not check(game.hud.selection.text.begins_with("00 WORKERS / %02d STRIDERS / 01 LANCERS" % (1 + extra_striders)), "HUD reports the selected mixed army roles"): return false
	phase("group_queue_before")
	# Stay clear of the ridges while exercising formation/FIFO semantics; the later
	# anchor attack crosses the map through the ordinary pathfinding implementation.
	var rally := point_of(anchor) + Vector3(7 if player == 0 else -7, 0, 10 if player == 0 else -10)
	label = "group_move"
	await mouse(MOUSE_BUTTON_RIGHT, rally)
	label = "group_queue_move"
	await mouse(MOUSE_BUTTON_RIGHT, rally + Vector3(0, 0, 4 if player == 0 else -4), true)
	label = "group_queue_attack"
	await key(KEY_A, true)
	await mouse(MOUSE_BUTTON_LEFT, rally + Vector3(4 if player == 0 else -4, 0, 4 if player == 0 else -4), true)
	label = "group_queue_move_tail"
	await mouse(MOUSE_BUTTON_RIGHT, rally + Vector3(4 if player == 0 else -4, 0, 8 if player == 0 else -8), true)
	label = "group_queue_attack_tail"
	await key(KEY_A, true)
	await mouse(MOUSE_BUTTON_LEFT, rally + Vector3(0, 0, 8 if player == 0 else -8), true)
	if not check(await until(func(): return army().all(func(u): return u.order == 1 and u.order_queue.size() == 4), game.current.tick + 200), "All mixed-army units retain four Shift Move/AttackMove legs"): return false
	phase("group_queue_after")
	var goals: Dictionary = {}
	var final_goals: Dictionary = {}
	for unit in army():
		goals[str(unit.goal_x) + ":" + str(unit.goal_z)] = true
		final_goals[unit.id] = unit.order_queue[-1].duplicate()
	if not check(goals.size() == army().size() and game._order_paths().size() == army().size(), "Formation has distinct authoritative goals and selected overlays"): return false
	await navigate(rally)
	await capture("group-queued")
	if not check(await until(func(): return group_arrived(final_goals), game.current.tick + 1600), "Multi-unit queued route drains at exact formation destinations"): return false
	phase("group_queue_drained")
	await capture("group-arrived")
	if player == 1:
		label = "defender_hold"
		await key(KEY_H)
		if not check(await until(func(): return army().all(func(u): return u.order == 3), game.current.tick + 200), "Defender holds after its queued route"): return false
		phase("defender_ready")
	else:
		if not check(await until(func(): return game.current.researched[1] and game.current.units.any(func(u): return u.player == 1 and u.kind == 2 and u.hp > 0) and game.current.units.filter(func(u): return u.player == 1 and u.kind != 1 and u.hp > 0).all(func(u): return u.order == 3), game.current.tick + 1500), "Both players tech before the coordinated attack"): return false
		var enemy_anchor: Dictionary = game.current.structures.filter(func(s): return s.player == 1 and s.kind == 0)[0]
		label = "attack_anchor"
		await key(KEY_A)
		await mouse(MOUSE_BUTTON_LEFT, point_of(enemy_anchor))
		phase("attack_submitted")
	if not check(await until(func(): return game.current.units.any(func(u): return u.target != 0 or u.get("target_structure", 0) != 0), game.finish_tick - 500), "Networked army acquires combat targets"): return false
	phase("combat_started")
	await capture("combat")
	if not check(await until(func(): return game.current.structures.any(func(s): return s.player == 1 and s.kind == 0 and s.hp < initial_snapshot.structures[1].hp), game.finish_tick - 100), "Player 1 anchor takes authoritative damage"): return false
	phase("anchor_damaged")
	await navigate(point_of(game.current.structures[1]))
	await capture("anchor-damaged")
	if not check(await until(func(): return game.current.winner != -1, game.finish_tick - 10), "Resource-funded network route reaches an anchor outcome"): return false
	phase("outcome")
	if not check(game.current.winner == 0 and game.current.structures[1].hp <= 0, "Both peers report player 0 victory with destroyed opposing anchor"): return false
	await frames()
	if not check(game.hud.result.text.begins_with("VICTORY" if player == 0 else "DEFEAT"), "Local winner/loser result appears in HUD"): return false
	await capture("outcome")
	return errors.is_empty()

func run() -> void:
	done = await exercise()
	final_snapshot = game.current.duplicate(true)
	if not done: phase("aborted_" + stage)
	# Keep the real session and canonical recording through the shared terminal tick.
	while not game.completed and str(game.network_state.get("state", "")) not in ["complete", "error"]:
		await game.get_tree().process_frame

func report() -> Dictionary:
	return {"checks": checks, "errors": errors, "inputs": inputs, "phases": phases, "captures": captures,
		"initial_snapshot": initial_snapshot, "final_snapshot": final_snapshot,
		"fixture_done": done, "player": player, "stage": stage,
		"role": "attacker" if player == 0 else "bounded_passive_defender",
		"note": "Two real loopback clients using ordinary software InputEvents and fixed scripted roles. No human play, balance, LAN/Internet, responsiveness, production quality or shipping acceptance."}

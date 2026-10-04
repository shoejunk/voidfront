extends RefCounted
# Networked economy fixture: each of two packaged clients drives its own workers
# through the ordinary UI InputEvent path (gather, build a Foundry, train a Strider)
# over a real lockstep session. All state comes from the bridge snapshot.
var game
var checks: Array[String] = []
var errors: Array[String] = []
var inputs: Array[Dictionary] = []
var label := ""
var player := 0
var done := false

func _init(owner) -> void:
	game = owner
	player = game.local_player

func check(ok: bool, message: String) -> void:
	if ok: checks.append(message)
	else: errors.append(message)
	print("VOIDFRONT_NETECON_CHECK player=", player, " tick=", game.current.tick, " ok=", ok, " ", message)

func record_input(accepted: bool, order: int, at: Vector3, actors: Array) -> void:
	inputs.append({"label": label, "accepted": accepted, "order": order, "units": actors.duplicate(),
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
	while game.current.tick < mini(limit, game.finish_tick) and not predicate.call() and not game.completed:
		await game.get_tree().process_frame
	return bool(predicate.call())

func own_structures(kind: int) -> Array:
	return game.current.structures.filter(func(s): return s.player == player and s.kind == kind and s.hp > 0)

func own_units(kind: int) -> Array:
	return game.current.units.filter(func(u): return u.player == player and u.hp > 0 and int(u.get("kind", 0)) == kind)

func run() -> void:
	await frames()
	check(await until(func(): return game.network_state.get("ready", false) and str(game.network_state.get("state", "")) in ["running", "stalled"], 5), "Session ready")
	var initial: Dictionary = game.current
	check(initial.map_id == 2 and own_units(1).size() == 3 and own_structures(0).size() == 1, "Networked economy map: own anchor and three workers")
	var anchor: Dictionary = own_structures(0)[0]
	var deposit: Dictionary = {}
	var best := INF
	for candidate in initial.deposits:
		if int(candidate.kind) != 0: continue
		var distance := Vector2(candidate.x - anchor.x, candidate.z - anchor.z).length()
		if distance < best:
			best = distance
			deposit = candidate
	var deposit_at := Vector3(deposit.x / 256.0, 0.5, deposit.z / 256.0)
	await key(KEY_F1)
	var own_only: bool = game.selected.size() == 3
	for id in game.selected:
		for unit in game.current.units:
			if unit.id == id and unit.player != player: own_only = false
	check(own_only, "F1 selects exactly the local player's three workers")
	label = "gather"
	await mouse(MOUSE_BUTTON_RIGHT, deposit_at)
	var start_salvage: int = game.current.salvage[player]
	check(await until(func(): return int(game.current.salvage[player]) >= int(game.current.foundry_cost) + 10 and int(game.current.salvage[player]) > start_salvage, 1500), "Lockstep gathering funds a Foundry")
	# Pick a valid site near the anchor using the shared simulation's placement rule.
	var site := Vector3.ZERO
	var found := false
	for radius in range(3, 9):
		for dz in range(-radius, radius + 1):
			for dx in [-radius, radius]:
				var candidate := Vector3(anchor.x / 256.0 + dx + 0.5, 0, anchor.z / 256.0 + dz + 0.5)
				if not found and game.bridge.can_build(int(candidate.x * 256), int(candidate.z * 256)):
					site = candidate
					found = true
	check(found, "A valid Foundry site exists near the local anchor")
	var salvage_before: int = game.current.salvage[player]
	label = "build"
	await key(KEY_B)
	await mouse(MOUSE_BUTTON_LEFT, site)
	check(await until(func(): return own_structures(1).size() == 1, game.current.tick + 200), "Build order creates one Foundry on both peers' simulation")
	check(int(game.current.salvage[player]) <= salvage_before - int(game.current.foundry_cost) + 40, "Foundry cost is debited (income may offset)")
	check(await until(func(): return own_structures(1).size() == 1 and own_structures(1)[0].build_ticks >= int(game.current.build_duration), game.current.tick + 400), "Foundry construction completes")
	label = "gather_again"
	await mouse(MOUSE_BUTTON_RIGHT, deposit_at)
	check(await until(func(): return int(game.current.salvage[player]) >= int(game.current.strider_cost), game.current.tick + 800), "Income funds a Strider")
	var foundry: Dictionary = own_structures(1)[0]
	label = "train"
	await mouse(MOUSE_BUTTON_LEFT, Vector3(foundry.x / 256.0, 0.5, foundry.z / 256.0))
	check(game.selected_entity.get("kind", -1) == 1, "Click selects the own Foundry")
	await key(KEY_T)
	check(await until(func(): return own_units(0).size() >= 1, game.current.tick + 400), "Trained Strider spawns")
	done = true
	# Idle until the session completes; the main loop requests the report then.
	while not game.completed and str(game.network_state.get("state", "")) not in ["complete", "error"]:
		await game.get_tree().process_frame

func report() -> Dictionary:
	return {"checks": checks, "errors": errors, "inputs": inputs, "fixture_done": done, "player": player}

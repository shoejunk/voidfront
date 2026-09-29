extends "res://production_smoke.gd"
# Ordinary input route, either real time or eight ticks per rendered frame.
# No state injection. Real-time playability and complete RTS scope remain open.
var enemy_gathered := false
var enemy_built := false
var enemy_trained := false
var enemy_attacked := false
var destroyed_structure_hidden := false
var structure_attack_requests: Array[Dictionary] = []

func record_structure_attack(unit: Dictionary, clip: String) -> void:
	structure_attack_requests.append({"tick": game.current.tick, "unit": unit.id,
		"target_structure": unit.target_structure, "clip": clip, "cooldown": unit.cooldown})

func tick() -> void:
	if int(game.current.tick) > last_tick:
		samples.append(game.current.duplicate(true))
		last_tick = int(game.current.tick)
	for unit in game.current.units:
		if unit.player != 1: continue
		if unit.kind == 1 and unit.order == 4: enemy_gathered = true
		if unit.kind == 0:
			enemy_trained = true
			if unit.order == 2: enemy_attacked = true
	for structure in game.current.structures:
		if structure.player == 1 and structure.kind == 1 and structure.build_ticks == game.current.build_duration: enemy_built = true
	if game.current.tick >= game.finish_tick: game.completed = true

func run() -> void:
	await frames()
	check(initial.map_id == 2 and initial.get("enemy_ai", false), "Ordinary economy starts with opposing AI enabled")
	var deposit: Dictionary = initial.deposits[0]
	await key(KEY_F2)
	label = "gather_for_defence"
	await mouse(MOUSE_BUTTON_RIGHT, Vector3(deposit.x / 256.0, 0.5, deposit.z / 256.0))
	check(await until(func(): return int(game.current.salvage[0]) >= 150, game.finish_tick - 1000), "Workers gather salvage for a Foundry and Strider")
	label = "stop_workers"
	await key(KEY_S)
	var site := Vector3(9.5, 0, 12.5)
	label = "build_foundry"
	await key(KEY_B)
	await mouse(MOUSE_BUTTON_LEFT, site)
	check(await until(func(): return own_foundries().size() == 1 and own_foundries()[0].build_ticks == game.current.build_duration, game.finish_tick - 500), "Player completes resource-funded Foundry against active AI")
	await mouse(MOUSE_BUTTON_LEFT, site + Vector3(0, 0.5, 0))
	label = "train_defender"
	await key(KEY_T)
	check(await until(func(): return not striders().is_empty(), game.finish_tick - 200), "Player trains a paid Strider against active AI")
	await capture("opposing-production")
	if not striders().is_empty():
		var unit: Dictionary = striders()[0]
		await mouse(MOUSE_BUTTON_LEFT, Vector3(unit.x / 256.0, 0.5, unit.z / 256.0))
		check(game.selected == [unit.id], "Produced Strider is selectable in active match")
		var anchor: Dictionary = initial.structures[1]
		label = "enemy_anchor_context_attack"
		await mouse(MOUSE_BUTTON_RIGHT, Vector3(anchor.x / 256.0, 0.5, anchor.z / 256.0))
		check(not inputs.is_empty() and inputs.back().label == label and inputs.back().accepted and inputs.back().order == 2, "Enemy-anchor context click submits canonical AttackMove")
		check(await until(func():
			for moved in game.current.units:
				if moved.id == unit.id and Vector2(moved.x - unit.x, moved.z - unit.z).length() > 256: return true
			return false, game.finish_tick - 100), "Produced Strider advances after attack input")
	# Deliberately stop production: one defender should lose to the growing AI.
	check(await until(func(): return game.current.structures[0].hp < initial.structures[0].hp, game.finish_tick), "Player anchor takes authoritative damage")
	check(game.current.winner == -1, "Anchor damage is visible before the match ends")
	await capture("anchor-under-attack")
	check(await until(func(): return game.current.winner != -1, game.finish_tick), "Unreinforced player reaches an anchor-based match outcome")
	check(not structure_attack_requests.is_empty(), "Renderer requests structure-target beam and attack animation")
	check(structure_attack_requests.any(func(event): return "attack" in str(event.clip).to_lower()), "Structure-target attacker uses its exported attack clip")
	check(enemy_gathered and enemy_built and enemy_trained and enemy_attacked, "Opponent visibly gathers, completes Foundry, trains and attack-moves")
	check(game.current.winner == 1, "AI wins when player's command anchor is destroyed")
	var dead_anchor: Dictionary = game.current.structures[0]
	check(dead_anchor.hp <= 0, "Defeat snapshot contains destroyed player anchor")
	game._present_economy()
	await frames()
	var actor_key: String = "structures" + str(dead_anchor.id)
	destroyed_structure_hidden = game.economy_actors.has(actor_key) and not game.economy_actors[actor_key].root.visible
	check(destroyed_structure_hidden, "Destroyed anchor is absent from rendered scene")
	check(game.hud.result.text.begins_with("DEFEAT"), "Defeat and restart instruction appear in HUD")
	check(game.selected.is_empty(), "Dead combat selection is cleared")
	await capture("defeat")
	game.completed = true
	var final_snapshot: Dictionary = game.current.duplicate(true)
	var replay_path: String = game.report_path.get_basename() + ".vfr"
	var replay: PackedByteArray = game.bridge.replay_bytes()
	var replay_file := FileAccess.open(replay_path, FileAccess.WRITE)
	check(replay_file != null and not replay.is_empty(), "Both-player match replay bytes available")
	if replay_file:
		replay_file.store_buffer(replay)
		replay_file.close()
	if not game.capture_path.is_empty():
		await RenderingServer.frame_post_draw
		check(game.get_viewport().get_texture().get_image().save_png(game.capture_path) == OK, "Final screenshot saved")
	await key(KEY_R)
	var restart_snapshot: Dictionary = game.current.duplicate(true)
	check(game.current.tick == 0 and game.current.hash == initial.hash and game.current.enemy_ai, "Input restart restores initial state and opposing AI")
	check(game.selected.is_empty() and game.selected_entity.is_empty() and not game.build_pending and not game.order_mark.visible, "Restart clears stale selections and feedback")
	game.completed = false
	await until(func(): return game.current.tick >= 80, 80)
	check(game.current.deposits[1].remaining < initial.deposits[1].remaining or game.current.units.any(func(u): return u.player == 1 and u.order == 4), "Restarted AI resumes gathering through commands")
	game.completed = true
	var after_restart_snapshot: Dictionary = game.current.duplicate(true)
	await capture("restarted")
	var trace: Array = []
	for snapshot in samples: trace.append({"tick": snapshot.tick, "hash": snapshot.hash})
	var report := {"ok": errors.is_empty(), "mode": "match", "setup": {"map_id": initial.map_id, "width": initial.width, "height": initial.height, "protocol": initial.protocol, "content_id": initial.content_id},
		"initial_snapshot": initial, "final_snapshot": final_snapshot, "restart_snapshot": restart_snapshot, "after_restart_snapshot": after_restart_snapshot, "snapshots": samples, "trace": trace, "inputs": inputs,
		"checks": checks, "errors": errors, "captures": captures, "replay_path": replay_path,
		"timing_mode": "real_time_20hz" if game.match_realtime else "accelerated_eight_ticks_per_frame",
		"ticks_per_frame": null if game.match_realtime else 8, "simulation_hz": 20, "structure_attack_requests": structure_attack_requests,
		"note": "Packaged software InputEvents: resource-funded player production, enemy-building attack-move, active economic AI, player defeat by anchor destruction and input restart. Replay contains both players. Real-time mode uses the normal accumulator (ticks per frame varies); accelerated mode batches eight ticks per frame. Renderer requests prove the presentation branch executed, not that every shot was visible. No human play, balance, victory route, full RTS scope or performance acceptance."}
	var output := FileAccess.open(game.report_path, FileAccess.WRITE)
	if output: output.store_string(JSON.stringify(report, "\t"))
	else: errors.append("Cannot save report")
	print("VOIDFRONT_MATCH_SMOKE ok=", errors.is_empty(), " tick=", final_snapshot.tick, " errors=", errors)
	game.get_tree().quit(0 if errors.is_empty() else 1)

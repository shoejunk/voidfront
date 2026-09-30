extends "res://match_smoke.gd"
# Snapshot-observing software player; every order uses ordinary keyboard/mouse input.
# Keep mining while one worker builds, reinforce defensively, then counterattack.
var role_selection_events: Array[Dictionary] = []

func role_key(code: Key) -> void:
	var before: Dictionary = game.current.duplicate(true)
	var pending_before := {"build": game.build_pending, "attack": game.attack_pending}
	await key(code)
	role_selection_events.append({"tick": game.current.tick, "key": "F1" if code == KEY_F1 else "F2", "selected": game.selected.duplicate(), "before": before, "after": game.current.duplicate(true), "pending_before": pending_before, "pending_after": {"build": game.build_pending, "attack": game.attack_pending}})

func select_unit(unit: Dictionary) -> void:
	# Click the displayed actor, the same visible selection target used by _select.
	await mouse(MOUSE_BUTTON_LEFT, game.actors[unit.id].root.position + Vector3(0, 0.5, 0))

func run() -> void:
	await frames()
	check(initial.map_id == 2 and initial.get("enemy_ai", false), "Ordinary economy starts with opposing AI enabled")
	var deposit: Dictionary = initial.deposits[0]
	var deposit_point := Vector3(deposit.x / 256.0, 0.5, deposit.z / 256.0)
	await role_key(KEY_F2)
	check(game.selected.is_empty(), "Army hotkey selects nothing in workers-only start")
	await role_key(KEY_F1)
	check(game.selected == [1, 2, 3], "Worker hotkey selects exactly three own live workers")
	await key(KEY_B)
	await role_key(KEY_F2)
	check(not game.build_pending and not game.attack_pending, "Role selection cancels pending build or attack targeting")
	await role_key(KEY_F1)
	await key(KEY_A)
	await role_key(KEY_F1)
	check(not game.attack_pending and not game.build_pending, "Worker selection clears pending attack targeting")
	label = "gather_for_victory"
	await mouse(MOUSE_BUTTON_RIGHT, deposit_point)
	check(await until(func(): return int(game.current.salvage[0]) >= 100, 1800), "Workers fund Foundry through gathering")
	await select_unit(game.current.units[0])
	var site := Vector3(9.5, 0, 15.5)
	label = "build_foundry"
	await key(KEY_B)
	await mouse(MOUSE_BUTTON_LEFT, site)
	check(await until(func(): return own_foundries().size() == 1 and own_foundries()[0].build_ticks == game.current.build_duration, 2400), "Single worker completes resource-funded Foundry while others gather")
	await select_unit(game.current.units[0])
	label = "builder_resumes_gathering"
	await mouse(MOUSE_BUTTON_RIGHT, deposit_point)
	await capture("opposing-production")
	var commanded: Dictionary = {}
	var attacking := false
	var next_queue_tick := 0
	var anchor: Dictionary = initial.structures[1]
	while game.current.winner == -1 and game.current.tick < game.finish_tick:
		if not attacking and striders().size() >= 5:
			attacking = true
			await role_key(KEY_F1)
			var live_workers: Array = game.current.units.filter(func(u): return u.player == 0 and u.kind == 1 and u.hp > 0)
			check(game.selected.size() == live_workers.size() and live_workers.all(func(u): return u.id in game.selected), "Worker hotkey excludes combat, enemy and dead units during mixed army")
			await key(KEY_B)
			await role_key(KEY_F2)
			var live_army: Array = striders()
			check(game.selected.size() == live_army.size() and live_army.all(func(u): return u.id in game.selected), "Army hotkey selects exactly live own Striders without workers")
			check(not game.build_pending and not game.attack_pending, "Mixed army role switch clears construction targeting")
			var gathering_ids: Array = live_workers.filter(func(u): return u.order == 4).map(func(u): return u.id)
			label = "army_hotkey_anchor_attack"
			await key(KEY_A)
			await mouse(MOUSE_BUTTON_LEFT, Vector3(anchor.x / 256.0, 0.5, anchor.z / 256.0))
			check(not gathering_ids.is_empty() and game.current.units.filter(func(u): return u.id in gathering_ids).all(func(u): return u.order == 4), "Army attack preserves gathering orders for working miners")
			for unit in live_army: commanded[unit.id] = true
			await capture("counterattack")
		for unit in striders():
			if commanded.has(unit.id): continue
			await select_unit(unit)
			label = "enemy_spawn_scout_attack" if attacking else "defensive_rally"
			if attacking:
				await key(KEY_A)
				await mouse(MOUSE_BUTTON_LEFT, Vector3(anchor.x / 256.0, 0.5, anchor.z / 256.0))
			else:
				await key(KEY_A)
				await mouse(MOUSE_BUTTON_LEFT, Vector3(10.0, 0, 12.5 + float(unit.id % 3) * 0.6))
			commanded[unit.id] = true
		if game.current.tick >= next_queue_tick and game.current.salvage[0] >= game.current.strider_cost and not own_foundries().is_empty() and own_foundries()[0].hp > 0 and own_foundries()[0].production_queue < 5 and game.current.population_used[0] + game.current.population_reserved[0] < 12:
			await mouse(MOUSE_BUTTON_LEFT, site + Vector3(0, 0.5, 0))
			label = "train_reinforcement"
			await key(KEY_T)
			next_queue_tick = game.current.tick + 20
		await frames()
	check(attacking, "Paid defensive army grows into a counterattack")
	check(game.current.winner != -1, "Resource-funded player route reaches anchor outcome")
	check(not structure_attack_requests.is_empty(), "Renderer requests structure-target beam and attack animation")
	check(structure_attack_requests.any(func(event): return "attack" in str(event.clip).to_lower()), "Structure-target attacker uses its exported attack clip")
	check(enemy_gathered and enemy_built and enemy_trained and enemy_attacked, "Opponent visibly gathers, completes Foundry, trains and attack-moves")
	check(game.current.winner == 0, "Player wins when opposing command anchor is destroyed")
	var dead_anchor: Dictionary = game.current.structures[1]
	check(dead_anchor.hp <= 0, "Victory snapshot contains destroyed opposing anchor")
	game._present_economy()
	await frames()
	var actor_key: String = "structures" + str(dead_anchor.id)
	destroyed_structure_hidden = game.economy_actors.has(actor_key) and not game.economy_actors[actor_key].root.visible
	check(destroyed_structure_hidden, "Destroyed anchor is absent from rendered scene")
	check(game.hud.result.text.begins_with("VICTORY"), "Victory and restart instruction appear in HUD")
	await capture("victory")
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
	var report := {"ok": errors.is_empty(), "mode": "victory", "expected_winner": 0, "role_selection_events": role_selection_events, "setup": {"map_id": initial.map_id, "width": initial.width, "height": initial.height, "protocol": initial.protocol, "content_id": initial.content_id},
		"initial_snapshot": initial, "final_snapshot": final_snapshot, "restart_snapshot": restart_snapshot, "after_restart_snapshot": after_restart_snapshot, "snapshots": samples, "trace": trace, "inputs": inputs,
		"checks": checks, "errors": errors, "captures": captures, "replay_path": replay_path,
		"timing_mode": "real_time_20hz" if game.match_realtime else "accelerated_eight_ticks_per_frame",
		"ticks_per_frame": null if game.match_realtime else 8, "simulation_hz": 20, "structure_attack_requests": structure_attack_requests,
		"note": "Packaged software InputEvents: resource-funded player production, enemy-building attack-move, active economic AI, player victory by anchor destruction and input restart. Replay contains both players. Real-time mode uses the normal accumulator (ticks per frame varies); accelerated mode batches eight ticks per frame. Renderer requests prove the presentation branch executed, not that every shot was visible. No human play, balance, full RTS scope or performance acceptance."}
	var output := FileAccess.open(game.report_path, FileAccess.WRITE)
	if output: output.store_string(JSON.stringify(report, "\t"))
	else: errors.append("Cannot save report")
	print("VOIDFRONT_VICTORY_SMOKE ok=", errors.is_empty(), " tick=", final_snapshot.tick, " errors=", errors)
	game.get_tree().quit(0 if errors.is_empty() else 1)

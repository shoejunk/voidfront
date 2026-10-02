extends "res://production_smoke.gd"
# Packaged InputEvent fixture: scout the dark crossing, mine contested flux and
# research Hardened Plating. All state is read from bridge snapshots.

func flux_deposits() -> Array:
	return game.current.deposits.filter(func(d): return int(d.get("kind", 0)) == 1)

func run() -> void:
	await frames()
	check(initial.map_id == 2 and initial.units.size() == 6, "Flux fixture starts from economy workers")
	check(flux_deposits().size() == 2 and int(initial.flux[0]) == 0, "Two contested flux deposits start unmined")
	var salvage_site: Dictionary = initial.deposits[0]
	var flux_site: Dictionary = flux_deposits()[0]
	check(not game._entity_visible(flux_site), "Flux deposit starts hidden by fog")
	await key(KEY_F1)
	label = "gather_salvage"
	await mouse(MOUSE_BUTTON_RIGHT, Vector3(salvage_site.x / 256.0, 0.5, salvage_site.z / 256.0))
	check(await until(func(): return int(game.current.salvage[0]) >= 100, 900), "Salvage funds the Foundry")
	label = "stop_workers"
	await key(KEY_S)
	await advance_ticks(3)
	var site := Vector3(9.5, 0, 12.5)
	label = "build_foundry"
	await key(KEY_B)
	await mouse(MOUSE_BUTTON_LEFT, site)
	check(await until(func(): return own_foundries().size() == 1 and own_foundries()[0].build_ticks == game.current.build_duration, 1500), "Worker completes the Foundry")
	await key(KEY_F1)
	label = "scout_crossing"
	await mouse(MOUSE_BUTTON_RIGHT, Vector3(26.5, 0, 24.5))
	check(await until(func(): return game._entity_visible(game.current.deposits[flux_site.id - 1]), 2000), "Scouting workers reveal the flux deposit")
	await capture("flux-revealed")
	label = "gather_flux"
	await mouse(MOUSE_BUTTON_RIGHT, Vector3(flux_site.x / 256.0, 0.5, flux_site.z / 256.0))
	await advance_ticks(5)
	check(game.current.command_results[0] == 1, "Gather order on flux accepted")
	var salvage_before: int = game.current.salvage[0]
	var salvage_cargo := 0
	for unit in game.current.units:
		if unit.player == 0 and int(unit.get("cargo_kind", 0)) == 0: salvage_cargo += int(unit.cargo)
	check(await until(func(): return int(game.current.flux[0]) >= game.current.research_cost + game.current.lancer_flux_cost, game.finish_tick - 800), "Workers deliver enough flux for research")
	check(game.current.salvage[0] - salvage_before <= salvage_cargo, "Flux deliveries never enter the salvage bank (only pre-existing salvage cargo)")
	var flux_total := 0
	for deposit in flux_deposits(): flux_total += 1000 - int(deposit.remaining)
	var flux_banked: int = game.current.flux[0]
	var flux_carried := 0
	for unit in game.current.units:
		if unit.player == 0 and int(unit.get("cargo_kind", 0)) == 1: flux_carried += int(unit.cargo)
	check(flux_total == flux_banked + flux_carried, "Mined flux equals banked plus carried flux")
	await capture("flux-mined")
	await mouse(MOUSE_BUTTON_LEFT, site + Vector3(0, 0.5, 0))
	check(game.selected_entity.get("kind", -1) == 1, "Foundry selected for research")
	var flux_before_research: int = game.current.flux[0]
	label = "research"
	await key(KEY_G)
	await advance_ticks(3)
	check(game.current.command_results[0] == 1 and int(game.current.research_ticks[0]) > 0, "Research begins")
	check(game.current.flux[0] <= flux_before_research - game.current.research_cost + 15, "Research charges displayed flux cost")
	check(game.economy_notice.begins_with("Hardened Plating research submitted") or game.economy_notice == "Order accepted by simulation", "Research acknowledgement is visible in HUD")
	label = "duplicate_research"
	await key(KEY_G)
	await advance_ticks(3)
	check(game.current.command_results[0] == 13 and game.economy_notice.begins_with("Hardened Plating is already"), "Duplicate research rejects visibly")
	await capture("researching")
	label = "lancer_before_research"
	await key(KEY_L)
	await advance_ticks(3)
	check(game.current.command_results[0] == 14 and game.economy_notice.begins_with("Lancers require"), "Lancer before research rejects visibly")
	check(await until(func(): return game.current.researched[0], game.finish_tick - 5), "Hardened Plating completes")
	await capture("researched")
	# Workers return to salvage so the Lancer (salvage plus flux) becomes affordable.
	await key(KEY_F1)
	label = "gather_salvage_for_lancer"
	await mouse(MOUSE_BUTTON_RIGHT, Vector3(salvage_site.x / 256.0, 0.5, salvage_site.z / 256.0))
	check(await until(func(): return int(game.current.salvage[0]) >= game.current.lancer_cost and int(game.current.flux[0]) >= game.current.lancer_flux_cost, game.finish_tick - 150), "Resources for a Lancer accumulate")
	await mouse(MOUSE_BUTTON_LEFT, site + Vector3(0, 0.5, 0))
	var salvage_pre: int = game.current.salvage[0]
	var flux_pre: int = game.current.flux[0]
	label = "train_lancer"
	await key(KEY_L)
	await advance_ticks(3)
	check(game.current.command_results[0] == 1 and own_foundries()[0].queue_lancers == 1, "Lancer queued behind research gate")
	check(game.current.salvage[0] <= salvage_pre - game.current.lancer_cost + 20 and game.current.flux[0] <= flux_pre - game.current.lancer_flux_cost + 10, "Lancer charges salvage and flux")
	check(await until(func(): return game.current.units.any(func(u): return u.player == 0 and int(u.get("kind", 0)) == 2 and u.hp == 70), game.finish_tick - 5), "Lancer spawns with its own hp")
	await capture("lancer")
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
	check(int(game.current.flux[0]) == 0 and not game.current.researched[0], "Restart clears flux and research")
	var trace: Array = []
	for snapshot in samples: trace.append({"tick": snapshot.tick, "hash": snapshot.hash})
	var report := {"ok": errors.is_empty(), "mode": "flux", "setup": {"map_id": initial.map_id, "width": initial.width, "height": initial.height, "protocol": initial.protocol, "content_id": initial.content_id},
		"initial_snapshot": initial, "final_snapshot": final_snapshot, "snapshots": samples, "trace": trace, "inputs": inputs,
		"checks": checks, "errors": errors, "captures": captures, "replay_path": replay_path,
		"note": "Packaged software InputEvents: salvage, Foundry, scouting through fog, contested flux mining, Hardened Plating research and restart. No full match, human usability, balance or performance acceptance."}
	var output := FileAccess.open(game.report_path, FileAccess.WRITE)
	if output: output.store_string(JSON.stringify(report, "\t"))
	else: errors.append("Cannot save report")
	print("VOIDFRONT_FLUX ok=", errors.is_empty(), " tick=", final_snapshot.tick, " errors=", errors)
	game.get_tree().quit(0 if errors.is_empty() else 1)

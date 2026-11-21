extends RefCounted

# Two capped Glass Reach rounds. In the second round, hold the host's next
# UDP poll after the final advance, until the guest observes the real TCP EOF.
const CardAction = preload("res://command_card.gd").Action
var game
var folder: String
var player: int
var events: Array[Dictionary] = []
var rounds: Array[Dictionary] = []

func _init(owner) -> void:
	game = owner
	folder = game.report_path
	player = game.local_player

func fail(reason: String) -> bool:
	game.set_process(true)
	push_error("Terminal loss fixture p%d: %s" % [player, reason])
	game.get_tree().quit(1)
	return false

func until(predicate: Callable, label: String) -> bool:
	var deadline := Time.get_ticks_msec() + 20000
	while not predicate.call():
		if Time.get_ticks_msec() >= deadline: return fail(label)
		await game.get_tree().process_frame
	return true

func mark(label: String) -> bool:
	var file := FileAccess.open(folder.path_join(label), FileAccess.WRITE)
	if not file: return fail("Cannot write " + label)
	file.close()
	return true

func wait_marker(label: String) -> bool:
	return await until(func(): return FileAccess.file_exists(folder.path_join(label)), "Waiting for " + label)

func capture(name: String) -> bool:
	await game.get_tree().process_frame
	await RenderingServer.frame_post_draw
	if game.get_viewport().get_texture().get_image().save_png(folder.path_join(name)) != OK:
		return fail("Cannot capture " + name)
	return true

func witness(label: String) -> Dictionary:
	var state: Dictionary = game.bridge.network_status()
	var row := {"event": label, "usec": Time.get_ticks_usec(), "state": state.state,
		"tick": state.tick, "confirmed_ticks": state.confirmed_ticks, "hash": state.hash,
		"session": game.lobby_round.session, "lobby_present": game.lobby != null,
		"archive": game.lobby_archive_path, "notice": game.lobby_notice}
	events.append(row)
	print("VOIDFRONT_TERMINAL_LOSS ", JSON.stringify(row))
	return row

func inject_at_terminal(cap: int) -> bool:
	if not await until(func(): return game.network_state.state == "finishing", "Did not reach finishing"):
		return false
	if player == 0:
		# process_frame resumes before the next Node._process. The first native
		# poll that reached cap has generated, but has yet to send, its checksum.
		game.set_process(false)
	var before := witness("finishing")
	if before.tick != cap or before.confirmed_ticks >= cap or not before.lobby_present:
		return fail("Missing outstanding final checksum at cap")
	if player == 0:
		if not await wait_marker("guest-finishing"): return false
		witness("disconnecting-tcp")
		game.lobby._peer.disconnect_from_host()
		if not await wait_marker("guest-observed-eof"): return false
		game.set_process(true)
	else:
		if not mark("guest-finishing"): return false
		if not await until(func(): return game.lobby == null, "Receiver did not observe TCP loss"): return false
		var observed := witness("observed-eof")
		if observed.state != "finishing" or observed.confirmed_ticks >= cap or not observed.archive.is_empty():
			return fail("Receiver did not preserve the active, unconfirmed native finish")
		if not mark("guest-observed-eof"): return false
	return true

func run_round(index: int, seed: int, delay: int, cap: int) -> bool:
	var previous_id := str(game.lobby_round.get("session", ""))
	if player == 0:
		if not game.lobby.offer_match(seed, delay, cap): return fail("Could not offer capped round")
	else:
		if not await until(func(): return not game.lobby.offer.is_empty(), "No new offer"): return false
		if not await capture("p1-setup-offer.png" if index == 1 else "p1-rematch-offer.png"): return false
		game._smoke_key(KEY_ENTER)
	if not await until(func(): return str(game.lobby_round.get("session", "")) not in ["", previous_id] and game.commands_available(), "New round did not become ready"):
		return false
	var initial: Dictionary = game.current
	if index == 1:
		# Verify that Home restores each genuine peer's own starting view.
		var home: Vector3 = game.camera_target
		var minimap: Rect2 = game.hud.minimap_rect()
		var away := minimap.position + minimap.size * (0.95 if player == 0 else 0.05)
		game._smoke_mouse(MOUSE_BUTTON_LEFT, away, true)
		game._smoke_mouse(MOUSE_BUTTON_LEFT, away, false)
		if not await until(func(): return game.camera_target.distance_to(home) > 5.0, "Minimap did not move away from home"): return false
		game._smoke_key(KEY_HOME)
		if not await until(func(): return game.camera_target.distance_to(home) < 0.01, "Home did not restore the peer's spawn view"): return false
		var anchor: Dictionary = initial.structures[player]
		var at := Vector3(anchor.x / 256.0, 0, anchor.z / 256.0)
		if anchor.player != player or not game.get_viewport().get_visible_rect().has_point(game.camera.unproject_position(at)):
			return fail("The peer's own anchor is outside its Home view")
		if not await capture("p%d-home.png" % player): return false
	game._smoke_key(KEY_F1)
	var button: Button = game.hud.command_card.buttons[CardAction.STOP]
	if not await until(func(): return game.selected.size() == 3 and button.is_visible_in_tree(), "Workers' Stop button is unavailable"):
		return false
	# Let the container lay out the newly visible buttons before sending mouse input.
	await game.get_tree().process_frame
	var center := button.get_global_rect().get_center()
	var actors: Array = game.selected.duplicate()
	game._smoke_mouse(MOUSE_BUTTON_LEFT, center, true)
	game._smoke_mouse(MOUSE_BUTTON_LEFT, center, false)
	if index == 2 and not await inject_at_terminal(cap): return false
	if not await until(func(): return not game.lobby_archive_path.is_empty(), "Production archive never appeared"):
		return false
	witness("archived-%d" % index)
	if index == 2:
		if not await capture("p%d-terminal.png" % player): return false
		var detail: Label = game.hud.message_detail
		if not detail.is_visible_in_tree() or not detail.text.contains("Replay saved") or not detail.text.contains("Press M"):
			return fail("Confirmed match did not show its saved replay and new setup action")
	var native: Dictionary = game.bridge.network_report()
	var replay := FileAccess.open(folder.path_join("r%d-p%d.vfr" % [index, player]), FileAccess.WRITE)
	if not replay or not replay.store_buffer(native.replay): return fail("Cannot retain replay")
	replay.close()
	native.erase("replay")
	rounds.append({"lobby": game.lobby_round.duplicate(), "archive": game.lobby_archive_path,
		"network": native, "winner": game.current.winner, "selected": actors,
		"fixture": {"initial_snapshot": {"protocol": initial.protocol, "width": initial.width,
			"height": initial.height, "content_id": initial.content_id}}})
	if not mark("p%d-archived-%d" % [player, index]): return false
	return await wait_marker("p%d-archived-%d" % [1 - player, index])

func run() -> void:
	if player == 1 and not await wait_marker("host-listening"): return
	game._smoke_key(KEY_H if player == 0 else KEY_J)
	if player == 0:
		if not await until(func(): return game.lobby != null, "Host lobby failed to open"): return
		if not mark("host-listening"): return
	if not await until(func(): return game.lobby and game.lobby.connected, "TCP connection failed"): return
	if not await run_round(1, 7, 2, 32): return
	if not await run_round(2, 8, 3, 64): return
	var report := FileAccess.open(folder.path_join("p%d.json" % player), FileAccess.WRITE)
	if not report:
		fail("Cannot write report")
		return
	report.store_string(JSON.stringify({"pid": OS.get_process_id(), "player": player,
		"rounds": rounds, "events": events}, "\t"))
	report.close()
	game.get_tree().quit()

extends RefCounted

# Test driver layered over the same lobby keys and economy fixture as the client.
var game
var root_path: String
var player: int
var round := 0
var finished := 0
var ok := true

func _init(owner) -> void:
	game = owner
	root_path = game.report_path
	player = game.local_player

func until(predicate: Callable, seconds: float) -> bool:
	var end := Time.get_ticks_msec() + int(seconds * 1000)
	while Time.get_ticks_msec() < end:
		if predicate.call(): return true
		if not ok: return false
		await game.get_tree().process_frame
	return false

func run() -> void:
	var ready_marker := root_path.path_join("host-listening")
	if player == 1 and not await until(func(): return FileAccess.file_exists(ready_marker), 15):
		fail("Host did not start listening")
		return
	game._smoke_key(KEY_H if player == 0 else KEY_J)
	if player == 0:
		if not await until(func(): return game.lobby != null, 5):
			fail("Host could not open the lobby")
			return
		var marker := FileAccess.open(ready_marker, FileAccess.WRITE)
		if marker == null:
			fail("Could not mark host listening")
			return
		marker.close()
	if not await until(func(): return game.lobby and game.lobby.connected, 15):
		fail("Connection did not open (role %d, setup %s): %s" % [player, game.setup_open, game.lobby_notice])
		return
	for i in range(1, 3):
		if player == 0:
			if i == 2:
				game._smoke_key(KEY_BRACKETRIGHT)
				game._smoke_key(KEY_EQUAL)
				await game.get_tree().process_frame
			game._smoke_key(KEY_ENTER)
		else:
			if not await until(func(): return game.lobby and not game.lobby.offer.is_empty(), 15):
				fail("Guest did not receive an offer")
				return
			await game.get_tree().process_frame
			await RenderingServer.frame_post_draw
			var path := root_path.path_join("round%d/p1-offer.png" % i)
			if game.get_viewport().get_texture().get_image().save_png(path) != OK:
				fail("Guest could not save the offer capture")
				return
			game._smoke_key(KEY_ENTER)
		if not await until(func(): return round == i, 15):
			fail("Round did not start")
			return
		if not await until(func(): return finished == i, 650):
			fail("Round did not reach a confirmed outcome")
			return
	# Let both clients finish recording before this process closes the lobby.
	if not await until(func(): return FileAccess.file_exists(root_path.path_join("p%d.finished" % (1 - player))), 15):
		fail("Peer's final report was not written")
		return
	game.get_tree().quit()

func match_started() -> void:
	round += 1
	var prefix := root_path.path_join("round%d/p%d" % [round, player])
	game.report_path = prefix + ".json"
	game.capture_path = prefix + ".png"
	game.network_economy_smoke = true
	game.net_economy_fixture = preload("res://net_economy_smoke.gd").new(game)
	game.net_economy_fixture.run.call_deferred()

func round_finished(report: Dictionary) -> void:
	if not report.ok:
		fail("Round evidence is incomplete")
		return
	finished = round
	if finished == 2:
		var marker := FileAccess.open(root_path.path_join("p%d.finished" % player), FileAccess.WRITE)
		if marker == null:
			fail("Could not mark reports finished")
			return
		marker.close()

func fail(message: String) -> void:
	if not ok: return
	ok = false
	push_error("Rematch fixture: " + message)
	game.get_tree().quit(1)

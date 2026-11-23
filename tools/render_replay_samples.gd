extends SceneTree

# The native reader supplies both adjacent snapshots for each still.
# Only the scene, selection, camera and interpolation are controlled here.
func _initialize() -> void:
	run.call_deferred()

# Godot parses JSON numbers as floats. The game's actor dictionary uses the
# bridge's integer entity IDs, including the six entries from the initial state.
func native_integers(value: Variant) -> Variant:
	if value is float:
		return int(value)
	if value is Array or value is Dictionary:
		for key in (value.keys() if value is Dictionary else range(value.size())):
			value[key] = native_integers(value[key])
	return value


func run() -> void:
	var folder := OS.get_environment("VOIDFRONT_REPLAY_SAMPLES")
	var plan: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(folder.path_join("plan.json")))
	native_integers(plan)
	var frames := []
	for pair in plan.frames:
		var game = load("res://main.tscn").instantiate()
		root.add_child(game)
		game.set_process(false)
		game.setup_open = false
		game.setup_ai = false
		game.setup_seed = int(plan.seed)
		game.local_player = int(plan.player)
		game._reset()
		if game.current.protocol != plan.protocol or game.current.content_id != plan.content_id:
			push_error("Native replay and project use different simulation content")
			quit(2)
			return
		var tick := int(pair[0])
		var id := int(pair[1])
		for t in [tick - 1, tick]:
			var snapshot: Dictionary = plan.snapshots[str(t)]
			snapshot.vision = [PackedByteArray(snapshot.vision[0]), PackedByteArray(snapshot.vision[1])]
			game.previous = game.current.duplicate()
			game.current = game.current.merged(snapshot, true)
			if t == tick - 1:
				game.previous = game.current
			# A freshly produced worker has no position in the preceding state.
			var unit: Dictionary = plan.snapshots[str(tick)].units[id - 1]
			if id <= game.current.units.size():
				unit = game.current.units[id - 1]
			game.camera_target = Vector3(unit.x / 256.0, 0, unit.z / 256.0)
			game._update_camera()
			game.selected.clear()
			if id <= game.current.units.size() and unit.hp > 0:
				game.selected.append(id)
			game._update_fog()
			game._present(1.0, 1.0 / 20.0)
			game._present_economy()
			if t == tick - 1:
				# Initializing a scene at a replay prefix must not leave a spurious
				# attack beam from comparing a late cooldown to the opening state.
				await create_timer(0.15).timeout
		# The renderer installs authoritative snapshots into a paused scene.
		# Its local setup does not describe the players in the source recording.
		game.hud._process(0.0)
		game.hud.set_process(false)
		game.hud.connection.text = "REPLAY SAMPLE   /   TICK %d   /   PLAYER %d" % [tick, game.local_player + 1]
		await process_frame
		await RenderingServer.frame_post_draw
		var shot := root.get_texture().get_image()
		if shot.save_png(folder.path_join("tick-%d-unit-%d.png" % [tick, id])) != OK:
			quit(2)
			return
		var visible_enemies := []
		for other in game.current.units:
			if other.player != game.local_player and other.hp > 0 and game.actors[other.id].root.visible:
				visible_enemies.append(int(other.id))
		var position: Vector3 = game.actors[id].root.position
		var card = game.hud.command_card
		frames.append({"tick":tick, "actor":id, "hash":game.current.hash, "visible_enemies":visible_enemies,
			"caption":game.hud.connection.text,
			"position":[roundi(position.x * 256.0), roundi(position.z * 256.0)],
			"selected":id in game.selected, "hp":game.current.units[id - 1].hp,
			"card_heading":card.heading.text if card.is_visible_in_tree() else ""})
		game.queue_free()
		await process_frame
	var report := FileAccess.open(folder.path_join("frames.json"), FileAccess.WRITE)
	if not report:
		quit(2)
		return
	report.store_string(JSON.stringify({"frames":frames, "renderer":RenderingServer.get_video_adapter_name()}, "  "))
	report.close()
	quit(0)

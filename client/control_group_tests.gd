extends RefCounted
# Real game instance and dispatched keyboard events; presentation tests only.
var game
var checks: Array[String] = []
var errors: Array[String] = []

func _init(owner) -> void:
	game = owner

func check(ok: bool, label: String) -> void:
	if ok: checks.append(label)
	else: errors.append(label)
	print("VOIDFRONT_CONTROL_CHECK tick=", game.current.tick, " ok=", ok, " ", label)

func key(code: Key, control := false, shift := false, echo := false) -> void:
	var event := InputEventKey.new()
	event.physical_keycode = code
	event.pressed = true
	event.ctrl_pressed = control
	event.shift_pressed = shift
	event.echo = echo
	Input.parse_input_event(event)
	var release := InputEventKey.new()
	release.physical_keycode = code
	Input.parse_input_event(release)
	await game.get_tree().process_frame
	await game.get_tree().process_frame

func run() -> void:
	await game.get_tree().process_frame
	game.selected.assign([1])
	await key(KEY_1, true)
	check(game.control_groups.get(1, []) == [1], "Ctrl1 saves selection")
	game.selected.assign([2])
	await key(KEY_2, true)
	await key(KEY_1, false, true)
	check(game.selected == [1, 2], "Shift1 adds saved group")
	await key(KEY_1, true, true)
	check(game.control_groups.get(1, []) == [1, 2], "CtrlShift1 merges without duplicates")
	await key(KEY_0, true)
	await key(KEY_2)
	check(game.selected == [2], "2 recalls a single group")
	await key(KEY_1, true)
	check(game.control_groups.get(1, []) == [2], "Ctrl1 replaces prior membership")
	await key(KEY_0)
	check(game.selected == [1, 2], "0 recalls the tenth group")
	await key(KEY_9)
	check(game.selected == [1, 2], "Empty group preserves selection")
	await key(KEY_9, true)
	game.selected.clear()
	await key(KEY_9, true, false, true)
	check(game.control_groups.get(9, []) == [1, 2], "Key echo does not change saved group")
	await key(KEY_9, true)
	check(not game.control_groups.has(9), "Saving empty selection clears group")
	game.control_groups[3] = [1, 1, 7, 999]
	await key(KEY_3)
	check(game.selected == [1] and game.control_groups[3] == [1], "Recall filters foreign missing and duplicate IDs")
	game.selected.assign([1, 2])
	var center: Vector3 = (game.actors[1].root.position + game.actors[2].root.position) / 2.0
	await key(KEY_S)
	check(game.order_mark.position.distance_to(Vector3(center.x, 0.05, center.z)) < 0.001, "Stop marker uses selected actor center")
	await key(KEY_H)
	check(game.order_mark.position.distance_to(Vector3(center.x, 0.05, center.z)) < 0.001, "Hold marker uses selected actor center")
	if not game.capture_path.is_empty():
		await RenderingServer.frame_post_draw
		check(game.get_viewport().get_texture().get_image().save_png(game.capture_path) == OK, "Control feedback screenshot saved")
	await key(KEY_F2)
	await key(KEY_0, true)
	var saved: Array = game.control_groups.get(0, []).duplicate()
	var casualty := false
	while game.current.tick < 700 and not casualty:
		await game.get_tree().process_frame
		for unit in game.current.units:
			if unit.id in saved and unit.hp <= 0: casualty = true
	check(casualty, "Combat produced an actual saved-group casualty")
	var casualty_tick: int = game.current.tick
	await key(KEY_0)
	var survivors: Array[int] = []
	var dead_ids: Array[int] = []
	for unit in game.current.units:
		if unit.id in saved:
			if unit.hp > 0 and unit.player == game.local_player: survivors.append(int(unit.id))
			elif unit.hp <= 0: dead_ids.append(int(unit.id))
	survivors.sort()
	check(game.selected == survivors and game.control_groups.get(0, []) == survivors, "Recall prunes actual dead members")
	for id in dead_ids: check(id not in game.selected, "Dead member absent from selection: %d" % id)
	game._reset()
	check(game.control_groups.is_empty() and game.current.tick == 0, "Restart clears control groups")
	var report := {"ok": errors.is_empty(), "checks": checks, "errors": errors, "casualty_tick": casualty_tick,
		"note": "Godot game instance with real InputEvents and combat casualty. Automated presentation checks; no human responsiveness acceptance."}
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--report="):
			var file := FileAccess.open(argument.trim_prefix("--report="), FileAccess.WRITE)
			if file: file.store_string(JSON.stringify(report, "\t"))
			else: errors.append("Cannot write test report")
	print("VOIDFRONT_CONTROL_GROUPS ", JSON.stringify(report))
	game.get_tree().quit(0 if errors.is_empty() else 1)

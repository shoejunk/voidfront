extends Node3D

const STEP := 1.0 / 20.0
const SCALE := 256.0
const WALKER = preload("res://assets/cairn_walker.glb")
const HUD = preload("res://hud.gd")
var bridge: RefCounted
var camera: Camera3D
var hud: Control
var current: Dictionary = {}
var previous: Dictionary = {}
var actors: Dictionary = {}
var selected: Array[int] = []
var obstacle_cells: Array[Vector2i] = []
var accumulator := 0.0
var max_hp := 100.0
var attack_pending := false
var drag_start := Vector2.ZERO
var camera_target := Vector3(16, 0, 12)
var order_mark: MeshInstance3D
var order_age := 99.0
var smoke := false
var smoke_stage := 0
var smoke_moves := 0
var smoke_damage := false
var capture_path := ""
var report_path := ""
var finish_tick := 400
var frame_times: Array[float] = []
var sim_times: Array[float] = []
var initial_hash := ""
var initial_positions: Dictionary = {}
var completed := false
var accepted_orders: Array[int] = []
var observed_clips: Array[String] = []
var stop_positions: Dictionary = {}
var stopped_without_drift := true
var click_selection_passed := false
var drag_selection_passed := false
var early_capture_done := false
var material_cache: Dictionary = {}
var team_material_cache: Dictionary = {}
var last_frame_usec := 0
var network := false
var network_smoke := false
var local_player := 0
var local_port := 39000
var remote_port := 39001
var session_id := 1
var input_delay := 2
var network_state: Dictionary = {}
var option_error := ""
var network_notice := ""
var network_inputs: Array[Dictionary] = []
var feedback_samples: Array[Dictionary] = []
var rendered_snapshots: Array[Dictionary] = []
var presented_tick := -1
var event_usec := 0
var network_finish_requested := false
var status_history: Array[Dictionary] = []
var own_selection_passed := false
var pending_execution_display: Array[Dictionary] = []
var execution_display_samples: Array[Dictionary] = []

func _ready() -> void:
	var ticks_specified := false
	for argument in OS.get_cmdline_user_args():
		if argument == "--smoke": smoke = true
		elif argument == "--network": network = true
		elif argument == "--network-smoke":
			network = true
			network_smoke = true
		elif argument.begins_with("--capture="): capture_path = argument.trim_prefix("--capture=")
		elif argument.begins_with("--report="): report_path = argument.trim_prefix("--report=")
		elif argument.begins_with("--ticks="):
			finish_tick = _integer_option(argument, 1, 10000000)
			ticks_specified = true
		elif argument.begins_with("--player="): local_player = _integer_option(argument, 0, 1)
		elif argument.begins_with("--port="): local_port = _integer_option(argument, 1, 65535)
		elif argument.begins_with("--remote-port="): remote_port = _integer_option(argument, 1, 65535)
		elif argument.begins_with("--session="): session_id = _integer_option(argument, 1, 2147483647)
		elif argument.begins_with("--delay="): input_delay = _integer_option(argument, 1, 16)
		else: option_error = "Unknown option: " + argument
	if network and not ticks_specified: finish_tick = 400 if network_smoke else 36000
	if network and local_port == remote_port: option_error = "Local and remote ports must differ."
	if network and smoke: option_error = "Choose --smoke or --network-smoke."
	if network_smoke and finish_tick < 80: option_error = "Network smoke needs at least 80 ticks."
	bridge = ClassDB.instantiate("VoidfrontBridge")
	if bridge == null:
		push_error("Required C++ simulation extension failed to load")
		get_tree().quit(2)
		return
	_setup_world()
	var canvas := CanvasLayer.new()
	add_child(canvas)
	hud = HUD.new()
	hud.game = self
	canvas.add_child(hud)
	_reset()
	print("VOIDFRONT_RUNTIME extension=ready renderer=", RenderingServer.get_video_adapter_name())
	if not option_error.is_empty():
		push_error(option_error)
		if smoke or network_smoke:
			completed = true
			_finish_network_smoke.call_deferred()

func _integer_option(argument: String, minimum: int, maximum: int) -> int:
	var value := argument.get_slice("=", 1)
	if not value.is_valid_int() or int(value) < minimum or int(value) > maximum:
		option_error = "Invalid option %s (expected %d..%d)" % [argument, minimum, maximum]
		return minimum
	return int(value)

func _material(color: Color, metallic: float = 0.0, emission: bool = false) -> StandardMaterial3D:
	var key := "%s/%s/%s" % [color.to_html(), metallic, emission]
	if material_cache.has(key): return material_cache[key]
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = 0.8
	material.metallic = metallic
	if emission:
		material.emission_enabled = true
		material.emission = color
		material.emission_energy_multiplier = 1.4
	material_cache[key] = material
	return material

func _box(size: Vector3, at: Vector3, material: Material) -> MeshInstance3D:
	var instance := MeshInstance3D.new()
	var mesh := BoxMesh.new()
	mesh.size = size
	instance.mesh = mesh
	instance.material_override = material
	instance.position = at
	add_child(instance)
	return instance

func _ring(radius: float, color: Color) -> MeshInstance3D:
	var instance := MeshInstance3D.new()
	var mesh := TorusMesh.new()
	mesh.inner_radius = radius - 0.025
	mesh.outer_radius = radius + 0.025
	mesh.rings = 24
	mesh.ring_segments = 6
	instance.mesh = mesh
	instance.material_override = _material(color, 0, true)
	return instance

func _setup_world() -> void:
	var environment := WorldEnvironment.new()
	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color("17272f")
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color("a3c0c6")
	env.ambient_light_energy = 0.32
	env.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	environment.environment = env
	add_child(environment)
	var sun := DirectionalLight3D.new()
	sun.rotation_degrees = Vector3(-54, -28, 0)
	sun.light_color = Color("ffdcad")
	sun.light_energy = 0.95
	sun.shadow_enabled = true
	sun.directional_shadow_max_distance = 80
	add_child(sun)
	var fill := DirectionalLight3D.new()
	fill.rotation_degrees = Vector3(-28, 130, 0)
	fill.light_color = Color("7cbed7")
	fill.light_energy = 0.22
	add_child(fill)
	camera = Camera3D.new()
	camera.projection = Camera3D.PROJECTION_ORTHOGONAL
	camera.size = 27
	camera.far = 150
	add_child(camera)
	_update_camera()
	var floor_material := _material(Color("29383f"), 0.12)
	_box(Vector3(32, 0.45, 24), Vector3(16, -0.3, 12), floor_material)
	var grid_material := _material(Color("334249"))
	for x in range(0, 33, 2):
		_box(Vector3(0.018, 0.012, 24), Vector3(x, -0.067, 12), grid_material)
	for z in range(0, 25, 2):
		_box(Vector3(32, 0.012, 0.018), Vector3(16, -0.067, z), grid_material)
	var border_material := _material(Color("253b43"), 0.4)
	_box(Vector3(34, 0.6, 0.5), Vector3(16, -0.15, -0.4), border_material)
	_box(Vector3(34, 0.6, 0.5), Vector3(16, -0.15, 24.4), border_material)
	_box(Vector3(0.5, 0.6, 24), Vector3(-0.4, -0.15, 12), border_material)
	_box(Vector3(0.5, 0.6, 24), Vector3(32.4, -0.15, 12), border_material)
	var obstacle_material := _material(Color("263b44"), 0.25)
	for x in range(32):
		for z in range(24):
			if bridge.is_blocked(x, z):
				obstacle_cells.append(Vector2i(x, z))
				var height := 0.75 + float((x * 7 + z * 3) % 5) * 0.12
				_box(Vector3(0.96, height, 0.96), Vector3(x + 0.5, height / 2, z + 0.5), obstacle_material)
				if (x + z) % 3 == 0:
					_box(Vector3(0.64, 0.02, 0.04), Vector3(x + 0.5, height + 0.02, z + 0.5), _material(Color("769c9c"), 0.2, true))
	for player in range(2):
		var pad_color := Color("4d9caa") if player == 0 else Color("c78960")
		var pad := _ring(3.5, pad_color)
		pad.position = Vector3(5 if player == 0 else 27, -0.04, 12)
		pad.material_override = _material(pad_color * 0.42, 0.25)
		add_child(pad)
		var lettering := Label3D.new()
		lettering.text = "CAIRN  /  01" if player == 0 else "CAIRN  /  02"
		lettering.font_size = 70
		lettering.pixel_size = 0.008
		lettering.modulate = pad_color
		lettering.rotation_degrees.x = -90
		lettering.position = Vector3(5 if player == 0 else 27, 0.005, 16.5)
		add_child(lettering)
	order_mark = _ring(0.55, Color("a3ffea"))
	order_mark.visible = false
	add_child(order_mark)

func _reset() -> void:
	# A peer must never reset the authoritative shared match unilaterally.
	if network and not current.is_empty():
		if option_error.is_empty() and str(network_state.get("state", "")) not in ["complete", "error"]:
			network_notice = "Shared match active. Restart is available after it ends."
			return
		bridge.network_cancel()
		network = false
		local_player = 0
		network_notice = ""
		option_error = ""
	elif not current.is_empty() and not option_error.is_empty():
		option_error = ""
	for entry in actors.values(): entry.root.queue_free()
	actors.clear()
	selected.clear()
	bridge.reset(1, true)
	if network and option_error.is_empty():
		if not bridge.network_start(local_player, local_port, remote_port, session_id, input_delay, finish_tick):
			network_notice = "Network session could not start."
		_update_network_status()
	current = bridge.snapshot()
	previous = current.duplicate(true)
	initial_hash = current.hash
	accumulator = 0
	attack_pending = false
	initial_positions.clear()
	for unit in current.units:
		max_hp = maxf(max_hp, unit.hp)
		_spawn_actor(unit)
		initial_positions[unit.id] = Vector2(unit.x, unit.z)
	print("VOIDFRONT_MATCH tick=0 hash=", initial_hash, " units=", actors.size())

func _spawn_actor(unit: Dictionary) -> void:
	var root := Node3D.new()
	add_child(root)
	root.position = Vector3(unit.x / SCALE, 0, unit.z / SCALE)
	var model: Node3D = WALKER.instantiate()
	root.add_child(model)
	var color := Color("62d7d1") if unit.player == 0 else Color("ef9259")
	_apply_team(model, color)
	var ring := _ring(0.62, color)
	ring.position.y = 0.035
	root.add_child(ring)
	ring.visible = false
	var animation: AnimationPlayer = _find_animation(model)
	actors[unit.id] = {"root": root, "model": model, "ring": ring, "animation": animation, "clip": "", "dead": false, "hp": unit.hp, "cooldown": 0, "flash": 0.0}
	if animation:
		print("VOIDFRONT_ASSET id=", unit.id, " clips=", animation.get_animation_list())

func _find_animation(node: Node) -> AnimationPlayer:
	if node is AnimationPlayer: return node
	for child in node.get_children():
		var found := _find_animation(child)
		if found: return found
	return null

func _apply_team(node: Node, color: Color) -> void:
	if node is MeshInstance3D:
		for surface in range(node.mesh.get_surface_count()):
			var source: Material = node.mesh.surface_get_material(surface)
			if source and "team" in source.resource_name.to_lower():
				var key := color.to_html()
				if not team_material_cache.has(key):
					var material: StandardMaterial3D = source.duplicate()
					material.albedo_color = color * 0.55
					material.emission_enabled = true
					material.emission = color * 0.035
					team_material_cache[key] = material
				node.set_surface_override_material(surface, team_material_cache[key])
	for child in node.get_children(): _apply_team(child, color)

func _play(entry: Dictionary, desired: String) -> void:
	var animation: AnimationPlayer = entry.animation
	if not animation or entry.clip == desired: return
	for clip in animation.get_animation_list():
		if clip.to_lower().ends_with(desired):
			var resource := animation.get_animation(clip)
			resource.loop_mode = Animation.LOOP_LINEAR if desired in ["idle", "walk"] else Animation.LOOP_NONE
			animation.play(clip, 0.12)
			entry.clip = desired
			if desired not in observed_clips: observed_clips.append(desired)
			return

func _subdue_corpse(node: Node) -> void:
	if node is MeshInstance3D:
		node.material_override = _material(Color("33454e"), 0.2)
	for child in node.get_children(): _subdue_corpse(child)

func _process(delta: float) -> void:
	if current.is_empty() or completed: return
	if not option_error.is_empty(): return
	if smoke or network_smoke:
		var now := Time.get_ticks_usec()
		if last_frame_usec != 0: frame_times.append((now - last_frame_usec) / 1000.0)
		last_frame_usec = now
	if network:
		var start := Time.get_ticks_usec()
		var advanced: bool = bridge.network_poll(start)
		if network_smoke: sim_times.append((Time.get_ticks_usec() - start) / 1000.0)
		_update_network_status()
		if advanced:
			previous = current
			current = bridge.snapshot()
			accumulator = 0
			if network_smoke:
				for execution in network_state.get("executed_inputs", []): pending_execution_display.append(execution)
		else: accumulator = minf(accumulator + delta, STEP)
		if network_smoke and network_state.get("ready", false) and str(network_state.get("state", "")) in ["running", "stalled"]:
			_network_smoke_tick()
	else:
		accumulator += delta
		var ticks := 0
		while accumulator >= STEP and ticks < 8:
			previous = current
			var start := Time.get_ticks_usec()
			bridge.advance()
			if smoke: sim_times.append((Time.get_ticks_usec() - start) / 1000.0)
			current = bridge.snapshot()
			accumulator -= STEP
			ticks += 1
			if smoke: _smoke_tick()
	_present(clampf(accumulator / STEP, 0, 1), delta)
	# The first positive interpolation includes the new authoritative state.
	if network_smoke and accumulator > 0 and current.tick != presented_tick:
		presented_tick = current.tick
		_record_network_frame.call_deferred(int(current.tick))
	var camera_axis := Vector2.ZERO
	if Input.is_physical_key_pressed(KEY_UP): camera_axis.y -= 1
	if Input.is_physical_key_pressed(KEY_DOWN): camera_axis.y += 1
	if Input.is_physical_key_pressed(KEY_RIGHT): camera_axis.x += 1
	if Input.is_physical_key_pressed(KEY_LEFT): camera_axis.x -= 1
	camera_target += Vector3(camera_axis.x, 0, camera_axis.y) * delta * 13
	camera_target.x = clampf(camera_target.x, 2, 30)
	camera_target.z = clampf(camera_target.z, 2, 22)
	_update_camera()
	order_age += delta
	order_mark.visible = order_age < 1.2
	order_mark.scale = Vector3.ONE * (1.0 + minf(order_age, 1.2) * 0.5)
	if smoke and current.tick >= finish_tick and not completed:
		completed = true
		_finish_smoke.call_deferred()
	if network_smoke and str(network_state.get("state", "")) in ["complete", "error"] and not network_finish_requested:
		network_finish_requested = true
		_finish_network_smoke.call_deferred()

func _update_network_status() -> void:
	network_state = bridge.network_status()
	var state := str(network_state.get("state", "error"))
	if status_history.is_empty() or status_history[-1].state != state:
		status_history.append({"state": state, "tick": network_state.get("tick", 0), "usec": Time.get_ticks_usec(), "ready": network_state.get("ready", false), "error": network_state.get("error", "")})
		if not network_smoke and status_history.size() > 64: status_history.pop_front()
		print("VOIDFRONT_NETWORK player=", local_player, " state=", state, " tick=", network_state.get("tick", 0), " error=", network_state.get("error", ""))

func _record_network_frame(tick: int) -> void:
	await RenderingServer.frame_post_draw
	var now := Time.get_ticks_usec()
	rendered_snapshots.append({"tick": tick, "rendered_usec": now})
	var remaining: Array[Dictionary] = []
	for execution in pending_execution_display:
		if int(execution.execution_tick) < tick:
			var sample: Dictionary = execution.duplicate()
			sample["displayed_usec"] = now
			sample["display_tick"] = tick
			execution_display_samples.append(sample)
		else: remaining.append(execution)
	pending_execution_display = remaining

func _present(alpha: float, delta: float) -> void:
	var old_units := {}
	for unit in previous.units: old_units[unit.id] = unit
	for unit in current.units:
		if not actors.has(unit.id): _spawn_actor(unit)
		var entry: Dictionary = actors[unit.id]
		var old: Dictionary = old_units.get(unit.id, unit)
		var from := Vector3(old.x / SCALE, 0, old.z / SCALE)
		var to := Vector3(unit.x / SCALE, 0, unit.z / SCALE)
		entry.root.position = from.lerp(to, alpha)
		entry.ring.visible = unit.id in selected and unit.hp > 0
		if unit.hp <= 0:
			if not entry.dead:
				_play(entry, "death")
				entry.dead = true
				_subdue_corpse(entry.model)
				selected.erase(unit.id)
			continue
		var direction := to - from
		if unit.target != 0 and actors.has(unit.target):
			direction = actors[unit.target].root.position - entry.root.position
		if direction.length_squared() > 0.0001:
			entry.root.rotation.y = lerp_angle(entry.root.rotation.y, atan2(-direction.x, -direction.z), minf(delta * 12, 1))
		if unit.cooldown > entry.cooldown and unit.target != 0 and actors.has(unit.target):
			_beam(entry.root.position, actors[unit.target].root.position, unit.player)
			entry.clip = ""
			_play(entry, "attack")
			entry.flash = 0.25
		elif unit.hp < entry.hp:
			entry.clip = ""
			_play(entry, "hit")
			entry.flash = 0.15
		elif entry.flash <= 0:
			_play(entry, "walk" if unit.moving else "idle")
		entry.flash -= delta
		entry.hp = unit.hp
		entry.cooldown = unit.cooldown

func _beam(from: Vector3, to: Vector3, player: int) -> void:
	var offset := Vector3(0, 0.8, 0)
	var vector := to - from
	if vector.length() < 0.001: return
	var beam := _box(Vector3(0.035, 0.035, vector.length()), (from + to) / 2 + offset, _material(Color("aaffec") if player == 0 else Color("ffbf7b"), 0, true))
	beam.look_at(to + offset, Vector3.UP)
	var tween := create_tween()
	tween.tween_interval(0.075)
	tween.tween_callback(beam.queue_free)

func _update_camera() -> void:
	camera.position = camera_target + Vector3(0, 25, 21)
	camera.look_at(camera_target)

func _world_at(point: Vector2) -> Vector3:
	var origin := camera.project_ray_origin(point)
	var direction := camera.project_ray_normal(point)
	if absf(direction.y) < 0.001: return Vector3(-1, 0, -1)
	return origin + direction * (-origin.y / direction.y)

func _input(event: InputEvent) -> void:
	event.set_meta("voidfront_input_usec", Time.get_ticks_usec())

func _unhandled_input(event: InputEvent) -> void:
	event_usec = int(event.get_meta("voidfront_input_usec", Time.get_ticks_usec()))
	if current.is_empty(): return
	if event is InputEventKey and event.pressed and event.physical_keycode == KEY_R:
		_reset()
		return
	if not option_error.is_empty(): return
	if network and (not network_state.get("ready", false) or str(network_state.get("state", "")) not in ["running", "stalled"]): return
	if event is InputEventKey and event.pressed and not event.echo:
		match event.physical_keycode:
			KEY_F2:
				selected.clear()
				for unit in current.units:
					if unit.player == local_player and unit.hp > 0: selected.append(unit.id)
				if network_smoke:
					own_selection_passed = not selected.is_empty()
					for unit in current.units:
						if unit.id in selected and unit.player != local_player: own_selection_passed = false
			KEY_A: attack_pending = true
			KEY_ESCAPE: attack_pending = false
			KEY_S: _issue(0, Vector3(0, 0, 0))
			KEY_H: _issue(3, Vector3(0, 0, 0))
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP and event.pressed: camera.size = maxf(14, camera.size - 1.5)
		if event.button_index == MOUSE_BUTTON_WHEEL_DOWN and event.pressed: camera.size = minf(36, camera.size + 1.5)
		if event.button_index == MOUSE_BUTTON_RIGHT and event.pressed:
			_issue(1, _world_at(event.position))
			attack_pending = false
		if event.button_index == MOUSE_BUTTON_LEFT:
			if attack_pending and event.pressed:
				_issue(2, _world_at(event.position))
				attack_pending = false
				return
			if event.pressed:
				drag_start = event.position
				hud.drag_from = drag_start
				hud.drag_to = drag_start
				hud.dragging = true
			elif hud.dragging:
				hud.dragging = false
				_select(drag_start, event.position, event.shift_pressed)
	if event is InputEventMouseMotion and hud.dragging: hud.drag_to = event.position

func _select(from: Vector2, to: Vector2, additive: bool) -> void:
	if not additive: selected.clear()
	var bounds := Rect2(from, to - from).abs()
	var nearest := -1
	var distance := 32.0
	for unit in current.units:
		if unit.player != local_player or unit.hp <= 0: continue
		var at: Vector2 = camera.unproject_position(actors[unit.id].root.position + Vector3(0, 0.5, 0))
		if from.distance_to(to) > 7:
			if bounds.has_point(at) and unit.id not in selected: selected.append(unit.id)
		elif at.distance_to(to) < distance:
			distance = at.distance_to(to)
			nearest = unit.id
	if nearest != -1:
		if additive and nearest in selected: selected.erase(nearest)
		else: selected.append(nearest)
	if smoke:
		# Assert after Godot dispatches the event, not from a tick that may precede dispatch.
		if from.distance_to(to) <= 7 and selected.size() == 1: click_selection_passed = true
		if from.distance_to(to) > 7 and selected.size() == 6: drag_selection_passed = true
		print("VOIDFRONT_SELECTION from=", from, " to=", to, " ids=", selected)

func _issue(order: int, at: Vector3) -> void:
	if selected.is_empty(): return
	if at.x < 0 or at.x >= 32 or at.z < 0 or at.z >= 24: return
	var accepted := false
	var sequence := -1
	if network:
		if not network_state.get("ready", false): return
		sequence = bridge.network_issue(order, PackedInt32Array(selected), int(at.x * SCALE), int(at.z * SCALE), event_usec)
		accepted = sequence >= 0
		if accepted and network_smoke:
			network_inputs.append({"sequence": sequence, "input_usec": event_usec, "order": order, "units": selected.duplicate(), "x": int(at.x * SCALE), "z": int(at.z * SCALE), "event_tick": current.tick})
		elif not accepted: network_notice = "Order rejected: " + str(bridge.network_status().get("error", "session unavailable"))
	else:
		accepted = bridge.issue(order, PackedInt32Array(selected), int(at.x * SCALE), int(at.z * SCALE))
	if accepted:
		if order not in accepted_orders: accepted_orders.append(order)
		order_mark.position = Vector3(at.x, 0.05, at.z)
		order_age = 0
		order_mark.visible = true
		if network:
			network_notice = ""
			if network_smoke: _record_feedback.call_deferred(sequence, event_usec)
		if smoke: print("VOIDFRONT_INPUT order=", order, " count=", selected.size(), " tick=", current.tick)

func _record_feedback(sequence: int, input_usec: int) -> void:
	await RenderingServer.frame_post_draw
	feedback_samples.append({"sequence": sequence, "input_usec": input_usec, "feedback_rendered_usec": Time.get_ticks_usec()})

func _network_smoke_tick() -> void:
	if current.tick >= 3 and smoke_stage == 0:
		_smoke_key(KEY_F2)
		smoke_stage = 1
	elif current.tick >= 5 and smoke_stage == 1:
		_smoke_mouse(MOUSE_BUTTON_RIGHT, camera.unproject_position(Vector3(11 if local_player == 0 else 21, 0, 12)), true)
		smoke_stage = 2
	elif current.tick >= 45 and smoke_stage == 2:
		_smoke_key(KEY_S)
		smoke_stage = 3
	elif current.tick >= 55 and smoke_stage == 3:
		_smoke_key(KEY_A)
		smoke_stage = 4
	elif current.tick >= 56 and smoke_stage == 4:
		_smoke_mouse(MOUSE_BUTTON_LEFT, camera.unproject_position(Vector3(26 if local_player == 0 else 6, 0, 12)), true)
		smoke_stage = 5
	if current.tick >= 150 and not early_capture_done and not capture_path.is_empty():
		early_capture_done = true
		_capture_battle.call_deferred()
	for unit in current.units:
		if unit.player == local_player and Vector2(unit.x, unit.z).distance_to(initial_positions[unit.id]) > SCALE: smoke_moves += 1
		if unit.hp < max_hp: smoke_damage = true

func _finish_network_smoke() -> void:
	# Allow the final snapshot's interpolation and post-draw instrumentation to settle.
	await get_tree().process_frame
	await RenderingServer.frame_post_draw
	await get_tree().process_frame
	await RenderingServer.frame_post_draw
	completed = true
	var report_data: Dictionary = bridge.network_report() if network else {}
	var replay: PackedByteArray = report_data.get("replay", PackedByteArray())
	report_data.erase("replay")
	var ok: bool = option_error.is_empty() and str(report_data.get("state", "")) == "complete" and own_selection_passed and smoke_stage == 5 and network_inputs.size() == 3 and feedback_samples.size() == 3 and execution_display_samples.size() == 3 and smoke_moves > 0 and 0 in accepted_orders and 1 in accepted_orders and 2 in accepted_orders
	var report := {"ok": ok, "player": local_player, "network": report_data, "status_history": status_history, "option_error": option_error, "selection_own_player": own_selection_passed, "accepted_inputs": network_inputs, "feedback_samples": feedback_samples, "execution_display_samples": execution_display_samples, "rendered_snapshots": rendered_snapshots, "accepted_orders": accepted_orders, "input_stage": smoke_stage, "moved_samples": smoke_moves, "combat_damage": smoke_damage, "winner": current.get("winner", -1), "renderer": RenderingServer.get_video_adapter_name(), "frame_interval_ms_p95": _percentile(frame_times, 0.95), "frame_interval_ms_p99": _percentile(frame_times, 0.99), "network_poll_ms_p95": _percentile(sim_times, 0.95), "network_poll_ms_p99": _percentile(sim_times, 0.99), "note": "Programmatic InputEvent path on loopback. Post-draw measures submitted viewport rendering, not photons, human responsiveness, or a complete RTS match. Snapshot tick N first includes canonical execution tick N-1; displayed_usec includes first positive interpolation."}
	if not report_path.is_empty() and not replay.is_empty():
		var replay_path := report_path.get_basename() + ".vfr"
		var replay_file := FileAccess.open(replay_path, FileAccess.WRITE)
		if replay_file:
			replay_file.store_buffer(replay)
			replay_file.close()
			report["replay_path"] = replay_path
		else:
			ok = false
			report["replay_error"] = FileAccess.get_open_error()
	elif replay.is_empty():
		ok = false
		report["replay_error"] = "No applied-prefix replay available."
	if not capture_path.is_empty():
		var capture_error := get_viewport().get_texture().get_image().save_png(capture_path)
		if capture_error != OK:
			ok = false
			report["capture_error"] = capture_error
	report["ok"] = ok
	if not report_path.is_empty():
		var report_file := FileAccess.open(report_path, FileAccess.WRITE)
		if report_file:
			report_file.store_string(JSON.stringify(report, "\t"))
			report_file.close()
		else:
			ok = false
			report["ok"] = false
			report["report_error"] = FileAccess.get_open_error()
	print("VOIDFRONT_NETWORK_SMOKE ", JSON.stringify(report))
	get_tree().quit(0 if ok else 3)

func _smoke_key(key: Key) -> void:
	var event := InputEventKey.new()
	event.physical_keycode = key
	event.pressed = true
	Input.parse_input_event(event)
	var release := InputEventKey.new()
	release.physical_keycode = key
	Input.parse_input_event(release)

func _smoke_tick() -> void:
	if current.tick == 1:
		var first: Dictionary = current.units[0]
		var at := camera.unproject_position(actors[first.id].root.position + Vector3(0, 0.5, 0))
		_smoke_mouse(MOUSE_BUTTON_LEFT, at, true)
		_smoke_mouse(MOUSE_BUTTON_LEFT, at, false)
	if current.tick == 2:
		var bounds := Rect2()
		var first := true
		for unit in current.units:
			if unit.player != 0: continue
			var at := camera.unproject_position(actors[unit.id].root.position + Vector3(0, 0.5, 0))
			if first:
				bounds = Rect2(at, Vector2.ZERO)
				first = false
			else: bounds = bounds.expand(at)
		bounds = bounds.grow(22)
		_smoke_mouse(MOUSE_BUTTON_LEFT, bounds.position, true)
		var motion := InputEventMouseMotion.new()
		motion.position = get_viewport().get_final_transform() * bounds.end
		motion.button_mask = MOUSE_BUTTON_MASK_LEFT
		Input.parse_input_event(motion)
		_smoke_mouse(MOUSE_BUTTON_LEFT, bounds.end, false)
	if current.tick >= 150 and not early_capture_done and not capture_path.is_empty():
		early_capture_done = true
		_capture_battle.call_deferred()
	if current.tick == 46:
		for unit in current.units:
			if unit.player == 0: stop_positions[unit.id] = Vector2(unit.x, unit.z)
	if current.tick > 46 and current.tick < 55:
		for unit in current.units:
			if unit.player == 0 and stop_positions.has(unit.id) and Vector2(unit.x, unit.z) != stop_positions[unit.id]: stopped_without_drift = false
	if current.tick >= 3 and smoke_stage == 0:
		_smoke_key(KEY_F2)
		smoke_stage = 1
	elif current.tick >= 5 and smoke_stage == 1:
		_smoke_mouse(MOUSE_BUTTON_RIGHT, camera.unproject_position(Vector3(11, 0, 12)), true)
		smoke_stage = 2
	elif current.tick >= 45 and smoke_stage == 2:
		_smoke_key(KEY_S)
		smoke_stage = 3
	elif current.tick >= 55 and smoke_stage == 3:
		_smoke_key(KEY_A)
		smoke_stage = 4
	elif current.tick >= 56 and smoke_stage == 4:
		_smoke_mouse(MOUSE_BUTTON_LEFT, camera.unproject_position(Vector3(26, 0, 12)), true)
		smoke_stage = 5
	for unit in current.units:
		if unit.player == 0 and Vector2(unit.x, unit.z).distance_to(initial_positions[unit.id]) > SCALE: smoke_moves += 1
		if unit.hp < max_hp: smoke_damage = true

func _smoke_mouse(button: MouseButton, at: Vector2, pressed: bool) -> void:
	var event := InputEventMouseButton.new()
	event.button_index = button
	# parse_input_event takes window coordinates, then Godot applies stretch to the event.
	event.position = get_viewport().get_final_transform() * at
	event.pressed = pressed
	Input.parse_input_event(event)

func _capture_battle() -> void:
	await RenderingServer.frame_post_draw
	get_viewport().get_texture().get_image().save_png(capture_path.get_basename() + "-battle.png")

func _finish_smoke() -> void:
	await RenderingServer.frame_post_draw
	var all_animated := true
	var clips: Array[String] = []
	for entry in actors.values():
		if not entry.animation: all_animated = false
		else:
			for required in ["idle", "walk", "attack", "hit", "death"]:
				var found := false
				for clip in entry.animation.get_animation_list():
					if clip.to_lower().ends_with(required): found = true
				if not found: all_animated = false
			for clip in entry.animation.get_animation_list():
				if clip not in clips: clips.append(clip)
	var ok: bool = all_animated and click_selection_passed and drag_selection_passed and smoke_stage == 5 and smoke_moves > 0 and smoke_damage and stopped_without_drift and not stop_positions.is_empty() and 0 in accepted_orders and 1 in accepted_orders and 2 in accepted_orders and "walk" in observed_clips and "attack" in observed_clips
	var report := {"ok": ok, "tick": current.tick, "hash": current.hash, "input_stage": smoke_stage, "click_selection": click_selection_passed, "drag_selection": drag_selection_passed, "accepted_orders": accepted_orders, "stop_without_drift": stopped_without_drift, "requested_clips": observed_clips, "moved_samples": smoke_moves, "combat_damage": smoke_damage, "animations": clips, "winner": current.winner, "renderer": RenderingServer.get_video_adapter_name(), "frame_interval_ms_p95": _percentile(frame_times, 0.95), "frame_interval_ms_p99": _percentile(frame_times, 0.99), "sim_ms_p95": _percentile(sim_times, 0.95), "sim_ms_p99": _percentile(sim_times, 0.99), "peak_static_memory_bytes": Performance.get_monitor(Performance.MEMORY_STATIC_MAX), "note": "Programmatic input smoke. Frame intervals include vsync; static allocator is not resident memory. Clip requests need visual motion inspection. Not human responsiveness or representative battle evidence."}
	if not capture_path.is_empty():
		var error := get_viewport().get_texture().get_image().save_png(capture_path)
		if error != OK:
			ok = false
			report["capture_error"] = error
	report["ok"] = ok
	_smoke_key(KEY_R)
	await get_tree().process_frame
	await get_tree().process_frame
	var restarted: bool = current.tick == 0 and current.hash == initial_hash and current.units.size() == 12
	report["restart"] = restarted
	ok = ok and restarted
	report["ok"] = ok
	if not report_path.is_empty():
		var file := FileAccess.open(report_path, FileAccess.WRITE)
		if file: file.store_string(JSON.stringify(report, "\t"))
		else: ok = false
	print("VOIDFRONT_SMOKE ", JSON.stringify(report))
	get_tree().quit(0 if ok else 3)

func _percentile(values: Array[float], fraction: float) -> float:
	if values.is_empty(): return 0
	var ordered := values.duplicate()
	ordered.sort()
	return ordered[min(ordered.size() - 1, int(ordered.size() * fraction))]

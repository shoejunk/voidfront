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
var control_groups: Dictionary = {}
var obstacle_cells: Array[Vector2i] = []
var accumulator := 0.0
var max_hp := 100.0
var attack_pending := false
var drag_start := Vector2.ZERO
var camera_target := Vector3(16, 0, 12)
var order_mark: MeshInstance3D
var order_age := 99.0
var smoke := false
var movement_smoke := false
var crowd_smoke := false
var crowd_fixture: RefCounted
var controls_smoke := false
var controls_fixture: RefCounted
var scale128 := false
var scale_smoke := false
var scale_count := 250
var scale_fixture: RefCounted
var map_size := Vector2i(32, 24)
var movement_stage := 0
var movement_inputs: Array[Dictionary] = []
var movement_samples: Array[Dictionary] = []
var movement_captures: Array[Dictionary] = []
var movement_errors: Array[String] = []
var movement_label := ""
var movement_selected := false
var movement_direct_arrived := false
var movement_detour_arrived := false
var movement_retarget_arrived := false
var movement_ridge_crossed := false
var movement_midsegment_stop := false
var movement_live_retarget := false
var movement_stop_tick := -1
var movement_stop_at := Vector2i.ZERO
var movement_stop_samples := 0
var movement_stop_drift := false
var movement_arbitrary_steps := 0
var movement_max_step_squared := 0
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
var profile_presentation := false
var presentation_profiler: RefCounted
var economy := false
var skirmish := false
var economy_smoke := false
var economy_fixture: RefCounted
var production_smoke := false
var flux_smoke := false
var production_fixture: RefCounted
var match_smoke := false
var victory_smoke := false
var match_realtime := false
var match_fixture: RefCounted
var build_pending := false
var economy_actors: Dictionary = {}
var selected_entity: Dictionary = {}
var economy_notice := ""
var build_preview: MeshInstance3D
var economy_result_sequence := -1
var fog_texture: ImageTexture
var fog_material: ShaderMaterial
var fog_tick := -1
var camera_dragging := false
var minimap_dragging := false
var exploration_smoke := false
var exploration_fixture: RefCounted
var setup_open := false
var setup_ai := true
var setup_seed := 1

func _ready() -> void:
	var ticks_specified := false
	var profile_options := false
	var controlled_profile := false
	var profile_tick_specified := false
	var profile_camera_specified := false
	for argument in OS.get_cmdline_user_args():
		if argument == "--smoke": smoke = true
		elif argument == "--skirmish": skirmish = true
		elif argument == "--economy": economy = true
		elif argument == "--exploration-smoke":
			economy = true
			exploration_smoke = true
		elif argument == "--match-realtime": match_realtime = true
		elif argument == "--victory-smoke":
			economy = true
			match_smoke = true
			victory_smoke = true
		elif argument == "--match-smoke":
			economy = true
			match_smoke = true
		elif argument == "--production-smoke":
			economy = true
			production_smoke = true
		elif argument == "--flux-smoke":
			economy = true
			production_smoke = true
			flux_smoke = true
		elif argument == "--economy-smoke":
			economy = true
			economy_smoke = true
		elif argument == "--profile-presentation": profile_presentation = true
		elif argument.begins_with("--profile-frames="):
			profile_options = true
			_integer_option(argument, 1, 3600)
		elif argument.begins_with("--profile-warmup="):
			profile_options = true
			_integer_option(argument, 0, 1200)
		elif argument.begins_with("--profile-controlled="):
			profile_options = true
			controlled_profile = true
			if argument.trim_prefix("--profile-controlled=") not in ["pose-refresh", "pose-frozen"]:
				option_error = "Unknown controlled presentation profile."
		elif argument.begins_with("--profile-tick="):
			profile_options = true
			profile_tick_specified = true
			_integer_option(argument, 0, 1000)
		elif argument.begins_with("--profile-camera="):
			profile_options = true
			profile_camera_specified = true
			if argument.trim_prefix("--profile-camera=") not in ["standard", "overview"]:
				option_error = "Unknown profile camera."
		elif argument == "--movement-smoke": movement_smoke = true
		elif argument == "--crowd-smoke": crowd_smoke = true
		elif argument == "--controls-smoke": controls_smoke = true
		elif argument == "--scale128": scale128 = true
		elif argument == "--scale-smoke":
			scale128 = true
			scale_smoke = true
		elif argument.begins_with("--units-per-team="): scale_count = _integer_option(argument, 1, 250)
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
	var legacy_mode := skirmish or smoke or movement_smoke or crowd_smoke or controls_smoke or scale128 or profile_presentation or network
	if economy and legacy_mode: option_error = "Economy requires its own offline match."
	if not legacy_mode: economy = true
	if match_realtime and not match_smoke: option_error = "--match-realtime requires --match-smoke."
	if exploration_smoke and not ticks_specified: finish_tick = 10000
	if exploration_smoke and (match_smoke or economy_smoke or production_smoke): option_error = "Exploration fixture requires its own match."
	if match_smoke and not ticks_specified: finish_tick = 12000
	if match_smoke and (production_smoke or economy_smoke or finish_tick < 2000 or finish_tick > 12000): option_error = "Match smoke requires its own 2000..12000 tick fixture."
	if production_smoke and not ticks_specified: finish_tick = 2400
	if production_smoke and (economy_smoke or finish_tick < 300 or finish_tick > 6000): option_error = "Production smoke requires its own 300..6000 tick fixture."
	if economy_smoke and not ticks_specified: finish_tick = 1600
	if economy_smoke and (finish_tick < 300 or finish_tick > 2400): option_error = "Economy smoke requires 300..2400 ticks."
	if economy and not option_error.is_empty():
		push_error(option_error)
		get_tree().quit(2)
		return
	if movement_smoke and not ticks_specified: finish_tick = 400
	if movement_smoke and (smoke or network): option_error = "Movement smoke requires its own offline fixture."
	if movement_smoke and finish_tick > 600: option_error = "Movement smoke is bounded to 600 ticks."
	if crowd_smoke and (smoke or movement_smoke or network or finish_tick > 600): option_error = "Crowd smoke requires its own offline fixture of at most 600 ticks."
	if controls_smoke and (smoke or movement_smoke or crowd_smoke or network): option_error = "Controls smoke requires its own offline fixture."
	if network and not ticks_specified: finish_tick = 400 if network_smoke else 36000
	if network and local_port == remote_port: option_error = "Local and remote ports must differ."
	if network and smoke: option_error = "Choose --smoke or --network-smoke."
	if network_smoke and finish_tick < 80: option_error = "Network smoke needs at least 80 ticks."
	if scale128 and (network or smoke or movement_smoke or crowd_smoke or controls_smoke): option_error = "Scale mode requires its own offline match."
	if scale_smoke and not ticks_specified: finish_tick = 700
	if scale_smoke and (finish_tick < 200 or finish_tick > 1000): option_error = "Scale smoke requires 200..1000 ticks."
	if profile_presentation and (smoke or movement_smoke or crowd_smoke or controls_smoke or scale_smoke or network): option_error = "Presentation profiling requires ordinary offline play."
	if profile_options and not profile_presentation: option_error = "Profile frame options require --profile-presentation."
	if profile_tick_specified and not controlled_profile: option_error = "Profile tick requires --profile-controlled."
	if profile_camera_specified and not controlled_profile: option_error = "Profile camera requires --profile-controlled."
	if profile_presentation and report_path.is_empty(): option_error = "Presentation profiling requires --report=<path>."
	if (profile_presentation or profile_options) and not option_error.is_empty():
		push_error(option_error)
		get_tree().quit(2)
		return
	bridge = ClassDB.instantiate("VoidfrontBridge")
	if bridge == null:
		push_error("Required C++ simulation extension failed to load")
		get_tree().quit(2)
		return
	if economy: bridge.reset_economy(setup_seed, setup_ai and not (economy_smoke or production_smoke))
	if scale128:
		if not option_error.is_empty() or not bridge.reset_scale(1, scale_count, not scale_smoke):
			push_error(option_error if not option_error.is_empty() else "Scale setup rejected")
			get_tree().quit(2)
			return
	var setup: Dictionary = bridge.snapshot()
	map_size = Vector2i(setup.width, setup.height)
	camera_target = Vector3(map_size.x / 2.0, 0, map_size.y / 2.0)
	if scale128 and not scale_smoke: camera_target = Vector3(24, 0, 64)
	if economy: camera_target = Vector3(10, 0, 12)
	_setup_world()
	var canvas := CanvasLayer.new()
	add_child(canvas)
	hud = HUD.new()
	hud.game = self
	canvas.add_child(hud)
	_reset()
	# Only bare interactive launches show the skirmish setup; every fixture starts directly.
	setup_open = economy and not (economy_smoke or production_smoke or match_smoke or exploration_smoke or network)
	if exploration_smoke:
		exploration_fixture = preload("res://exploration_smoke.gd").new(self)
		exploration_fixture.run.call_deferred()
	if match_smoke:
		match_fixture = preload("res://victory_smoke.gd").new(self) if victory_smoke else preload("res://match_smoke.gd").new(self)
		match_fixture.run.call_deferred()
	if production_smoke:
		production_fixture = (preload("res://flux_smoke.gd") if flux_smoke else preload("res://production_smoke.gd")).new(self)
		production_fixture.run.call_deferred()
	if economy_smoke:
		economy_fixture = preload("res://economy_smoke.gd").new(self)
		economy_fixture.run.call_deferred()
	if profile_presentation and option_error.is_empty():
		presentation_profiler = preload("res://presentation_profile.gd").new(self)
		presentation_profiler.start()
	if scale_smoke:
		scale_fixture = preload("res://scale_smoke.gd").new(self)
		scale_fixture.run.call_deferred()
	if crowd_smoke: crowd_fixture = preload("res://crowd_smoke.gd").new(self)
	if controls_smoke and option_error.is_empty():
		controls_fixture = preload("res://control_group_tests.gd").new(self)
		controls_fixture.run.call_deferred()
	print("VOIDFRONT_RUNTIME extension=ready renderer=", RenderingServer.get_video_adapter_name())
	if not option_error.is_empty():
		push_error(option_error)
		if controls_smoke:
			get_tree().quit(2)
		elif crowd_smoke:
			completed = true
			crowd_fixture.finish.call_deferred()
		elif movement_smoke:
			completed = true
			_finish_movement_smoke.call_deferred()
		elif smoke or network_smoke:
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

func _queue_static_box(groups: Dictionary, size: Vector3, at: Vector3, material: Material) -> void:
	# Bound instance groups spatially so terrain can still be culled in chunks.
	# Keep the original mesh dimensions: translation-only instances preserve its
	# vertex positions, normals and UVs, including the thin grid and ridge strips.
	var chunk := Vector2i(floori(at.x / 16.0), floori(at.z / 16.0))
	var key := [size, material.get_instance_id(), chunk]
	if not groups.has(key): groups[key] = {"size": size, "material": material, "positions": []}
	groups[key].positions.append(at)

func _build_static_boxes(groups: Dictionary) -> void:
	for group in groups.values():
		if group.positions.size() == 1:
			_box(group.size, group.positions[0], group.material)
			continue
		var mesh := BoxMesh.new()
		mesh.size = group.size
		var instances := MultiMesh.new()
		instances.transform_format = MultiMesh.TRANSFORM_3D
		instances.mesh = mesh
		instances.instance_count = group.positions.size()
		for index in group.positions.size():
			instances.set_instance_transform(index, Transform3D(Basis.IDENTITY, group.positions[index]))
		var batch := MultiMeshInstance3D.new()
		batch.multimesh = instances
		batch.material_override = group.material
		batch.set_meta("voidfront_static_box", true)
		add_child(batch)

func _static_geometry_manifest() -> Dictionary:
	# Expanded inventory is diagnostic only, outside the profile measurement.
	# Hash actual vertex arrays and exact transforms, not resource/process IDs.
	var boxes: Array[String] = []
	var meshes := {}
	for child in get_children():
		if not child.get_meta("voidfront_static_box", false): continue
		var mesh: BoxMesh
		var transforms: Array[Transform3D] = []
		if child is MeshInstance3D:
			mesh = child.mesh
			transforms.append(child.global_transform)
		elif child is MultiMeshInstance3D:
			mesh = child.multimesh.mesh
			for index in child.multimesh.instance_count:
				transforms.append(child.global_transform * child.multimesh.get_instance_transform(index))
		else: continue
		if not meshes.has(mesh):
			meshes[mesh] = var_to_bytes(mesh.surface_get_arrays(0)).hex_encode().sha256_text()
		var geometry := child as GeometryInstance3D
		var material: StandardMaterial3D = geometry.material_override
		var surface := [material.albedo_color, material.metallic, material.roughness,
			material.emission_enabled, material.emission, material.emission_energy_multiplier,
			material.cull_mode, material.shading_mode, material.transparency]
		var rendering := [geometry.cast_shadow, geometry.layers, geometry.gi_mode, geometry.visible]
		for transform in transforms:
			boxes.append(var_to_bytes([meshes[mesh], transform, surface, rendering]).hex_encode().sha256_text())
	boxes.sort()
	return {"box_count": boxes.size(), "sha256": JSON.stringify(boxes).sha256_text(), "box_hashes": boxes}

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
	camera.size = 96 if scale_smoke else 27
	camera.far = 400 if scale128 else 150
	add_child(camera)
	_update_camera()
	var floor_material := _material(Color("29383f"), 0.12)
	var w := map_size.x
	var h := map_size.y
	_box(Vector3(w, 0.45, h), Vector3(w / 2.0, -0.3, h / 2.0), floor_material)
	var static_boxes := {}
	var grid_material := _material(Color("334249"))
	for x in range(0, w + 1, 2):
		_queue_static_box(static_boxes, Vector3(0.018, 0.012, h), Vector3(x, -0.067, h / 2.0), grid_material)
	for z in range(0, h + 1, 2):
		_queue_static_box(static_boxes, Vector3(w, 0.012, 0.018), Vector3(w / 2.0, -0.067, z), grid_material)
	var border_material := _material(Color("253b43"), 0.4)
	_box(Vector3(w + 2, 0.6, 0.5), Vector3(w / 2.0, -0.15, -0.4), border_material)
	_box(Vector3(w + 2, 0.6, 0.5), Vector3(w / 2.0, -0.15, h + 0.4), border_material)
	_box(Vector3(0.5, 0.6, h), Vector3(-0.4, -0.15, h / 2.0), border_material)
	_box(Vector3(0.5, 0.6, h), Vector3(w + 0.4, -0.15, h / 2.0), border_material)
	var obstacle_material := _material(Color("263b44"), 0.25)
	var initial_structures: Array = bridge.snapshot().get("structures", []) if economy else []
	var initial_deposits: Array = bridge.snapshot().get("deposits", []) if economy else []
	for x in range(w):
		for z in range(h):
			# Economy structures have their own dynamic visuals and minimap markers.
			# Do not bake them into permanent terrain at startup.
			if economy:
				var dynamic_footprint := false
				for structure in initial_structures:
					if absi(x * 256 + 128 - int(structure.x)) <= 256 and absi(z * 256 + 128 - int(structure.z)) <= 256:
						dynamic_footprint = true
				for deposit in initial_deposits:
					if absi(x * 256 + 128 - int(deposit.x)) <= 128 and absi(z * 256 + 128 - int(deposit.z)) <= 128:
						dynamic_footprint = true
				if dynamic_footprint: continue
			if bridge.is_blocked(x, z):
				obstacle_cells.append(Vector2i(x, z))
				var height := 0.75 + float((x * 7 + z * 3) % 5) * 0.12
				_queue_static_box(static_boxes, Vector3(0.96, height, 0.96), Vector3(x + 0.5, height / 2, z + 0.5), obstacle_material)
				if (x + z) % 3 == 0:
					_queue_static_box(static_boxes, Vector3(0.64, 0.02, 0.04), Vector3(x + 0.5, height + 0.02, z + 0.5), _material(Color("769c9c"), 0.2, true))
	_build_static_boxes(static_boxes)
	for player in range(0 if economy else 2):
		var pad_color := Color("4d9caa") if player == 0 else Color("c78960")
		var pad := _ring(3.5, pad_color)
		var pad_x: float = (20 if player == 0 else 108) if scale128 else (5 if player == 0 else 27)
		pad.position = Vector3(pad_x, -0.04, h / 2.0)
		pad.material_override = _material(pad_color * 0.42, 0.25)
		add_child(pad)
		var lettering := Label3D.new()
		lettering.text = "CAIRN  /  01" if player == 0 else "CAIRN  /  02"
		lettering.font_size = 70
		lettering.pixel_size = 0.008
		lettering.modulate = pad_color
		lettering.rotation_degrees.x = -90
		lettering.position = Vector3(pad_x, 0.005, h / 2.0 + 4.5)
		add_child(lettering)
	order_mark = _ring(0.55, Color("a3ffea"))
	order_mark.visible = false
	add_child(order_mark)
	# Profile inventory distinguishes fixed world boxes from later transient beams.
	for child in get_children():
		if child is MeshInstance3D and child.mesh is BoxMesh:
			child.set_meta("voidfront_static_box", true)
	if economy:
		fog_material = ShaderMaterial.new()
		fog_material.shader = preload("res://fog.gdshader")
		fog_material.set_shader_parameter("map_extent", Vector2(map_size))
		fog_material.render_priority = 10
		for child in get_children():
			if child is GeometryInstance3D and child != order_mark:
				child.material_overlay = fog_material

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
	for entry in economy_actors.values(): entry.root.queue_free()
	economy_actors.clear()
	selected_entity.clear()
	build_pending = false
	if build_preview: build_preview.visible = false
	economy_notice = ""
	economy_result_sequence = -1
	selected.clear()
	control_groups.clear()
	if scale128:
		if not bridge.reset_scale(1, scale_count, not scale_smoke):
			push_error("Scale reset rejected")
			get_tree().quit(2)
			return
	elif economy: bridge.reset_economy(setup_seed, setup_ai and not (economy_smoke or production_smoke))
	else: bridge.reset(1, not (movement_smoke or crowd_smoke))
	if network and option_error.is_empty():
		if not bridge.network_start(local_player, local_port, remote_port, session_id, input_delay, finish_tick):
			network_notice = "Network session could not start."
		_update_network_status()
	current = bridge.snapshot()
	fog_tick = -1
	camera_dragging = false
	minimap_dragging = false
	if economy:
		camera.size = 27
		_home_camera()
		_update_fog()
	previous = current.duplicate(true)
	initial_hash = current.hash
	accumulator = 0
	attack_pending = false
	order_age = 99.0
	order_mark.visible = false
	initial_positions.clear()
	for unit in current.units:
		max_hp = maxf(max_hp, unit.hp)
		_spawn_actor(unit)
		initial_positions[unit.id] = Vector2(unit.x, unit.z)
	if economy: _present_economy()
	print("VOIDFRONT_MATCH tick=0 hash=", initial_hash, " units=", actors.size())

func _spawn_actor(unit: Dictionary) -> void:
	var root := Node3D.new()
	add_child(root)
	root.position = Vector3(unit.x / SCALE, 0, unit.z / SCALE)
	root.visible = _entity_visible(unit)
	var model: Node3D = WALKER.instantiate()
	root.add_child(model)
	if int(unit.get("kind", 0)) == 1:
		# Temporary smaller salvage rig; authored worker art remains a production gate.
		model.scale = Vector3(0.65, 0.7, 0.65)
		var cargo := MeshInstance3D.new()
		var cargo_mesh := BoxMesh.new()
		cargo_mesh.size = Vector3(0.52, 0.28, 0.52)
		cargo.mesh = cargo_mesh
		cargo.position = Vector3(0, 0.95, 0)
		cargo.material_override = _material(Color("d6b869"), 0.25)
		cargo.name = "SalvageCargo"
		root.add_child(cargo)
	if int(unit.get("kind", 0)) == 2:
		# Temporary tall, narrow ranged rig; authored Lancer art remains a production gate.
		model.scale = Vector3(0.8, 1.25, 0.8)
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

func _selected_workers() -> int:
	var count := 0
	for unit in current.get("units", []):
		if unit.id in selected and int(unit.get("kind", 0)) == 1 and unit.hp > 0: count += 1
	return count

func _economy_entity_at(screen: Vector2) -> Dictionary:
	var result := {}
	var nearest := 50.0
	for category in ["structures", "deposits"]:
		for entity in current.get(category, []):
			if int(entity.get("hp", 1)) <= 0 or not _entity_visible(entity): continue
			var point := camera.unproject_position(Vector3(entity.x / SCALE, 0.5, entity.z / SCALE))
			var distance := point.distance_to(screen)
			if distance < nearest:
				nearest = distance
				result = entity.duplicate()
				result["category"] = "deposit" if category == "deposits" else "structure"
	return result

func _present_economy() -> void:
	# Tombstones stay in authoritative snapshots; selection must not retain them.
	for unit in current.units:
		if unit.hp <= 0: selected.erase(unit.id)
	if selected_entity.get("category", "") == "structure":
		for structure in current.structures:
			if structure.id == selected_entity.id and structure.hp <= 0:
				selected_entity.clear()
				break
	if not selected_entity.is_empty() and not _entity_visible(selected_entity): selected_entity.clear()
	if current.winner != -1:
		build_pending = false
		attack_pending = false
	var sequence: int = current.get("result_sequences", [-1, -1])[local_player]
	if sequence != economy_result_sequence:
		economy_result_sequence = sequence
		match int(current.get("command_results", [0, 0])[local_player]):
			1: economy_notice = "Order accepted by simulation"
			2: economy_notice = "Invalid target: select a salvage deposit or your command anchor"
			3: economy_notice = "Not enough salvage: Foundry %d / Strider %d" % [current.foundry_cost, current.strider_cost]
			4: economy_notice = "Placement blocked: requires clear space away from structures, deposits and workers"
			5: economy_notice = "Select a living worker to gather or construct"
			6: economy_notice = "Select your living Foundry to produce units"
			7: economy_notice = "Finish construction before training units"
			8: economy_notice = "Production queue full (%d slots)" % current.production_queue_limit
			9: economy_notice = "Population limit reached (%d including queued units)" % current.population_cap
			10: economy_notice = "Unit roster limit reached"
			11: economy_notice = "Production queue is empty; nothing to cancel"
			12: economy_notice = "Not enough flux: Hardened Plating costs %d" % current.research_cost
			13: economy_notice = "Hardened Plating is already researched or underway"
			14: economy_notice = "Lancers require Hardened Plating research first (G)"
	if not build_preview:
		build_preview = _box(Vector3(2, 0.1, 2), Vector3.ZERO, _material(Color("4bbca5")))
		build_preview.visible = false
	for category in ["structures", "deposits"]:
		for entity in current.get(category, []):
			var key: String = category + str(entity.id)
			var deposit: bool = category == "deposits"
			var anchor: bool = not deposit and int(entity.kind) == 0
			if not economy_actors.has(key):
				var root := Node3D.new()
				add_child(root)
				root.position = Vector3(entity.x / SCALE, 0, entity.z / SCALE)
				var color := (Color("5cc8ff") if int(entity.get("kind", 0)) == 1 else Color("d6b869")) if deposit else (Color("438d97") if entity.player == 0 else Color("ae684e"))
				var mesh := BoxMesh.new()
				mesh.size = Vector3(0.94, 0.65, 0.94) if deposit else Vector3(1.94, 1.4 if anchor else 0.9, 1.94)
				var body := MeshInstance3D.new()
				body.mesh = mesh
				body.material_override = _material(color, 0.3)
				body.position.y = mesh.size.y / 2.0
				root.add_child(body)
				if anchor:
					var mast := MeshInstance3D.new()
					var mast_mesh := BoxMesh.new()
					mast_mesh.size = Vector3(0.4, 1.4, 0.4)
					mast.mesh = mast_mesh
					mast.position.y = 1.8
					mast.material_override = _material(color.lightened(0.2), 0.4)
					root.add_child(mast)
				var label := Label3D.new()
				label.font_size = 36
				label.pixel_size = 0.012
				label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
				label.no_depth_test = true
				label.position.y = 2.9 if anchor else 1.6
				root.add_child(label)
				var ring := _ring(1.2, Color("bdded6"))
				ring.position.y = 0.04
				root.add_child(ring)
				economy_actors[key] = {"root": root, "body": body, "label": label, "ring": ring}
			var entry: Dictionary = economy_actors[key]
			entry.root.visible = int(entity.get("hp", 1)) > 0 and _entity_visible(entity)
			if not entry.root.visible: continue
			entry.ring.visible = selected_entity.get("id", -1) == entity.id and selected_entity.get("category", "") == ("deposit" if deposit else "structure")
			if entry.ring.visible:
				selected_entity = entity.duplicate()
				selected_entity["category"] = "deposit" if deposit else "structure"
			if deposit:
				entry.label.text = "%s  %d" % ["FLUX" if int(entity.get("kind", 0)) == 1 else "SALVAGE", entity.remaining]
				entry.body.scale.y = 0.15 if entity.remaining == 0 else 1.0
			else:
				var progress := int(entity.build_ticks)
				var fraction := minf(float(progress) / float(current.build_duration), 1.0)
				entry.label.text = "COMMAND ANCHOR" if anchor else ("FOUNDRY" if fraction >= 1.0 else "FOUNDRY  %d%%" % int(fraction * 100))
				entry.label.text += "  %d HP" % entity.hp
				if not anchor:
					entry.body.scale.y = 0.2 + 0.8 * fraction
					if fraction >= 1.0 and int(entity.get("production_queue", 0)) > 0:
						entry.label.text = "FOUNDRY  %d QUEUED / %d%%" % [entity.production_queue, int(float(entity.production_ticks) / current.train_ticks * 100)]
						if entity.get("spawn_blocked", false): entry.label.text = "FOUNDRY / EXIT BLOCKED"

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
	if current.is_empty() or completed or setup_open: return
	if not option_error.is_empty(): return
	var profile_start := Time.get_ticks_usec() if presentation_profiler else 0
	if presentation_profiler and presentation_profiler.is_controlled():
		presentation_profiler.controlled_frame(delta, profile_start)
		return
	if smoke or network_smoke or movement_smoke or crowd_smoke or scale_smoke:
		var now := Time.get_ticks_usec()
		if last_frame_usec != 0 and (not movement_smoke or frame_times.size() < finish_tick * 12): frame_times.append((now - last_frame_usec) / 1000.0)
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
		# This bounded fixture advances eight canonical ticks per rendered frame.
		# It is reachability/replay evidence, never real-time responsiveness evidence.
		accumulator += STEP * 8 if (match_smoke and not match_realtime) or exploration_smoke else delta
		var ticks := 0
		while accumulator >= STEP and ticks < 8 and (not (movement_smoke or crowd_smoke or scale_smoke or economy_smoke or production_smoke or match_smoke or exploration_smoke) or current.tick < finish_tick):
			previous = current
			var start := Time.get_ticks_usec()
			bridge.advance()
			if presentation_profiler: presentation_profiler.record("bridge", Time.get_ticks_usec() - start)
			if smoke or movement_smoke or crowd_smoke or scale_smoke: sim_times.append((Time.get_ticks_usec() - start) / 1000.0)
			var snapshot_start := Time.get_ticks_usec() if presentation_profiler else 0
			current = bridge.snapshot()
			if presentation_profiler: presentation_profiler.record("snapshot", Time.get_ticks_usec() - snapshot_start)
			accumulator -= STEP
			ticks += 1
			if smoke: _smoke_tick()
			if movement_smoke: _movement_smoke_tick()
			if crowd_smoke: crowd_fixture.tick()
			if scale_smoke: scale_fixture.tick()
			if economy_smoke and economy_fixture: economy_fixture.tick()
			if production_smoke and production_fixture: production_fixture.tick()
			if match_smoke and match_fixture: match_fixture.tick()
			if exploration_smoke and exploration_fixture: exploration_fixture.tick()
	var present_start := Time.get_ticks_usec() if presentation_profiler else 0
	if economy: _update_fog()
	_present(clampf(accumulator / STEP, 0, 1), delta)
	if presentation_profiler: presentation_profiler.record("present", Time.get_ticks_usec() - present_start)
	# The first positive interpolation includes the new authoritative state.
	if network_smoke and accumulator > 0 and current.tick != presented_tick:
		presented_tick = current.tick
		_record_network_frame.call_deferred(int(current.tick))
	var camera_axis := Vector2.ZERO
	if Input.is_physical_key_pressed(KEY_UP): camera_axis.y -= 1
	if Input.is_physical_key_pressed(KEY_DOWN): camera_axis.y += 1
	if Input.is_physical_key_pressed(KEY_RIGHT): camera_axis.x += 1
	if Input.is_physical_key_pressed(KEY_LEFT): camera_axis.x -= 1
	camera_target += Vector3(camera_axis.normalized().x, 0, camera_axis.normalized().y) * delta * (26 if Input.is_physical_key_pressed(KEY_SHIFT) else 13)
	camera_target.x = clampf(camera_target.x, 2, map_size.x - 2)
	camera_target.z = clampf(camera_target.z, 2, map_size.y - 2)
	_update_camera()
	if economy:
		_present_economy()
		if build_pending:
			var cursor := _world_at(get_viewport().get_mouse_position())
			build_preview.position = Vector3(floorf(cursor.x) + 0.5, 0.08, floorf(cursor.z) + 0.5)
			var affordable: bool = current.salvage[local_player] >= current.foundry_cost
			build_preview.material_override = _material(Color("4bbca5") if affordable and bridge.can_build(int(cursor.x * SCALE), int(cursor.z * SCALE)) else Color("d65f56"))
		build_preview.visible = build_pending
	order_age += delta
	order_mark.visible = order_age < 1.2
	order_mark.scale = Vector3.ONE * (1.0 + minf(order_age, 1.2) * 0.5)
	if presentation_profiler:
		presentation_profiler.record("main_process", Time.get_ticks_usec() - profile_start)
		presentation_profiler.frame(delta)
	if movement_smoke and current.tick >= finish_tick and not completed:
		completed = true
		_finish_movement_smoke.call_deferred()
	if crowd_smoke and current.tick >= finish_tick and not completed:
		completed = true
		crowd_fixture.finish.call_deferred()
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
		entry.root.visible = _entity_visible(unit)
		var old: Dictionary = old_units.get(unit.id, unit)
		var from := Vector3(old.x / SCALE, 0, old.z / SCALE)
		var to := Vector3(unit.x / SCALE, 0, unit.z / SCALE)
		entry.root.position = from.lerp(to, alpha)
		entry.ring.visible = unit.id in selected and unit.hp > 0
		if int(unit.get("kind", 0)) == 1:
			var cargo_node: MeshInstance3D = entry.root.get_node("SalvageCargo")
			cargo_node.visible = int(unit.get("cargo", 0)) > 0
			cargo_node.material_override = _material(Color("5cc8ff") if int(unit.get("cargo_kind", 0)) == 1 else Color("d6b869"), 0.25)
		if unit.hp <= 0:
			if not entry.dead:
				_play(entry, "death")
				entry.dead = true
				_subdue_corpse(entry.model)
				selected.erase(unit.id)
			continue
		var direction := to - from
		var attack_target := Vector3.ZERO
		var has_attack_target := false
		if unit.target != 0 and actors.has(unit.target):
			attack_target = actors[unit.target].root.position
			has_attack_target = true
		elif int(unit.get("target_structure", 0)) != 0:
			for structure in current.get("structures", []):
				if structure.id == unit.target_structure:
					attack_target = Vector3(structure.x / SCALE, 0, structure.z / SCALE)
					has_attack_target = true
					break
		if has_attack_target: direction = attack_target - entry.root.position
		if direction.length_squared() > 0.0001:
			entry.root.rotation.y = lerp_angle(entry.root.rotation.y, atan2(-direction.x, -direction.z), minf(delta * 12, 1))
		if unit.cooldown > entry.cooldown and has_attack_target and entry.root.visible:
			_beam(entry.root.position, attack_target, unit.player)
			entry.clip = ""
			_play(entry, "attack")
			entry.flash = 0.25
			if match_smoke and match_fixture and int(unit.get("target_structure", 0)) != 0:
				match_fixture.record_structure_attack(unit, entry.clip)
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
	camera.position = camera_target + (Vector3(0, 100, 84) if scale128 else Vector3(0, 25, 21))
	camera.look_at(camera_target)

func _world_at(point: Vector2) -> Vector3:
	var origin := camera.project_ray_origin(point)
	var direction := camera.project_ray_normal(point)
	if absf(direction.y) < 0.001: return Vector3(-1, 0, -1)
	return origin + direction * (-origin.y / direction.y)

func _input(event: InputEvent) -> void:
	event.set_meta("voidfront_input_usec", Time.get_ticks_usec())

func _unhandled_input(event: InputEvent) -> void:
	if presentation_profiler and presentation_profiler.is_controlled(): return
	event_usec = int(event.get_meta("voidfront_input_usec", Time.get_ticks_usec()))
	if current.is_empty(): return
	if event is InputEventKey and event.pressed and event.physical_keycode == KEY_R:
		_reset()
		return
	if not option_error.is_empty(): return
	if setup_open:
		_setup_input(event)
		return
	if economy and not network and event is InputEventKey and event.pressed and not event.echo and event.physical_keycode == KEY_M:
		setup_open = true
		return
	if _camera_input(event): return
	if economy and current.winner != -1: return
	if network and (not network_state.get("ready", false) or str(network_state.get("state", "")) not in ["running", "stalled"]): return
	if event is InputEventKey and event.pressed and not event.echo:
		if event.physical_keycode >= KEY_0 and event.physical_keycode <= KEY_9:
			_control_group(event.physical_keycode - KEY_0, event.ctrl_pressed, event.shift_pressed)
			return
		match event.physical_keycode:
			KEY_F1, KEY_F2:
				attack_pending = false
				build_pending = false
				selected_entity.clear()
				selected.clear()
				for unit in current.units:
					if unit.player == local_player and unit.hp > 0 and (int(unit.get("kind", 0)) == 1) == (event.physical_keycode == KEY_F1): selected.append(unit.id)
				if network_smoke:
					own_selection_passed = not selected.is_empty()
					for unit in current.units:
						if unit.id in selected and unit.player != local_player: own_selection_passed = false
			KEY_A:
				attack_pending = true
				build_pending = false
			KEY_B:
				if economy and _selected_workers() > 0:
					build_pending = true
					attack_pending = false
			KEY_ESCAPE:
				attack_pending = false
				build_pending = false
			KEY_T:
				if economy: _issue(7, Vector3.ZERO)
			KEY_X:
				if economy: _issue(8, Vector3.ZERO)
			KEY_G:
				if economy: _issue(9, Vector3.ZERO)
			KEY_L:
				if economy: _issue(10, Vector3.ZERO)
			KEY_S: _issue(0, Vector3(0, 0, 0))
			KEY_H: _issue(3, Vector3(0, 0, 0))
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_RIGHT and event.pressed:
			if build_pending:
				build_pending = false
			else:
				var destination := _world_at(event.position)
				var entity := _economy_entity_at(event.position) if economy else {}
				var order := 1
				if not entity.is_empty() and entity.category == "structure" and entity.player != local_player:
					order = 2
					destination = Vector3(entity.x / SCALE, 0, entity.z / SCALE)
				if _selected_workers() > 0 and not entity.is_empty():
					if entity.category == "deposit": order = 4
					elif entity.player == local_player and entity.kind == 0: order = 5
					elif entity.player == local_player and entity.build_ticks < current.build_duration: order = 6
					if order != 1: destination = Vector3(entity.x / SCALE, 0, entity.z / SCALE)
				_issue(order, destination)
			attack_pending = false
		if event.button_index == MOUSE_BUTTON_LEFT:
			if build_pending and event.pressed:
				_issue(6, _world_at(event.position))
				build_pending = false
				return
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

func _setup_input(event: InputEvent) -> void:
	if not (event is InputEventKey and event.pressed and not event.echo): return
	match event.physical_keycode:
		KEY_ENTER, KEY_KP_ENTER, KEY_SPACE:
			setup_open = false
			_reset()
		KEY_TAB, KEY_O: setup_ai = not setup_ai
		KEY_RIGHT, KEY_EQUAL, KEY_KP_ADD: setup_seed = mini(setup_seed + 1, 999999)
		KEY_LEFT, KEY_MINUS, KEY_KP_SUBTRACT: setup_seed = maxi(setup_seed - 1, 1)

func _live_own_ids(ids: Array) -> Array[int]:
	var result: Array[int] = []
	for unit in current.units:
		if unit.hp > 0 and unit.player == local_player and unit.id in ids:
			result.append(int(unit.id))
	result.sort()
	return result

func _control_group(number: int, store_group: bool, additive: bool) -> void:
	if number < 0 or number > 9: return
	var members: Array[int] = _live_own_ids(control_groups.get(number, []))
	if store_group:
		if additive: members.append_array(selected)
		else: members = selected.duplicate()
		members = _live_own_ids(members)
		if members.is_empty(): control_groups.erase(number)
		else: control_groups[number] = members
		return
	if members.is_empty():
		control_groups.erase(number)
		return
	control_groups[number] = members.duplicate()
	if additive: members.append_array(selected)
	selected = _live_own_ids(members)
	selected_entity.clear()

func _select(from: Vector2, to: Vector2, additive: bool) -> void:
	selected_entity.clear()
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
	elif economy and from.distance_to(to) <= 7:
		selected_entity = _economy_entity_at(to)
	if movement_smoke and from.distance_to(to) <= 7:
		movement_selected = selected.size() == 1 and selected[0] == 1
	if smoke:
		# Assert after Godot dispatches the event, not from a tick that may precede dispatch.
		if from.distance_to(to) <= 7 and selected.size() == 1: click_selection_passed = true
		if from.distance_to(to) > 7 and selected.size() == 6: drag_selection_passed = true
		print("VOIDFRONT_SELECTION from=", from, " to=", to, " ids=", selected)

func _issue(order: int, at: Vector3) -> void:
	var command_actors: Array[int] = selected.duplicate()
	if order in [7, 8, 9, 10]:
		if selected_entity.get("category", "") != "structure" or selected_entity.get("player", -1) != local_player:
			economy_notice = "Select your Foundry to train, cancel or research"
			return
		command_actors = [int(selected_entity.id)]
		at = Vector3.ZERO
	if command_actors.is_empty(): return
	if at.x < 0 or at.x >= map_size.x or at.z < 0 or at.z >= map_size.y: return
	var accepted := false
	var sequence := -1
	if network:
		if not network_state.get("ready", false): return
		sequence = bridge.network_issue(order, PackedInt32Array(command_actors), int(at.x * SCALE), int(at.z * SCALE), event_usec)
		accepted = sequence >= 0
		if accepted and network_smoke:
			network_inputs.append({"sequence": sequence, "input_usec": event_usec, "order": order, "units": selected.duplicate(), "x": int(at.x * SCALE), "z": int(at.z * SCALE), "event_tick": current.tick})
		elif not accepted: network_notice = "Order rejected: " + str(bridge.network_status().get("error", "session unavailable"))
	else:
		accepted = bridge.issue(order, PackedInt32Array(command_actors), int(at.x * SCALE), int(at.z * SCALE))
	if movement_smoke:
		movement_inputs.append({"label": movement_label, "accepted": accepted, "order": order, "units": selected.duplicate(), "x": int(at.x * SCALE), "z": int(at.z * SCALE), "event_tick": current.tick, "input_usec": event_usec})
		if not accepted: movement_errors.append("Rejected input: " + movement_label)
	if crowd_smoke: crowd_fixture.record_input(accepted, order, at)
	if scale_smoke: scale_fixture.record_input(accepted, order, at)
	if economy_smoke and economy_fixture: economy_fixture.record_input(accepted, order, at)
	if production_smoke and production_fixture: production_fixture.record_input(accepted, order, at, command_actors)
	if match_smoke and match_fixture: match_fixture.record_input(accepted, order, at, command_actors)
	if exploration_smoke and exploration_fixture: exploration_fixture.record_input(accepted, order, at, command_actors)
	if economy:
		economy_notice = "Order submitted" if accepted else "Order rejected"
		if accepted and order == 7: economy_notice = "Training order submitted: %d salvage" % current.strider_cost
		elif accepted and order == 8: economy_notice = "Cancel last queued Strider; awaiting refund"
		elif accepted and order == 9: economy_notice = "Hardened Plating research submitted: %d flux" % current.research_cost
		if order == 6:
			economy_notice = "Foundry order submitted: %d salvage" % current.foundry_cost if bridge.can_build(int(at.x * SCALE), int(at.z * SCALE)) else "Cannot place here: blocked, unaffordable, or occupied; no salvage spent"
	if accepted:
		if order not in accepted_orders: accepted_orders.append(order)
		var feedback_at := Vector3(at.x, 0.05, at.z)
		if order in [7, 8, 9]: feedback_at = Vector3(selected_entity.x / SCALE, 0.05, selected_entity.z / SCALE)
		if order == 0 or order == 3:
			var center := Vector3.ZERO
			var count := 0
			for id in _live_own_ids(selected):
				if actors.has(id):
					center += actors[id].root.position
					count += 1
			if count > 0: feedback_at = Vector3(center.x / count, 0.05, center.z / count)
		order_mark.position = feedback_at
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

func _movement_input(label: String, at: Vector3) -> void:
	movement_label = label
	_smoke_mouse(MOUSE_BUTTON_RIGHT, camera.unproject_position(at), true)
	_smoke_mouse(MOUSE_BUTTON_RIGHT, camera.unproject_position(at), false)

func _movement_last_input(label: String) -> Dictionary:
	for entry in movement_inputs:
		if entry.label == label and entry.accepted: return entry
	return {}

func _movement_arrived(unit: Dictionary, label: String) -> bool:
	var command := _movement_last_input(label)
	return not command.is_empty() and unit.x == command.x and unit.z == command.z and not unit.moving

func _movement_smoke_tick() -> void:
	var unit: Dictionary = current.units[0]
	var before: Dictionary = previous.units[0]
	var dx: int = unit.x - before.x
	var dz: int = unit.z - before.z
	var step_squared := dx * dx + dz * dz
	movement_max_step_squared = maxi(movement_max_step_squared, step_squared)
	if dx != 0 and dz != 0 and absi(dx) != absi(dz): movement_arbitrary_steps += 1
	if movement_samples.size() < finish_tick:
		movement_samples.append({"tick": current.tick, "stage": movement_stage, "x": unit.x, "z": unit.z, "dx": dx, "dz": dz, "step_squared": step_squared, "moving": unit.moving, "order": unit.order, "hp": unit.hp})
	if current.tick == 1:
		var screen := camera.unproject_position(actors[1].root.position + Vector3(0, 0.5, 0))
		_smoke_mouse(MOUSE_BUTTON_LEFT, screen, true)
		_smoke_mouse(MOUSE_BUTTON_LEFT, screen, false)
	if movement_stage == 0 and current.tick >= 3 and movement_selected:
		_movement_input("direct", Vector3(10.734375, 0, 6.3515625))
		movement_stage = 1
	elif movement_stage == 1 and _movement_arrived(unit, "direct"):
		movement_direct_arrived = true
		_capture_movement.call_deferred("direct", int(current.tick))
		_movement_input("detour", Vector3(20.234375, 0, 6.68359375))
		movement_stage = 2
	elif movement_stage == 2:
		# Northern ridge is [15,17] x [3,9], expanded by 64 fixed units.
		if unit.x >= 3776 and unit.x <= 4416 and unit.z >= 2368 and not movement_ridge_crossed:
			movement_ridge_crossed = true
			_capture_movement.call_deferred("ridge", int(current.tick))
		if _movement_arrived(unit, "detour"):
			movement_detour_arrived = true
			_capture_movement.call_deferred("detour", int(current.tick))
			_movement_input("stop_leg", Vector3(23.3125, 0, 13.7890625))
			movement_stage = 3
	elif movement_stage == 3:
		var command := _movement_last_input("stop_leg")
		if not command.is_empty() and current.tick >= command.event_tick + 10 and unit.moving:
			movement_midsegment_stop = not _movement_arrived(unit, "stop_leg")
			movement_label = "stop"
			_smoke_key(KEY_S)
			movement_stage = 4
	elif movement_stage == 4:
		var command := _movement_last_input("stop")
		if not command.is_empty() and current.tick > command.event_tick:
			if movement_stop_tick < 0:
				movement_stop_tick = int(current.tick)
				movement_stop_at = Vector2i(unit.x, unit.z)
			movement_stop_samples += 1
			if Vector2i(unit.x, unit.z) != movement_stop_at or unit.moving: movement_stop_drift = true
			if movement_stop_samples >= 9:
				_capture_movement.call_deferred("stopped", int(current.tick))
				_movement_input("resume", Vector3(23.3125, 0, 13.7890625))
				movement_stage = 5
	elif movement_stage == 5:
		var command := _movement_last_input("resume")
		if not command.is_empty() and current.tick >= command.event_tick + 8 and unit.moving:
			movement_live_retarget = not _movement_arrived(unit, "resume")
			_movement_input("retarget", Vector3(21.671875, 0, 11.171875))
			movement_stage = 6
	elif movement_stage == 6 and _movement_arrived(unit, "retarget"):
		movement_retarget_arrived = true
		_capture_movement.call_deferred("retarget", int(current.tick))
		movement_stage = 7

func _capture_movement(phase: String, tick: int) -> void:
	if capture_path.is_empty(): return
	await RenderingServer.frame_post_draw
	var path := capture_path.get_basename() + "-" + phase + ".png"
	var error := get_viewport().get_texture().get_image().save_png(path)
	movement_captures.append({"phase": phase, "snapshot_tick": tick, "path": path, "error": error})
	if error != OK: movement_errors.append("Capture failed: " + phase)

func _finish_movement_smoke() -> void:
	await RenderingServer.frame_post_draw
	var input_ok := movement_inputs.size() == 6
	for entry in movement_inputs:
		input_ok = input_ok and entry.accepted and entry.units == [1]
	var direct := _movement_last_input("direct")
	var off_center: bool = not direct.is_empty() and direct.x % 256 != 128 and direct.z % 256 != 128
	var ok: bool = option_error.is_empty() and movement_errors.is_empty() and movement_stage == 7 and movement_selected and input_ok and off_center and movement_direct_arrived and movement_detour_arrived and movement_ridge_crossed and movement_retarget_arrived and movement_midsegment_stop and movement_live_retarget and movement_stop_samples >= 9 and not movement_stop_drift and movement_arbitrary_steps > 20 and movement_max_step_squared <= 1024 and movement_samples.size() == finish_tick
	var report := {"ok": ok, "mode": "movement", "tick": current.tick, "hash": current.hash, "input_stage": movement_stage, "single_unit_selection": movement_selected, "accepted_inputs": movement_inputs, "positions": movement_samples, "captures": movement_captures, "errors": movement_errors, "option_error": option_error, "direct_arrived": movement_direct_arrived, "off_center_destination": off_center, "detour_arrived": movement_detour_arrived, "ridge_crossed_with_clearance": movement_ridge_crossed, "retarget_arrived": movement_retarget_arrived, "stopped_midsegment": movement_midsegment_stop, "live_retarget": movement_live_retarget, "stop_samples": movement_stop_samples, "stop_without_drift": not movement_stop_drift, "arbitrary_heading_steps": movement_arbitrary_steps, "max_step_squared": movement_max_step_squared, "renderer": RenderingServer.get_video_adapter_name(), "frame_interval_ms_p95": _percentile(frame_times, 0.95), "frame_interval_ms_p99": _percentile(frame_times, 0.99), "sim_ms_p95": _percentile(sim_times, 0.95), "sim_ms_p99": _percentile(sim_times, 0.99), "note": "Single-unit packaged InputEvent fixture with enemy command AI disabled. Actual canonical input coordinates are retained unchanged from production conversion; exact authoritative arrivals are asserted. Static ridge and stop/retarget evidence only, not crowds, manual responsiveness, route optimality or representative battle performance. Movie timings include recording overhead."}
	if not capture_path.is_empty():
		var error := get_viewport().get_texture().get_image().save_png(capture_path)
		if error != OK:
			ok = false
			report["capture_error"] = error
	report["ok"] = ok
	if not report_path.is_empty():
		var file := FileAccess.open(report_path, FileAccess.WRITE)
		if file: file.store_string(JSON.stringify(report, "\t"))
		else: ok = false
	print("VOIDFRONT_MOVEMENT_SMOKE ", JSON.stringify(report))
	get_tree().quit(0 if ok else 3)

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

# Fog is authoritative simulation data; moving the camera never reveals terrain.
func _vision_at(x: float, z: float) -> int:
	if not economy: return 2
	var cell := Vector2i(floori(x), floori(z))
	if cell.x < 0 or cell.y < 0 or cell.x >= map_size.x or cell.y >= map_size.y: return 0
	var vision: Array = current.get("vision", [])
	if vision.size() <= local_player: return 0
	return int(vision[local_player][cell.y * map_size.x + cell.x])

func _entity_visible(entity: Dictionary) -> bool:
	return not economy or int(entity.get("player", -1)) == local_player or _vision_at(entity.x / SCALE, entity.z / SCALE) == 2

func _update_fog() -> void:
	if fog_tick == int(current.tick): return
	fog_tick = int(current.tick)
	var cells: PackedByteArray = current.vision[local_player]
	var pixels := PackedByteArray()
	pixels.resize(map_size.x * map_size.y * 4)
	for index in cells.size():
		pixels[index * 4] = 5
		pixels[index * 4 + 1] = 10
		pixels[index * 4 + 2] = 15
		pixels[index * 4 + 3] = 0 if cells[index] == 2 else (185 if cells[index] == 1 else 255)
	var image := Image.create_from_data(map_size.x, map_size.y, false, Image.FORMAT_RGBA8, pixels)
	if not fog_texture:
		fog_texture = ImageTexture.create_from_image(image)
		fog_material.set_shader_parameter("fog_map", fog_texture)
	else: fog_texture.update(image)

func _home_camera() -> void:
	for building in current.get("structures", []):
		if building.player == local_player and building.kind == 0:
			camera_target = Vector3(clampf(building.x / SCALE + 5.5, 2, map_size.x - 2), 0, building.z / SCALE)
			_update_camera()
			return

func _minimap_camera(point: Vector2) -> void:
	var rect: Rect2 = hud.minimap_rect()
	var position_on_map := (point - rect.position) / rect.size * Vector2(map_size)
	camera_target = Vector3(clampf(position_on_map.x, 2, map_size.x - 2), 0, clampf(position_on_map.y, 2, map_size.y - 2))
	_update_camera()

func _camera_input(event: InputEvent) -> bool:
	if event is InputEventKey and event.pressed and event.physical_keycode == KEY_HOME:
		_home_camera()
		return true
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_MIDDLE:
			camera_dragging = event.pressed
			return true
		if event.button_index == MOUSE_BUTTON_WHEEL_UP and event.pressed:
			camera.size = maxf(14, camera.size - 1.5)
			return true
		if event.button_index == MOUSE_BUTTON_WHEEL_DOWN and event.pressed:
			camera.size = minf(160 if scale128 else 36, camera.size + 1.5)
			return true
		if event.button_index == MOUSE_BUTTON_LEFT and not event.pressed and minimap_dragging:
			minimap_dragging = false
			return true
		if event.button_index == MOUSE_BUTTON_LEFT and not event.pressed and hud.dragging: return false
		if hud.minimap_rect().has_point(event.position):
			if event.button_index == MOUSE_BUTTON_LEFT and event.pressed:
				minimap_dragging = true
				_minimap_camera(event.position)
			return true
	if event is InputEventMouseMotion:
		if minimap_dragging:
			_minimap_camera(event.position)
			return true
		if camera_dragging:
			var displacement := _world_at(event.position - event.relative) - _world_at(event.position)
			camera_target += displacement
			camera_target.x = clampf(camera_target.x, 2, map_size.x - 2)
			camera_target.z = clampf(camera_target.z, 2, map_size.y - 2)
			_update_camera()
			return true
	return false

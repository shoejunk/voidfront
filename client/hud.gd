extends Control

var game: Node3D
var drag_from := Vector2.ZERO
var drag_to := Vector2.ZERO
var dragging := false
var title: Label
var status: Label
var selection: Label
var tip: RichTextLabel
var result: Label
var connection: Label
var setup: Label
var minimap_obstacles: MultiMesh

func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	title = _label(Vector2(32, 22), 27, Color("edf1ea"))
	title.text = "V O I D F R O N T"
	status = _label(Vector2(32, 59), 13, Color("8eaaa8"))
	status.text = "THE GLASS REACH   /   CAIRN COMPACT"
	connection = _label(Vector2(32, 82), 13, Color("d6c49c"))
	connection.size.x = 888
	connection.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	selection = _label(Vector2(36, 785), 22, Color("eaf1e9"))
	tip = RichTextLabel.new()
	tip.position = Vector2(36, 825)
	tip.size = Vector2(930, 100)
	tip.scroll_active = false
	tip.mouse_filter = Control.MOUSE_FILTER_IGNORE
	tip.add_theme_font_size_override("normal_font_size", 14)
	tip.add_theme_color_override("default_color", Color("a4b9b6"))
	add_child(tip)
	tip.text = "LMB / drag  select    RMB  move    A + click  attack-move    S  stop    H  hold\nCtrl + 0-9  save group    0-9  recall    Ctrl+Shift+number  add to group    Shift+number  add to selection\nF2  select army    arrows  camera    wheel  zoom    R  restart"
	setup = _label(Vector2(0, 0), 20, Color("eaf1e9"))
	setup.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	setup.size = Vector2(760, 330)
	result = _label(Vector2(590, 350), 34, Color("f0d19b"))
	result.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	# Static authoritative terrain is submitted once as a canvas batch. Unit dots
	# and camera-dependent health bars remain dynamic on every rendered frame.
	minimap_obstacles = MultiMesh.new()
	minimap_obstacles.transform_format = MultiMesh.TRANSFORM_2D
	minimap_obstacles.use_colors = true
	var cell_mesh := QuadMesh.new()
	cell_mesh.size = Vector2.ONE
	minimap_obstacles.mesh = cell_mesh
	minimap_obstacles.instance_count = game.obstacle_cells.size()
	for index in game.obstacle_cells.size():
		var cell: Vector2i = game.obstacle_cells[index]
		minimap_obstacles.set_instance_transform_2d(index, Transform2D(0, Vector2(cell) + Vector2(0.5, 0.5)))
		minimap_obstacles.set_instance_color(index, Color("101b22"))

func _label(at: Vector2, font_size: int, color: Color) -> Label:
	var label := Label.new()
	label.position = at
	label.size = Vector2(930, 100)
	label.add_theme_font_size_override("font_size", font_size)
	label.add_theme_color_override("font_color", color)
	label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(label)
	return label

func _process(_delta: float) -> void:
	if not is_instance_valid(game) or game.current.is_empty():
		return
	var profile_start := Time.get_ticks_usec() if game.presentation_profiler else 0
	var view := get_viewport_rect().size
	selection.position.y = view.y - (144 if game.economy else 112)
	tip.position.y = view.y - (120 if game.economy else 88)
	if not game.economy:
		selection.text = "%02d  /  CAIRN STRIDERS" % game.selected.size()
		if game.attack_pending:
			selection.text += "     —     SELECT ATTACK DESTINATION"
		status.text = "THE GLASS REACH   /   FIELD TRIAL 01     •     %02d:%02d" % [int(game.current.tick / 1200), int(game.current.tick / 20) % 60]
	if game.scale128:
		status.text = "SCALE FIELD   /   128 × 128   /   %d STRIDERS     •     %02d:%02d" % [game.current.units.size(), int(game.current.tick / 1200), int(game.current.tick / 20) % 60]
	if not game.economy: connection.text = ""
	if not game.economy: tip.text = "LMB / drag  select    RMB  move    A + click  attack-move    S  stop    H  hold\nCtrl + 0-9  save group    0-9  recall    Ctrl+Shift+number  add to group    Shift+number  add to selection\nF2  select army    arrows  camera    wheel  zoom    R  restart"
	result.size.x = 700 if game.network or not game.option_error.is_empty() else 500
	result.position = Vector2((view.x - result.size.x) / 2, view.y / 2 - 30)
	if game.current.winner == game.local_player:
		result.text = "SECTOR SECURED\nR  /  deploy again"
	elif game.current.winner in [0, 1]:
		result.text = "SIGNAL LOST\nR  /  deploy again"
	elif game.current.winner == 2:
		result.text = "MUTUAL DESTRUCTION\nR  /  deploy again"
	else:
		result.text = ""
	if game.network:
		var state := str(game.network_state.get("state", "handshake"))
		if game.current.winner != -1:
			result.text = result.text.get_slice("\n", 0) + "\nAwaiting session confirmation"
		status.text = "EXPERIMENTAL LOOPBACK 1v1   /   PLAYER %d   /   DELAY %d TICKS (%d ms)" % [game.local_player + 1, game.input_delay, game.input_delay * 50]
		connection.text = "%s   •   tick %d   /   confirmed %d   •   UDP %d → %d" % [state.to_upper(), game.current.tick, game.network_state.get("confirmed_ticks", 0), game.local_port, game.remote_port]
		tip.text = "LMB / drag  select own units    RMB  move    A + click  attack-move    S  stop    H  hold\nCtrl + 0-9  save group    0-9  recall    Ctrl+Shift+number  add to group    Shift+number  add to selection\nF2  select army    arrows  camera    wheel  zoom    shared match — restart after session ends"
		if state in ["handshake", "readiness"]:
			result.text = "CONNECTING TO PEER" if state == "handshake" else "PREPARING INPUT BUFFER"
			result.text += "\nOrders unlock when ready"
		elif state == "stalled":
			connection.text += "\nWaiting for peer data — simulation paused"
		elif state == "finishing":
			connection.text += "\nConfirming final state with peer"
		elif state == "complete":
			if game.current.winner == -1: result.text = "SESSION LIMIT REACHED"
			else: result.text = result.text.get_slice("\n", 0)
			result.text += "\nR  /  return to offline skirmish"
			tip.text = "Final state confirmed by both peers.\nR  returns this client to offline play; launch both clients again for a new network session."
		elif state == "error":
			result.text = "NETWORK SESSION ENDED\nR  /  return to offline skirmish"
			connection.text += "\n" + str(game.network_state.get("error", "Unknown network failure"))
		if not game.network_notice.is_empty(): connection.text += "\n" + game.network_notice
	if game.economy:
		var resources: Array = game.current.get("salvage", [0, 0])
		var cost: int = game.current.foundry_cost
		var flux: Array = game.current.get("flux", [0, 0])
		var research_ticks: int = int(game.current.get("research_ticks", [0, 0])[game.local_player])
		var plating := "PLATING DONE" if game.current.get("researched", [false, false])[game.local_player] else ("PLATING %d%%" % int(float(research_ticks) / game.current.research_total_ticks * 100) if research_ticks > 0 else "PLATING %d FLUX" % game.current.research_cost)
		status.text = "SALVAGE %d   /   FLUX %d   /   %s   /   POPULATION %d + %d QUEUED / %d   /   FOUNDRY %d • STRIDER %d / LANCER %d+%d FLUX [L]" % [resources[game.local_player], flux[game.local_player], plating, game.current.population_used[game.local_player], game.current.population_reserved[game.local_player], game.current.population_cap, cost, game.current.strider_cost, game.current.lancer_cost, game.current.lancer_flux_cost]
		var briefing := "64 x 48 EXPLORATION / Scout the dark ground. Destroy the enemy anchor; protect yours. Gather, build and train."
		# Keep the session readiness/stall/error text above the briefing in shared matches.
		connection.text = (connection.text + "\n" + briefing) if game.network else briefing
		if not game.network and not game.current.get("enemy_ai", false): connection.text += " Passive economy fixture."
		if game.current.winner == game.local_player: result.text = "VICTORY / ENEMY ANCHOR DESTROYED\nR  /  new match"
		elif game.current.winner in [0, 1]: result.text = "DEFEAT / YOUR ANCHOR DESTROYED\nR  /  new match"
		result.size.x = 920
		result.position.x = (view.x - result.size.x) / 2
		var workers: int = game._selected_workers()
		selection.text = "%02d WORKERS / %02d STRIDERS" % [workers, game.selected.size() - workers]
		if workers > 0:
			var cargo := 0
			for unit in game.current.units:
				if unit.id in game.selected: cargo += int(unit.get("cargo", 0))
			selection.text += "   /   CARRYING %d" % cargo
		if not game.selected_entity.is_empty():
			var entity: Dictionary = game.selected_entity
			if entity.category == "deposit": selection.text = "%s DEPOSIT   /   %d REMAINING" % ["FLUX (CONTESTED)" if int(entity.get("kind", 0)) == 1 else "SALVAGE", entity.remaining]
			elif entity.kind == 0: selection.text = "COMMAND ANCHOR   /   %d HP" % entity.hp
			else:
				selection.text = "FOUNDRY   /   %s" % ("READY" if entity.build_ticks >= game.current.build_duration else "CONSTRUCTING %d%%" % int(float(entity.build_ticks) / game.current.build_duration * 100))
				if entity.build_ticks >= game.current.build_duration:
					selection.text += "   /   QUEUE %d/%d" % [entity.production_queue, game.current.production_queue_limit]
					if entity.production_queue > 0: selection.text += "   /   STRIDER %d%%" % int(float(entity.production_ticks) / game.current.train_ticks * 100)
					if entity.spawn_blocked: selection.text += "   /   EXIT BLOCKED: MOVE UNITS"
		if game.build_pending: selection.text = "PLACE FOUNDRY   /   %d SALVAGE   /   GREEN VALID • RED BLOCKED OR UNAFFORDABLE" % cost
		tip.text = "F1  workers    F2  army    LMB / drag  select    RMB  move / gather / work / attack building    Shift+RMB queue (4)\nB + click  Foundry (%d)    T  Strider (%d)    L  Lancer (%d+%d flux, needs plating)    X  refund last    G  Hardened Plating (%d flux)    A + click  attack-move    S  stop    H  hold\nCtrl+0-9  save / 0-9  recall    Arrows  pan (Shift fast)    MMB / minimap  pan    Home  base    Wheel  zoom    R  restart\n" % [cost, game.current.strider_cost, game.current.lancer_cost, game.current.lancer_flux_cost, game.current.research_cost] + game.economy_notice
	if not game.option_error.is_empty():
		result.text = "INVALID LAUNCH OPTIONS\nR  /  start offline skirmish"
		connection.text = game.option_error
	setup.visible = game.setup_open
	if game.setup_open:
		setup.position = Vector2((view.x - setup.size.x) / 2, view.y / 2 - 150)
		setup.text = "V O I D F R O N T   /   SKIRMISH\n\nMAP   THE GLASS REACH  64 x 48  (you: Cairn Compact, south-west)\nOPPONENT   %s      [Tab]\nMATCH SEED   %d      [Left / Right]\n\nEnter  start match\n\nDuring a match:  R  restart with these settings   /   M  return here" % ["ECONOMIC AI" if game.setup_ai else "PASSIVE (no opponent actions)", game.setup_seed]
		result.text = ""
	queue_redraw()
	if game.presentation_profiler: game.presentation_profiler.record("hud_process", Time.get_ticks_usec() - profile_start)

func _draw() -> void:
	var profile_start := Time.get_ticks_usec() if is_instance_valid(game) and game.presentation_profiler else 0
	var view := get_viewport_rect().size
	var network_panel: bool = is_instance_valid(game) and (game.network or game.economy or not game.option_error.is_empty())
	var panel_size := Vector2(920, 128) if network_panel else Vector2(548, 80)
	draw_rect(Rect2(Vector2(16, 12), panel_size), Color(0.025, 0.047, 0.055, 0.94))
	draw_rect(Rect2(16, 12, 3, panel_size.y), Color("61c9c6"))
	var extra_help_height := 32 if is_instance_valid(game) and game.economy else 0
	draw_rect(Rect2(16, view.y - 131 - extra_help_height, 965, 114 + extra_help_height), Color(0.025, 0.047, 0.055, 0.96))
	draw_line(Vector2(16, view.y - 131 - extra_help_height), Vector2(981, view.y - 131 - extra_help_height), Color("4b777b"), 1)
	if not is_instance_valid(game) or game.current.is_empty():
		return
	if game.setup_open:
		draw_rect(Rect2(Vector2.ZERO, view), Color(0.01, 0.02, 0.025, 0.72))
		draw_rect(Rect2(Vector2((view.x - 800) / 2, view.y / 2 - 170), Vector2(800, 330)), Color(0.025, 0.047, 0.055, 0.97))
		draw_rect(Rect2((view.x - 800) / 2, view.y / 2 - 170, 3, 330), Color("61c9c6"))
		return
	if not result.text.is_empty():
		var result_width := 740 if network_panel else 530
		draw_rect(Rect2((view.x - result_width) / 2, view.y / 2 - 48, result_width, 128), Color(0.025, 0.047, 0.055, 0.94))
	var map_rect := minimap_rect()
	var map_scale := map_rect.size / Vector2(game.map_size)
	draw_rect(map_rect.grow(6), Color("12252c"))
	draw_rect(map_rect, Color("27373b"))
	draw_set_transform(map_rect.position, 0, map_scale)
	draw_multimesh(minimap_obstacles, null)
	draw_set_transform(Vector2.ZERO)
	if game.economy and game.fog_texture: draw_texture_rect(game.fog_texture, map_rect, false)
	var selected_ids := {}
	if game.economy:
		for structure in game.current.get("structures", []):
			if structure.hp <= 0 or not game._entity_visible(structure): continue
			var at := map_rect.position + Vector2(structure.x, structure.z) / 256.0 * map_scale
			draw_rect(Rect2(at - Vector2(4, 4), Vector2(8, 8)), Color("62d7d1") if structure.player == 0 else Color("ef9259"))
		for deposit in game.current.get("deposits", []):
			if deposit.remaining > 0 and game._entity_visible(deposit):
				var at := map_rect.position + Vector2(deposit.x, deposit.z) / 256.0 * map_scale
				draw_circle(at, 3.0, Color("5cc8ff") if int(deposit.get("kind", 0)) == 1 else Color("d6b869"))
	for id in game.selected: selected_ids[id] = true
	for u in game.current.units:
		if u.hp <= 0 or not game._entity_visible(u):
			continue
		var color := Color("64e5df") if u.player == 0 else Color("fda06d")
		var at := map_rect.position + Vector2(u.x, u.z) / 256.0 * map_scale
		draw_circle(at, 2.5, color)
		if game.actors.has(u.id) and (not game.scale128 or selected_ids.has(u.id) or u.hp < game.max_hp):
			var actor: Node3D = game.actors[u.id].root
			var screen: Vector2 = game.camera.unproject_position(actor.position + Vector3(0, 1.8, 0))
			if not game.camera.is_position_behind(actor.position):
				var bar_width: float = clampf(24.0 * 27.0 / game.camera.size, 3, 24) if game.scale128 else 36.0
				draw_rect(Rect2(screen - Vector2(bar_width / 2, 0), Vector2(bar_width, 4)), Color("102128"))
				draw_rect(Rect2(screen - Vector2(bar_width / 2, 0), Vector2(bar_width * clampf(float(u.hp) / game.max_hp, 0, 1), 3)), color)
	if game.economy:
		var corners := PackedVector2Array()
		for screen in [Vector2.ZERO, Vector2(view.x, 0), view, Vector2(0, view.y), Vector2.ZERO]:
			var world: Vector3 = game._world_at(screen)
			var bounded := Vector2(clampf(world.x, 0, game.map_size.x), clampf(world.z, 0, game.map_size.y))
			corners.append(map_rect.position + bounded * map_scale)
		draw_polyline(corners, Color("d8f3e8"), 1.0, true)
	if dragging:
		draw_rect(Rect2(drag_from, drag_to - drag_from).abs(), Color(0.3, 0.85, 0.83, 0.1))
		draw_rect(Rect2(drag_from, drag_to - drag_from).abs(), Color("64d8d0"), false, 1)
	if game.presentation_profiler: game.presentation_profiler.record("hud_draw", Time.get_ticks_usec() - profile_start)

func minimap_rect() -> Rect2:
	var view := get_viewport_rect().size
	return Rect2(view.x - 169, view.y - 174, 153, 153) if game.map_size.x == game.map_size.y else Rect2(view.x - 220, view.y - 174, 204, 153)

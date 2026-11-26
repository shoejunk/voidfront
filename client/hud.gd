extends Control

var game: Node3D
var drag_from := Vector2.ZERO
var drag_to := Vector2.ZERO
var dragging := false
var title: Label
var clear_view_hint: Label
var status: Label
var selection: Label
var tip: RichTextLabel
var result: Label
var connection: Label
var setup: Label
var message_center := CenterContainer.new()
var message_panel := PanelContainer.new()
var message_detail: Label
var message_actions: Label
var minimap_obstacles: MultiMesh
var command_card := preload("res://command_card.gd").new()

func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	title = _label(Vector2(32, 22), 27, Color("edf1ea"))
	title.text = "V O I D F R O N T"
	clear_view_hint = _label(Vector2.ZERO, 14, Color("a4b9b6"))
	clear_view_hint.text = "Hold Space  /  clear view"
	clear_view_hint.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	clear_view_hint.size = Vector2(210, 28)
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
	add_child(command_card)
	command_card.command_pressed.connect(game._activate_command)
	# A separate canvas item keeps the opaque panel and its text together above
	# paths, health bars and all other HUD elements at every window size.
	add_child(message_center)
	message_center.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	message_center.z_index = 1
	message_center.mouse_filter = Control.MOUSE_FILTER_IGNORE
	message_center.hide()
	message_center.add_child(message_panel)
	message_panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var frame := StyleBoxFlat.new()
	frame.bg_color = Color("0d1b20")
	frame.border_color = Color("61c9c6")
	frame.border_width_top = 2
	frame.content_margin_left = 24
	frame.content_margin_right = 24
	frame.content_margin_top = 22
	frame.content_margin_bottom = 22
	message_panel.add_theme_stylebox_override("panel", frame)
	var lines := VBoxContainer.new()
	lines.mouse_filter = Control.MOUSE_FILTER_IGNORE
	lines.add_theme_constant_override("separation", 14)
	message_panel.add_child(lines)
	setup = _message_label(lines, 30, Color("eaf1e9"))
	result = _message_label(lines, 34, Color("f0d19b"))
	message_detail = _message_label(lines, 24, Color("eaf1e9"))
	message_actions = _message_label(lines, 20, Color("aad9d1"))
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

func _message_label(parent: VBoxContainer, font_size: int, color: Color) -> Label:
	var label := Label.new()
	label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	label.add_theme_font_size_override("font_size", font_size)
	label.add_theme_color_override("font_color", color)
	label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	parent.add_child(label)
	return label

func _process(_delta: float) -> void:
	if not is_instance_valid(game) or game.current.is_empty():
		return
	var profile_start := Time.get_ticks_usec() if game.presentation_profiler else 0
	var view := get_viewport_rect().size
	var workers := 0
	var detail := ""
	var actions := ""
	selection.position.y = view.y - (144 if game.economy else 112)
	tip.position.y = view.y - (120 if game.economy else 88)
	if not game.economy:
		selection.text = "%02d  /  CAIRN STRIDERS" % game.selected.size()
		status.text = "THE GLASS REACH   /   FIELD TRIAL 01     •     %02d:%02d" % [int(game.current.tick / 1200), int(game.current.tick / 20) % 60]
	if game.scale128:
		status.text = "SCALE FIELD   /   128 × 128   /   %d STRIDERS     •     %02d:%02d" % [game.current.units.size(), int(game.current.tick / 1200), int(game.current.tick / 20) % 60]
	if not game.economy: connection.text = ""
	if not game.economy: tip.text = "LMB / drag  select    RMB  move    A + click  attack-move    S  stop    H  hold\nCtrl + 0-9  save group    0-9  recall    Ctrl+Shift+number  add to group    Shift+number  add to selection\nF2  select army    arrows  camera    wheel  zoom    R  restart"
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
		connection.text = "PLAYER %d / DELAY %d TICKS   •   %s   •   tick %d / confirmed %d   •   UDP %d → %d" % [game.local_player + 1, game.input_delay, state.to_upper(), game.current.tick, game.network_state.get("confirmed_ticks", 0), game.local_port, game.remote_port]
		tip.text = "LMB / drag  select own units    RMB  move    A + click  attack-move    S  stop    H  hold\nCtrl + 0-9  save group    0-9  recall    Ctrl+Shift+number  add to group    Shift+number  add to selection\nF2  select army    arrows  camera    wheel  zoom    shared match — restart after session ends"
		if state in ["handshake", "readiness"]:
			result.text = "CONNECTING TO PEER" if state == "handshake" else "PREPARING INPUT BUFFER"
			result.text += "\nOrders unlock when ready"
		elif state == "stalled":
			connection.text += "\nWaiting for peer data — simulation paused"
		elif state == "finishing":
			connection.text += "\nConfirming final state with peer"
		elif state == "error":
			connection.text += "\n" + str(game.network_state.get("error", "Unknown network failure"))
		if not game.network_notice.is_empty(): connection.text += "\n" + game.network_notice
	if game.economy:
		var resources: Array = game.current.get("salvage", [0, 0])
		var cost: int = game.current.foundry_cost
		var flux: Array = game.current.get("flux", [0, 0])
		var research_ticks: int = int(game.current.get("research_ticks", [0, 0])[game.local_player])
		var plating := "PLATING DONE" if game.current.get("researched", [false, false])[game.local_player] else ("PLATING %d%%" % int(float(research_ticks) / game.current.research_total_ticks * 100) if research_ticks > 0 else "PLATING %d FLUX" % game.current.research_cost)
		status.text = "SALVAGE %d   /   FLUX %d   /   %s   /   POPULATION %d + %d QUEUED / %d   /   FOUNDRY %d • STRIDER %d / LANCER %d+%d FLUX [L]" % [resources[game.local_player], flux[game.local_player], plating, game.current.population_used[game.local_player], game.current.population_reserved[game.local_player], game.current.population_cap, cost, game.current.strider_cost, game.current.lancer_cost, game.current.lancer_flux_cost]
		if not game.network: connection.text = ""
		if game.commands_available():
			var briefing := "64 x 48 EXPLORATION / Scout the dark ground. Destroy the enemy anchor; protect yours. Gather, build and train."
			connection.text = (connection.text + "\n" + briefing) if game.network else briefing
			if not game.network and not game.current.get("enemy_ai", false): connection.text += " Passive economy fixture."
		elif game.current.winner != -1 or (game.network and game.network_state.get("state") == "complete"):
			status.text = "FINAL RESERVES   /   %d SALVAGE   /   %d FLUX" % [resources[game.local_player], flux[game.local_player]]
		if game.current.winner in [0, 1]:
			var outcome := "VICTORY / ENEMY ANCHOR DESTROYED" if game.current.winner == game.local_player else "DEFEAT / YOUR ANCHOR DESTROYED"
			result.text = outcome + ("\nAwaiting session confirmation" if game.network else "\nR  /  new match")
		var lancers := 0
		var salvage_load := 0
		var flux_load := 0
		var returning_workers := 0
		var weakest_worker: Dictionary = {}
		for unit in game.current.units:
			if unit.id not in game.selected or unit.hp <= 0: continue
			if unit.kind == 2: lancers += 1
			if unit.kind != 1: continue
			workers += 1
			if weakest_worker.is_empty() or unit.hp * weakest_worker.max_hp < weakest_worker.hp * unit.max_hp:
				weakest_worker = unit
			if unit.cargo_kind == 1: flux_load += int(unit.cargo)
			else: salvage_load += int(unit.cargo)
			if unit.returning: returning_workers += 1
		var roster := PackedStringArray()
		var striders: int = game.selected.size() - workers - lancers
		if workers > 0: roster.append("Worker" if workers == 1 else "%d Workers" % workers)
		if striders > 0: roster.append("Strider" if striders == 1 else "%d Striders" % striders)
		if lancers > 0: roster.append("Lancer" if lancers == 1 else "%d Lancers" % lancers)
		selection.text = "Select a unit or structure" if roster.is_empty() else "  /  ".join(roster)
		if workers > 0:
			var loads := PackedStringArray()
			if salvage_load: loads.append("%d salvage" % salvage_load)
			if flux_load: loads.append("%d flux" % flux_load)
			if game.selected.size() == 1:
				selection.text += "   ·   %d/%d HP" % [weakest_worker.hp, weakest_worker.max_hp]
			elif weakest_worker.hp < weakest_worker.max_hp:
				selection.text += "   ·   Lowest worker HP %d/%d" % [weakest_worker.hp, weakest_worker.max_hp]
			var load_text := "Empty cargo" if loads.is_empty() else " + ".join(loads)
			if returning_workers and game.selected.size() == 1:
				load_text = "Returning empty to anchor" if loads.is_empty() else "Returning %s to anchor" % load_text
			elif returning_workers:
				load_text += "   ·   %d returning to anchor" % returning_workers
			elif game.selected.size() == 1 and weakest_worker.player == game.local_player and weakest_worker.order == game.Action.GATHER:
				for deposit in game.current.deposits:
					if deposit.id != weakest_worker.resource_id: continue
					var tile := Vector2i(floori(deposit.x / game.SCALE), floori(deposit.z / game.SCALE))
					if game._vision_at(tile.x, tile.y) > 0:
						var resource := "Flux" if deposit.kind == 1 else "Salvage"
						load_text += "  →  %s (%d, %d)" % [resource, tile.x, tile.y]
					break
			selection.text += "   ·   " + load_text
		if not game.selected_entity.is_empty():
			var entity: Dictionary = game.selected_entity
			if entity.category == "deposit": selection.text = "%s DEPOSIT   /   %d REMAINING" % ["FLUX (CONTESTED)" if int(entity.get("kind", 0)) == 1 else "SALVAGE", entity.remaining]
			else:
				if entity.kind == 0: selection.text = "COMMAND ANCHOR   /   %d HP" % entity.hp
				else: selection.text = "FOUNDRY   /   %s" % ("READY" if entity.build_ticks >= game.current.build_duration else "CONSTRUCTING %d%%" % int(float(entity.build_ticks) / game.current.build_duration * 100))
				if entity.build_ticks >= game.current.build_duration:
					selection.text += "   /   QUEUE %d/%d" % [entity.production_queue, game.current.production_queue_limit]
					if entity.production_queue > 0:
						var unit_name := "LANCER" if int(entity.get("queue_lancers", 0)) & 1 else "STRIDER"
						if entity.kind == 0: unit_name = "WORKER"
						selection.text += "   /   %s %d%%" % [unit_name, int(float(entity.production_ticks) / game.current.train_ticks * 100)]
					if entity.spawn_blocked: selection.text += "   /   EXIT BLOCKED: MOVE UNITS"
		tip.text = "F1  workers    F2  army    LMB / drag  select    RMB  move / gather / work / attack building    Shift+RMB queue (4)\nB + click  Foundry (%d)    T  Strider (%d)    L  Lancer (%d+%d flux, needs plating)    X  refund last    G  Hardened Plating (%d flux)    A + click  attack-move    S  stop    H  hold\nCtrl+0-9  save / 0-9  recall    Arrows  pan (Shift fast)    MMB / minimap  pan    Home  base    Wheel  zoom    R  restart\n" % [cost, game.current.strider_cost, game.current.lancer_cost, game.current.lancer_flux_cost, game.current.research_cost] + game.economy_notice
	var paths: Array[Dictionary] = game._order_paths()
	if not paths.is_empty():
		var pending := 0
		for path in paths: pending = maxi(pending, path.points.size() - 1)
		selection.text += "   /   UP TO %d/%d WAYPOINTS%s" % [pending, game.current.order_queue_limit, " (FULL)" if pending == game.current.order_queue_limit else ""]
	if game.network:
		var state := str(game.network_state.get("state", ""))
		if state == "complete":
			result.text = "SESSION LIMIT REACHED" if game.current.winner == -1 else result.text.get_slice("\n", 0)
			if game.lobby:
				detail = "Rematch   •   " + game.lobby_text()
				actions = "Esc  /  close lobby     •     R  /  return to offline skirmish"
			else:
				result.text += "\nR  /  return to offline skirmish"
				if not game.lobby_round.is_empty() and not game.lobby_archive_path.is_empty():
					detail = "Replay saved. The lobby is closed.\nPress M after returning offline to set up another match."
				else:
					detail = game.lobby_notice
		elif state == "error":
			result.text = "NETWORK SESSION ENDED\nR  /  return to offline skirmish"
			detail = game.lobby_notice
		else:
			tip.text = tip.text.replace("R  restart", "Shift+Esc  leave shared match" if game.lobby else "R  available after session")
	elif game.economy:
		tip.text = tip.text.replace("R  restart", "R  restart    M  setup")
	if not game.option_error.is_empty():
		result.text = "INVALID LAUNCH OPTIONS\nR  /  start offline skirmish"
		connection.text = game.option_error
	command_card.refresh(game, selection.text, workers)
	setup.visible = game.setup_open
	if game.setup_open:
		if game.lobby:
			setup.text = "V O I D F R O N T   /   %s LOBBY" % ("HOST" if game.lobby_host else "GUEST")
			detail = game.lobby_text()
			actions = "Esc  /  close lobby"
		else:
			setup.text = "V O I D F R O N T   /   SKIRMISH"
			detail = "The Glass Reach   •   64 × 48   •   Cairn Compact, northwest\nOpponent   %s   [Tab]\nMatch seed   %d   [Left / Right]" % ["Economic AI" if game.setup_ai else "Passive", game.setup_seed]
			actions = "Enter  /  play offline     H  /  host 1v1     J  /  join loopback 1v1"
			if not game.lobby_notice.is_empty(): actions += "\n" + game.lobby_notice
		result.text = ""
	title.visible = not game.setup_open
	clear_view_hint.visible = game.commands_available()
	clear_view_hint.position = Vector2((920 if game.economy or game.network else 548) - clear_view_hint.size.x, 28)
	status.visible = not game.setup_open
	connection.visible = not game.setup_open
	result.visible = not result.text.is_empty()
	message_center.visible = setup.visible or result.visible
	message_detail.text = detail
	message_detail.visible = not detail.is_empty()
	message_actions.text = actions
	message_actions.visible = not actions.is_empty()
	message_panel.custom_minimum_size.x = minf(view.x - 32, 840)
	selection.visible = not command_card.visible and not message_center.visible
	tip.visible = selection.visible
	queue_redraw()
	if game.presentation_profiler: game.presentation_profiler.record("hud_process", Time.get_ticks_usec() - profile_start)

func _status_panel_rect() -> Rect2:
	var network_panel: bool = is_instance_valid(game) and (game.network or game.economy or not game.option_error.is_empty())
	var panel_size := Vector2(920, 128) if network_panel else Vector2(548, 80)
	if is_instance_valid(game) and game.economy and not game.network and connection.text.is_empty():
		panel_size = Vector2(380, 80)
	if is_instance_valid(game) and game.network:
		panel_size.y += maxi(0, connection.get_line_count() - 2) * 18
	return Rect2(Vector2(16, 12), panel_size)

func _draw() -> void:
	var profile_start := Time.get_ticks_usec() if is_instance_valid(game) and game.presentation_profiler else 0
	var view := get_viewport_rect().size
	if is_instance_valid(game) and game.setup_open:
		draw_rect(Rect2(Vector2.ZERO, view), Color(0.01, 0.02, 0.025, 0.72))
		return
	var panel := _status_panel_rect()
	draw_rect(panel, Color(0.025, 0.047, 0.055, 0.94))
	draw_rect(Rect2(panel.position, Vector2(3, panel.size.y)), Color("61c9c6"))
	var extra_help_height := 32 if is_instance_valid(game) and game.economy else 0
	if selection.visible:
		draw_rect(Rect2(16, view.y - 131 - extra_help_height, 965, 114 + extra_help_height), Color(0.025, 0.047, 0.055, 0.96))
		draw_line(Vector2(16, view.y - 131 - extra_help_height), Vector2(981, view.y - 131 - extra_help_height), Color("4b777b"), 1)
	if not is_instance_valid(game) or game.current.is_empty():
		return
	_draw_order_paths()
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
	var label_rects := _economy_label_rects()
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
				var bar := Rect2(screen - Vector2(bar_width / 2, 0), Vector2(bar_width, 4))
				if not label_rects.is_empty():
					bar = _place_health_bar(bar, label_rects, view.y)
				draw_rect(bar, Color("102128"))
				draw_rect(Rect2(bar.position, Vector2(bar_width * clampf(float(u.hp) / float(u.max_hp), 0, 1), 3)), color)
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

func _economy_label_rects() -> Array[Rect2]:
	var rects: Array[Rect2] = []
	if not game.economy: return rects
	var camera: Camera3D = game.camera
	var center: Vector3 = game.camera_target
	var pixels_per_unit := camera.unproject_position(center + camera.global_basis.x).distance_to(camera.unproject_position(center))
	for entry in game.economy_actors.values():
		var label: Label3D = entry.label
		if not label.is_visible_in_tree() or camera.is_position_behind(label.global_position): continue
		rects.append(_world_label_rect(label, pixels_per_unit))
	return rects

func _world_label_rect(label: Label3D, pixels_per_unit: float) -> Rect2:
	var font := label.font if label.font else ThemeDB.fallback_font
	var text_size := font.get_string_size(label.text, HORIZONTAL_ALIGNMENT_LEFT, -1, label.font_size)
	var text_scale := label.pixel_size * pixels_per_unit
	var screen_size := text_size * text_scale
	var at: Vector2 = game.camera.unproject_position(label.global_position)
	return Rect2(at - screen_size * 0.5, screen_size).grow(label.outline_size * text_scale + 2.0)

func status_covers_label(label: Label3D) -> bool:
	# The RTS camera keeps its vertical extent when the window changes shape.
	var pixels_per_unit: float = get_viewport_rect().size.y / game.camera.size
	return is_visible_in_tree() and _status_panel_rect().intersects(_world_label_rect(label, pixels_per_unit))

func _place_health_bar(desired: Rect2, labels: Array[Rect2], view_height: float) -> Rect2:
	var above := desired
	var below := desired
	# Find the nearest clear position on each side, including overlapping labels.
	for step in labels.size():
		var moved := false
		for label in labels:
			if above.intersects(label):
				above.position.y = floorf(label.position.y - above.size.y)
				moved = true
			if below.intersects(label):
				below.position.y = ceilf(label.end.y)
				moved = true
		if not moved: break
	if above.position.y < 0: return below
	if below.end.y > view_height: return above
	return above if desired.position.y - above.position.y <= below.position.y - desired.position.y else below

func _draw_order_paths() -> void:
	var font := ThemeDB.fallback_font
	for path in game._order_paths():
		if not game.actors.has(path.id): continue
		var origin: Vector3 = game.actors[path.id].root.position
		var previous_screen: Vector2 = game.camera.unproject_position(origin)
		var previous_visible: bool = not game.camera.is_position_behind(origin)
		for index in path.points.size():
			var point: Dictionary = path.points[index]
			var world := Vector3(point.x / 256.0, 0.1, point.z / 256.0)
			var screen: Vector2 = game.camera.unproject_position(world)
			var visible: bool = not game.camera.is_position_behind(world)
			var color := Color("f2b86d") if point.order == 2 else Color("64e5df")
			if previous_visible and visible: draw_dashed_line(previous_screen, screen, color, 1.5, 8.0, true)
			if visible:
				draw_circle(screen, 9.0, Color("12252c"))
				draw_arc(screen, 9.0, 0, TAU, 24, color, 1.5, true)
				draw_string(font, screen + Vector2(-4, 4), str(index), HORIZONTAL_ALIGNMENT_LEFT, -1, 12, color)
			previous_screen = screen
			previous_visible = visible

func minimap_rect() -> Rect2:
	var view := get_viewport_rect().size
	return Rect2(view.x - 169, view.y - 174, 153, 153) if game.map_size.x == game.map_size.y else Rect2(view.x - 220, view.y - 174, 204, 153)

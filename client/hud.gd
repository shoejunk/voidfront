extends Control

var game: Node3D
var drag_from := Vector2.ZERO
var drag_to := Vector2.ZERO
var dragging := false
var title: Label
var status: Label
var selection: Label
var tip: Label
var result: Label
var connection: Label

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
	tip = _label(Vector2(36, 825), 14, Color("a4b9b6"))
	tip.text = "LMB / drag  select    RMB  move    A + click  attack-move    S  stop    H  hold\nF2  select army    arrows  camera    wheel  zoom    R  restart"
	result = _label(Vector2(590, 350), 34, Color("f0d19b"))
	result.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER

func _label(at: Vector2, font_size: int, color: Color) -> Label:
	var label := Label.new()
	label.position = at
	label.add_theme_font_size_override("font_size", font_size)
	label.add_theme_color_override("font_color", color)
	label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(label)
	return label

func _process(_delta: float) -> void:
	if not is_instance_valid(game) or game.current.is_empty():
		return
	var view := get_viewport_rect().size
	selection.position.y = view.y - 112
	tip.position.y = view.y - 70
	selection.text = "%02d  /  CAIRN STRIDERS" % game.selected.size()
	if game.attack_pending:
		selection.text += "     —     SELECT ATTACK DESTINATION"
	status.text = "THE GLASS REACH   /   FIELD TRIAL 01     •     %02d:%02d" % [int(game.current.tick / 1200), int(game.current.tick / 20) % 60]
	connection.text = ""
	tip.text = "LMB / drag  select    RMB  move    A + click  attack-move    S  stop    H  hold\nF2  select army    arrows  camera    wheel  zoom    R  restart"
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
		tip.text = "LMB / drag  select own units    RMB  move    A + click  attack-move    S  stop    H  hold\nF2  select army    arrows  camera    wheel  zoom    shared match — restart after session ends"
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
	if not game.option_error.is_empty():
		result.text = "INVALID LAUNCH OPTIONS\nR  /  start offline skirmish"
		connection.text = game.option_error
	queue_redraw()

func _draw() -> void:
	var view := get_viewport_rect().size
	var network_panel: bool = is_instance_valid(game) and (game.network or not game.option_error.is_empty())
	var panel_size := Vector2(920, 128) if network_panel else Vector2(548, 80)
	draw_rect(Rect2(Vector2(16, 12), panel_size), Color(0.025, 0.047, 0.055, 0.94))
	draw_rect(Rect2(16, 12, 3, panel_size.y), Color("61c9c6"))
	draw_rect(Rect2(16, view.y - 131, 965, 114), Color(0.025, 0.047, 0.055, 0.96))
	draw_line(Vector2(16, view.y - 131), Vector2(981, view.y - 131), Color("4b777b"), 1)
	if not is_instance_valid(game) or game.current.is_empty():
		return
	if not result.text.is_empty():
		var result_width := 740 if network_panel else 530
		draw_rect(Rect2((view.x - result_width) / 2, view.y / 2 - 48, result_width, 128), Color(0.025, 0.047, 0.055, 0.94))
	var map_rect := Rect2(view.x - 220, view.y - 174, 204, 153)
	draw_rect(map_rect.grow(6), Color("12252c"))
	draw_rect(map_rect, Color("27373b"))
	for cell in game.obstacle_cells:
		draw_rect(Rect2(map_rect.position + Vector2(cell.x, cell.y) * 6.375, Vector2.ONE * 6.375), Color("101b22"))
	for u in game.current.units:
		if u.hp <= 0:
			continue
		var color := Color("64e5df") if u.player == 0 else Color("fda06d")
		var at := map_rect.position + Vector2(u.x, u.z) / 256.0 * 6.375
		draw_circle(at, 2.5, color)
		if game.actors.has(u.id):
			var actor: Node3D = game.actors[u.id].root
			var screen: Vector2 = game.camera.unproject_position(actor.position + Vector3(0, 1.8, 0))
			if not game.camera.is_position_behind(actor.position):
				draw_rect(Rect2(screen - Vector2(18, 0), Vector2(36, 4)), Color("102128"))
				draw_rect(Rect2(screen - Vector2(18, 0), Vector2(36 * clampf(float(u.hp) / game.max_hp, 0, 1), 3)), color)
	if dragging:
		draw_rect(Rect2(drag_from, drag_to - drag_from).abs(), Color(0.3, 0.85, 0.83, 0.1))
		draw_rect(Rect2(drag_from, drag_to - drag_from).abs(), Color("64d8d0"), false, 1)

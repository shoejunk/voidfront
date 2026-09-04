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

func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	title = _label(Vector2(32, 22), 27, Color("edf1ea"))
	title.text = "V O I D F R O N T"
	status = _label(Vector2(32, 59), 13, Color("8eaaa8"))
	status.text = "THE GLASS REACH   /   CAIRN COMPACT"
	selection = _label(Vector2(36, 785), 22, Color("eaf1e9"))
	tip = _label(Vector2(36, 825), 14, Color("a4b9b6"))
	tip.text = "LMB / drag  select    RMB  move    A + click  attack-move    S  stop    H  hold\nF2  select army    arrows  camera    wheel  zoom    R  restart"
	result = _label(Vector2(590, 350), 34, Color("f0d19b"))

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
	result.position = Vector2(view.x / 2 - 240, view.y / 2 - 30)
	if game.current.winner == 0:
		result.text = "SECTOR SECURED\nR  /  deploy again"
	elif game.current.winner == 1:
		result.text = "SIGNAL LOST\nR  /  deploy again"
	elif game.current.winner == 2:
		result.text = "MUTUAL DESTRUCTION\nR  /  deploy again"
	else:
		result.text = ""
	queue_redraw()

func _draw() -> void:
	var view := get_viewport_rect().size
	draw_rect(Rect2(16, 12, 548, 80), Color(0.025, 0.047, 0.055, 0.94))
	draw_rect(Rect2(16, 12, 3, 80), Color("61c9c6"))
	draw_rect(Rect2(16, view.y - 131, 965, 114), Color(0.025, 0.047, 0.055, 0.96))
	draw_line(Vector2(16, view.y - 131), Vector2(981, view.y - 131), Color("4b777b"), 1)
	if not is_instance_valid(game) or game.current.is_empty():
		return
	if game.current.winner != -1:
		draw_rect(Rect2(view.x / 2 - 265, view.y / 2 - 48, 530, 128), Color(0.025, 0.047, 0.055, 0.94))
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

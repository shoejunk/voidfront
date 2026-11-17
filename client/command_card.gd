extends PanelContainer

signal command_pressed(action: int)

# The first eleven values match the bridge's canonical orders.
enum Action { STOP, MOVE, ATTACK, HOLD, GATHER, RETURN_CARGO, BUILD, STRIDER, CANCEL_UNIT, PLATING, LANCER, SELECT_WORKERS = 13, CANCEL_TARGET }

const LABELS := {
	Action.MOVE: "Move", Action.ATTACK: "Attack-move  [A]",
	Action.STOP: "Stop  [S]", Action.HOLD: "Hold  [H]",
	Action.GATHER: "Gather", Action.RETURN_CARGO: "Return cargo",
	Action.BUILD: "Build Foundry  [B]", Action.STRIDER: "Train Strider  [T]",
	Action.LANCER: "Train Lancer  [L]", Action.PLATING: "Hardened Plating  [G]",
	Action.CANCEL_UNIT: "Cancel last unit  [X]", Action.SELECT_WORKERS: "Select workers  [F1]",
	Action.CANCEL_TARGET: "Cancel  [Esc]",
}

var heading := Label.new()
var buttons := {}
var grid := GridContainer.new()
var feedback := Label.new()

func _ready() -> void:
	hide()
	mouse_filter = Control.MOUSE_FILTER_STOP
	var panel := _style(Color("102126"), Color("355453"))
	panel.border_width_top = 2
	panel.content_margin_left = 14
	panel.content_margin_right = 14
	panel.content_margin_top = 8
	panel.content_margin_bottom = 8
	add_theme_stylebox_override("panel", panel)
	var stack := VBoxContainer.new()
	add_child(stack)
	heading.add_theme_font_size_override("font_size", 18)
	heading.add_theme_color_override("font_color", Color("eaf1e9"))
	heading.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	heading.custom_minimum_size.x = 528
	stack.add_child(heading)
	grid.add_theme_constant_override("h_separation", 6)
	grid.add_theme_constant_override("v_separation", 5)
	stack.add_child(grid)
	var normal := _style(Color("20393e"), Color("527975"))
	var hover := _style(Color("30565a"), Color("81d5cd"))
	var pressed := _style(Color("315652"), Color("f1c781"))
	var disabled := _style(Color("192b2f"), Color("33484b"))
	for action in LABELS:
		var button := Button.new()
		button.hide()
		button.text = LABELS[action]
		button.focus_mode = Control.FOCUS_NONE
		button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		button.custom_minimum_size.y = 34
		button.add_theme_font_size_override("font_size", 15)
		button.add_theme_color_override("font_disabled_color", Color("95a7a5"))
		button.add_theme_stylebox_override("normal", normal)
		button.add_theme_stylebox_override("hover", hover)
		button.add_theme_stylebox_override("pressed", pressed)
		button.add_theme_stylebox_override("disabled", disabled)
		button.pressed.connect(command_pressed.emit.bind(action))
		grid.add_child(button)
		buttons[action] = button
	feedback.add_theme_font_size_override("font_size", 14)
	feedback.add_theme_color_override("font_color", Color("c4d9d4"))
	feedback.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	feedback.custom_minimum_size.x = 528
	stack.add_child(feedback)
	set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_LEFT)
	grow_vertical = Control.GROW_DIRECTION_BEGIN
	offset_left = 16
	offset_right = 572
	offset_bottom = -17

func _style(fill: Color, edge: Color) -> StyleBoxFlat:
	var box := StyleBoxFlat.new()
	box.bg_color = fill
	box.border_color = edge
	box.set_border_width_all(1)
	box.set_corner_radius_all(3)
	return box

func refresh(game, title: String, workers: int, cargo: int) -> void:
	var entity: Dictionary = game.selected_entity
	var structure: bool = entity.get("category", "") == "structure" and entity.get("player", -1) == game.local_player
	visible = game.commands_available() and (structure or not game.selected.is_empty())
	if not visible: return
	heading.text = title
	var actions: Array = []
	var hint: String = game.economy_notice if game.economy else ""
	var columns := 2
	if game.target_order != -1:
		actions = [Action.CANCEL_TARGET]
		match game.target_order:
			Action.MOVE: hint = "Click a destination. Hold Shift to queue movement."
			Action.ATTACK: hint = "Click an attack destination. Hold Shift to queue it."
			Action.GATHER: hint = "Click a visible salvage or flux deposit."
			Action.BUILD: hint = "Click a clear site near your anchor, or an unfinished Foundry to resume."
	elif structure:
		if entity.kind == 0:
			actions = [Action.SELECT_WORKERS]
			hint = "Workers deliver salvage and flux here."
		else:
			actions = [Action.STRIDER, Action.LANCER, Action.PLATING, Action.CANCEL_UNIT]
			_update_foundry(game.current, game.local_player, entity)
	else:
		actions = [Action.MOVE, Action.STOP, Action.HOLD]
		if game.selected.size() > workers: actions.append(Action.ATTACK)
		if workers > 0:
			columns = 3
			actions.append_array([Action.GATHER, Action.RETURN_CARGO, Action.BUILD])
			_set_button(Action.BUILD, "%d salvage" % game.current.foundry_cost, "", "Choose a site or resume an unfinished Foundry.")
			_set_button(Action.RETURN_CARGO, "", "No cargo to return." if cargo == 0 else "", "Deliver carried resources to your anchor.")
	grid.columns = columns
	for action in buttons:
		buttons[action].visible = action in actions
	feedback.text = hint
	feedback.visible = not hint.is_empty()
	offset_top = offset_bottom - get_combined_minimum_size().y

func _update_foundry(state: Dictionary, player: int, foundry: Dictionary) -> void:
	var unfinished := "Finish construction first." if foundry.build_ticks < state.build_duration else ""
	var train_reason := unfinished
	if train_reason.is_empty():
		if foundry.production_queue >= state.production_queue_limit: train_reason = "Production queue is full."
		elif state.population_used[player] + state.population_reserved[player] >= state.population_cap: train_reason = "Population limit reached."
	var salvage: int = state.salvage[player]
	var flux: int = state.flux[player]
	var reason := train_reason
	if reason.is_empty() and salvage < state.strider_cost: reason = "Gather more salvage."
	_set_button(Action.STRIDER, "%d salvage" % state.strider_cost, reason, "Queue a Strider at this Foundry.")
	reason = train_reason
	if reason.is_empty():
		if not state.researched[player]: reason = "Requires Hardened Plating."
		elif salvage < state.lancer_cost or flux < state.lancer_flux_cost: reason = "Gather more salvage or flux."
	_set_button(Action.LANCER, "%d salvage + %d flux" % [state.lancer_cost, state.lancer_flux_cost], reason, "Queue a ranged Lancer.")
	reason = unfinished
	var detail := "%d flux" % state.research_cost
	if state.researched[player]:
		reason = "Plating research is complete."
		detail = "Researched"
	elif state.research_ticks[player] > 0:
		reason = "Plating research is underway."
		detail = "Researching"
	elif reason.is_empty() and flux < state.research_cost: reason = "Gather more flux."
	_set_button(Action.PLATING, detail, reason, "Upgrade Striders and unlock Lancers.")
	reason = "Production queue is empty." if foundry.production_queue == 0 else ""
	_set_button(Action.CANCEL_UNIT, "Refund cost", reason, "Cancel the last queued unit for a full refund.")

func _set_button(action: int, detail: String, reason: String, help: String) -> void:
	var button: Button = buttons[action]
	button.text = LABELS[action] if detail.is_empty() else "%s\n%s" % [LABELS[action], detail]
	button.disabled = not reason.is_empty()
	button.tooltip_text = help if reason.is_empty() else reason

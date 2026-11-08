extends SceneTree

# Direct adapter contract regression, separate from rendered client evidence.
var a: RefCounted
var b: RefCounted
var phase := 0
var began := 0
var failures: Array[String] = []
var accepted := -1
var tail_checked := false
var local_port := 39170
var second_started_usec := 0
var first_reports: Array[Dictionary] = []

func check(value: bool, label: String) -> void:
	if not value: failures.append(label)

func _initialize() -> void:
	began = Time.get_ticks_msec()
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--port="): local_port = int(argument.trim_prefix("--port="))
	a = ClassDB.instantiate("VoidfrontBridge")
	b = ClassDB.instantiate("VoidfrontBridge")
	check(not a.network_start(2, local_port, local_port + 1, 99203, 2, 30), "invalid player rejects")
	check(a.network_start(0, local_port, local_port + 1, 99203, 2, 30), "player zero starts")
	check(a.network_issue(1, PackedInt32Array([1]), 2000, 2000, Time.get_ticks_usec()) == -1, "pre-ready input rejects")
	check(b.network_start(1, local_port + 1, local_port, 99203, 2, 30), "player one starts")

func _process(_delta: float) -> bool:
	if Time.get_ticks_msec() - began > 22000:
		failures.append("bridge regression watchdog")
		finish()
		return false
	a.network_poll(Time.get_ticks_usec())
	b.network_poll(Time.get_ticks_usec())
	var sa: Dictionary = a.network_status()
	var sb: Dictionary = b.network_status()
	if sa.state == "error" or sb.state == "error":
		failures.append("unexpected session error: %s / %s" % [sa.error, sb.error])
		finish()
		return false
	if phase == 0 and sa.ready and sb.ready:
		check(a.network_issue(1, PackedInt32Array([7]), 2000, 2000, Time.get_ticks_usec()) == -1, "enemy ownership rejects")
		check(a.network_issue(1, PackedInt32Array([999]), 2000, 2000, Time.get_ticks_usec()) == -1, "unknown unit rejects")
		check(a.network_issue(4, PackedInt32Array([1]), 2000, 2000, Time.get_ticks_usec()) == -1, "invalid order rejects")
		check(a.network_issue(1, PackedInt32Array([1]), -1, 2000, Time.get_ticks_usec()) == -1, "invalid destination rejects")
		accepted = a.network_issue(1, PackedInt32Array([1]), 2000, 2000, Time.get_ticks_usec())
		check(accepted == 1, "owned command accepts without rejection sequence holes")
		for sequence in range(2, 65):
			check(a.network_issue(1, PackedInt32Array([1]), 2000, 2000, Time.get_ticks_usec()) == sequence, "bounded queue accepts slot %d" % sequence)
		check(a.network_issue(1, PackedInt32Array([1]), 2000, 2000, Time.get_ticks_usec()) == -1, "full queue rejects slot 65")
		phase = 1
	if phase == 1 and sa.state == "complete" and sb.state == "complete":
		check(a.snapshot().hash == b.snapshot().hash, "paired bridge final state agrees")
		check(a.network_issue(0, PackedInt32Array([1]), 0, 0, Time.get_ticks_usec()) == -1, "terminal input rejects")
		var report: Dictionary = a.network_report()
		check(report.inputs.size() == 64, "rejected commands leave no timing or sequence holes")
		for index in range(report.inputs.size()):
			check(report.inputs[index].sequence == index + 1 and report.inputs[index].executed_usec > 0, "queued command executed once")
		check(tail_checked, "tail rejection exercised")
		first_reports = [a.network_report(), b.network_report()]
		second_started_usec = Time.get_ticks_usec()
		check(a.network_start(0, local_port, local_port + 1, 99204, 3, 12, true, 73), "completed player zero socket reusable")
		check(b.network_start(1, local_port + 1, local_port, 99204, 3, 12, true, 73), "completed player one socket reusable")
		for bridge in [a, b]:
			var fresh: Dictionary = bridge.network_report()
			check(fresh.state == "handshake" and fresh.tick == 0 and not fresh.ready, "fresh bridge starts before readiness")
			check(fresh.seed == 73 and fresh.units_per_team == 3 and fresh.input_delay == 3 and fresh.requested_ticks == 12, "fresh match changed settings")
			check(fresh.inputs.is_empty() and fresh.executed_inputs.is_empty() and fresh.trace.is_empty(), "fresh match clears input timing and trace")
			check(fresh.queued_inputs == 0 and fresh.replay_command_count == 0 and fresh.recorded_ticks == 0 and fresh.confirmed_ticks == 0, "fresh match clears recorded and queued state")
		check(a.snapshot().hash == b.snapshot().hash and a.snapshot().hash != sa.hash, "fresh initial states agree and replace completed state")
		check(a.network_issue(0, PackedInt32Array([1]), 0, 0, Time.get_ticks_usec()) == -1, "second match pre-ready input rejects")
		phase = 2
	elif phase == 2 and sa.ready and sb.ready:
		check(a.network_issue(0, PackedInt32Array([1]), 0, 0, Time.get_ticks_usec()) == 1, "second player zero sequence resets")
		check(b.network_issue(0, PackedInt32Array([4]), 0, 0, Time.get_ticks_usec()) == 1, "second player one sequence resets")
		phase = 3
	elif phase == 3 and sa.state == "complete" and sb.state == "complete":
		var second_a: Dictionary = a.network_report()
		var second_b: Dictionary = b.network_report()
		check(second_a.hash == second_b.hash and second_a.trace == second_b.trace and second_a.replay == second_b.replay, "second match has identical state trace and canonical replay")
		for next_report in [second_a, second_b]:
			check(next_report.tick == 12 and next_report.confirmed_ticks == 12 and next_report.recorded_ticks == 12 and next_report.evidence_complete, "second match confirms complete recorded prefix")
			check(next_report.inputs.size() == 1 and next_report.replay_command_count == 2 and next_report.queued_inputs == 0, "second match contains only fresh commands")
			for input in next_report.inputs:
				check(input.sequence == 1 and input.disposition == "executed" and input.executed_usec >= second_started_usec and input.input_usec >= second_started_usec, "second match fresh input executes with new timing")
				check(input.execution_tick == input.source_tick + 3, "second match uses new input delay")
		check(first_reports[0].trace.size() == 30 and first_reports[0].inputs.size() == 64 and first_reports[0].replay_command_count == 64, "first match evidence remains frozen")
		check(first_reports[1].trace.size() == 30 and first_reports[1].inputs.is_empty() and first_reports[1].replay_command_count == 64, "first peer evidence remains frozen")
		a.reset(1, true)
		b.reset(1, true)
		check(a.snapshot().tick == 0, "reset returns offline initial state")
		check(a.issue(0, PackedInt32Array([1]), 0, 0), "offline input retained")
		a.advance()
		check(a.snapshot().tick == 1, "offline stepping retained")
		finish()
	elif phase == 1 and sa.tick >= 28 and not tail_checked:
		tail_checked = true
		check(a.network_issue(0, PackedInt32Array([1]), 0, 0, Time.get_ticks_usec()) == -1, "unschedulable tail input rejects")
	return false

func finish() -> void:
	a.network_cancel()
	b.network_cancel()
	print(JSON.stringify({"ok": failures.is_empty(), "failures": failures, "kind": "same-process bridge contract"}))
	quit(0 if failures.is_empty() else 1)

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
	if Time.get_ticks_msec() - began > 12000:
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
	if sa.state == "complete" and sb.state == "complete":
		check(a.snapshot().hash == b.snapshot().hash, "paired bridge final state agrees")
		check(a.network_issue(0, PackedInt32Array([1]), 0, 0, Time.get_ticks_usec()) == -1, "terminal input rejects")
		var report: Dictionary = a.network_report()
		check(report.inputs.size() == 64, "rejected commands leave no timing or sequence holes")
		for index in range(report.inputs.size()):
			check(report.inputs[index].sequence == index + 1 and report.inputs[index].executed_usec > 0, "queued command executed once")
		check(a.network_start(0, local_port, local_port + 1, 99204, 2, 30), "completed socket reusable")
		a.reset(1, true)
		b.reset(1, true)
		check(a.snapshot().tick == 0, "reset returns offline initial state")
		check(a.issue(0, PackedInt32Array([1]), 0, 0), "offline input retained")
		a.advance()
		check(a.snapshot().tick == 1, "offline stepping retained")
		check(tail_checked, "tail rejection exercised")
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

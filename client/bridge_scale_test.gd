extends SceneTree

# Adapter contract only: real large-map Sim state, canonical input and reset.
# This does not certify crowd progress, rendered performance or networking.
var failures: Array[String] = []
var checks := 0

func check(value: bool, label: String) -> void:
	checks += 1
	if not value:
		failures.append(label)

func check_population(bridge: RefCounted, count: int) -> void:
	var state: Dictionary = bridge.snapshot()
	check(state.map_id == 1 and state.width == 128 and state.height == 128, "actual scale map metadata")
	check(state.units.size() == count * 2, "%d actual simulation units" % (count * 2))
	var populations := [0, 0]
	var occupied := {}
	for unit: Dictionary in state.units:
		check(unit.id > 0 and unit.id <= count * 2 and unit.hp == 100, "live canonical unit identity")
		check(unit.player == 0 or unit.player == 1, "valid owner")
		populations[unit.player] += 1
		check(unit.x > 256 and unit.x < 127 * 256 and unit.z > 256 and unit.z < 127 * 256, "unit inside map")
		check(unit.z > 24 * 256, "unit is outside old Foundry bounds")
		var position := Vector2i(unit.x, unit.z)
		check(not occupied.has(position), "distinct real unit positions")
		occupied[position] = true
	check(populations == [count, count], "exact player populations")
	check(not bridge.is_blocked(100, 64), "far open terrain accessible")
	check(bridge.is_blocked(64, 20) and not bridge.is_blocked(64, 64), "large-map ridge and passage")
	check(not bridge.is_blocked(15, 5), "Foundry ridge is absent on scale map")
	for cell in [Vector2i(-1, 50), Vector2i(50, -1), Vector2i(128, 50), Vector2i(50, 128)]:
		check(bridge.is_blocked(cell.x, cell.y), "outside map is blocked")

func _initialize() -> void:
	var bridge: RefCounted = ClassDB.instantiate("VoidfrontBridge")
	for count in [100, 250]:
		check(bridge.reset_scale(42, count, false), "scale reset accepts population")
		check_population(bridge, count)
		var initial: Dictionary = bridge.snapshot()
		check(initial.tick == 0, "reset begins at tick zero")
		check(initial.content_id.length() == 16 and initial.protocol == 4, "replay compatibility metadata")
		# The leading right edge of the friendly formation has open space to move.
		var id: int = count
		var before: Dictionary = initial.units[id - 1]
		check(bridge.issue(1, PackedInt32Array([id]), 100 * 256 + 128, 64 * 256 + 128), "canonical far-map Move accepted")
		var pending_hash: String = bridge.snapshot().hash
		for invalid in [[-1, count], [4294967296, count], [42, 0], [42, 251], [42, -1], [42, 4294967296]]:
			check(not bridge.reset_scale(invalid[0], invalid[1], true), "invalid scale setup rejects")
			check(bridge.snapshot().hash == pending_hash and bridge.snapshot().tick == 0, "invalid reset preserves queued command and state")
		for target in [Vector2i(128 * 256, 64 * 256), Vector2i(64 * 256, 128 * 256), Vector2i(-1, 64 * 256), Vector2i(64 * 256, -1)]:
			check(not bridge.issue(1, PackedInt32Array([id]), target.x, target.y), "out-of-map order rejects")
			check(bridge.snapshot().hash == pending_hash, "invalid destination preserves pending simulation")
		bridge.advance()
		var stepped: Dictionary = bridge.snapshot()
		check(stepped.tick == 1 and stepped.map_id == 1 and stepped.units.size() == count * 2, "advance preserves map and population")
		var moved: Dictionary = stepped.units[id - 1]
		check(moved.order == 1 and moved.moving and (moved.x != before.x or moved.z != before.z), "far-map command executes and moves real unit")
		check(bridge.reset_scale(42, count, false), "same scale reset succeeds")
		check(bridge.snapshot().hash == initial.hash, "scale reset restores exact initial simulation")
		check(bridge.issue(1, PackedInt32Array([id]), 100 * 256 + 128, 64 * 256 + 128), "reset clears input sequence")
		bridge.advance()
		check(bridge.snapshot().hash == stepped.hash, "reset command replay is deterministic")
	check(bridge.reset_scale(0, 1, false), "zero seed accepted")
	check(bridge.reset_scale(4294967295, 250, false), "maximum uint32 seed accepted")
	bridge.reset(42, false)
	var foundry: Dictionary = bridge.snapshot()
	check(foundry.map_id == 0 and foundry.width == 32 and foundry.height == 24 and foundry.units.size() == 12, "legacy reset remains Foundry six per team")
	check(bridge.is_blocked(15, 5) and bridge.is_blocked(100, 64), "legacy terrain restored")
	check(not bridge.issue(1, PackedInt32Array([1]), 100 * 256, 64 * 256), "Foundry rejects large-map target")
	check(bridge.issue(1, PackedInt32Array([1]), 10 * 256, 12 * 256), "legacy command accepts")
	bridge.advance()
	check(bridge.snapshot().tick == 1, "legacy advancement preserved")
	# Starting a single session exercises the mutation guard without handshake waits.
	var port := 39370
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--port="): port = int(argument.trim_prefix("--port="))
	check(bridge.network_start(0, port, port + 1, 99213, 2, 30), "session starts for reset guard")
	var online: Dictionary = bridge.snapshot()
	check(online.map_id == 0 and online.width == 32 and online.height == 24, "network snapshot reports active map")
	check(not bridge.reset_scale(42, 250, false), "active session rejects scale reset")
	check(bridge.network_status().state == "handshake" and bridge.snapshot().hash == online.hash, "rejected reset preserves active session")
	bridge.network_cancel()
	bridge.reset(42, false)
	print(JSON.stringify({"ok": failures.is_empty(), "checks": checks, "failures": failures, "kind": "headless scale bridge contract"}))
	quit(0 if failures.is_empty() else 1)

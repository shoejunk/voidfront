extends SceneTree

const Lobby = preload("res://network_lobby.gd")
const FRAME_BYTES := 28
const FRAME_MAGIC := 0x314c4656
const OFFER := 1
const ACCEPT := 2
const START := 3
const WAIT_MS := 2500

var base_port := 39210
var host: RefCounted
var guest: RefCounted
var starts_on_host: Array[Dictionary] = []
var starts_on_guest: Array[Dictionary] = []
var failures: Array[String] = []
var checks := 0

func _initialize() -> void:
	_run.call_deferred()

func _check(value: bool, label: String) -> bool:
	checks += 1
	if not value: failures.append(label)
	return value

func _poll_pair() -> void:
	var host_start: Dictionary = host.poll()
	var guest_start: Dictionary = guest.poll()
	if not host_start.is_empty(): starts_on_host.append(host_start)
	if not guest_start.is_empty(): starts_on_guest.append(guest_start)

func _until(predicate: Callable, pump: Callable, label: String) -> bool:
	var deadline := Time.get_ticks_msec() + WAIT_MS
	while Time.get_ticks_msec() < deadline:
		pump.call()
		if predicate.call(): return _check(true, label)
		await create_timer(0.01).timeout
	return _check(false, "%s (timed out, host error: %s, guest error: %s)" % [
		label, host.error if host else "", guest.error if guest else ""])

func _pump_for_a_while(pump: Callable) -> void:
	var deadline := Time.get_ticks_msec() + 125
	while Time.get_ticks_msec() < deadline:
		pump.call()
		await create_timer(0.01).timeout

func _open_pair(port: int) -> bool:
	host = Lobby.new()
	guest = Lobby.new()
	starts_on_host.clear()
	starts_on_guest.clear()
	if not _check(host.open(true, port), "host listens on %d" % port): return false
	if not _check(guest.open(false, port), "guest dials port %d" % port): return false
	return await _until(func(): return host.connected and guest.connected, _poll_pair,
		"both peers connected on %d" % port)

func _close_pair() -> void:
	host.cancel()
	guest.cancel()

func _check_round(index: int, seed_value: int, delay_value: int, tick_limit: int) -> void:
	var from_host := starts_on_host[index]
	var from_guest := starts_on_guest[index]
	_check(from_host == from_guest, "round %d settings agree" % (index + 1))
	_check(int(from_host.get("id", 0)) > 0, "round %d uses a positive identity" % (index + 1))
	_check(from_host.get("seed") == seed_value and from_host.get("delay") == delay_value
		and from_host.get("ticks") == tick_limit, "round %d retains offered settings" % (index + 1))

func _consent_and_rematch() -> void:
	if not await _open_pair(base_port):
		_close_pair()
		return
	_check(not guest.accept_match(), "guest cannot accept before an offer")
	_check(not guest.offer_match(7, 2, 12000), "guest cannot offer a match")
	_check(not host.accept_match(), "host cannot accept its own offer")
	_check(not host.offer_match(7, 17, 12000), "bad input delay cannot be offered")
	_check(not host.offer_match(7, 2, 100001), "bad tick limit cannot be offered")
	_check(host.offer_match(7, 2, 12000), "host offers first match")
	_check(not host.offer_match(91, 3, 9000), "host cannot replace a pending offer")
	var saw_offer := await _until(func(): return not guest.offer.is_empty(), _poll_pair,
		"first offer reaches guest")
	await _pump_for_a_while(_poll_pair)
	_check(starts_on_host.is_empty() and starts_on_guest.is_empty(),
		"first offer cannot start a match without guest consent")
	if not saw_offer or not _check(guest.accept_match(), "guest accepts first match"):
		_close_pair()
		return
	_check(not guest.accept_match(), "guest cannot accept a pending offer twice")
	if not await _until(func(): return starts_on_host.size() == 1 and starts_on_guest.size() == 1,
			_poll_pair, "both peers receive the first agreed start"):
		_close_pair()
		return
	_check_round(0, 7, 2, 12000)
	_check(host.active and guest.active, "both peers record the active match")
	_check(not host.offer_match(91, 3, 9000), "active host cannot replace its match")
	_check(not guest.accept_match(), "active guest cannot accept a second start")
	host.match_finished()
	_check(host.offer_match(91, 3, 9000), "host can offer a rematch after local completion")
	await _pump_for_a_while(_poll_pair)
	_check(not guest.accept_match(), "guest cannot accept rematch while its match is active")
	_check(starts_on_host.size() == 1 and starts_on_guest.size() == 1,
		"early offer leaves the guest's previous match running")
	guest.match_finished()
	if not await _until(func(): return not guest.offer.is_empty(), _poll_pair,
			"rematch offer survives different completion times"):
		_close_pair()
		return
	await _pump_for_a_while(_poll_pair)
	_check(starts_on_host.size() == 1 and starts_on_guest.size() == 1,
		"host's rematch request cannot restart the guest by itself")
	_check(guest.accept_match(), "guest explicitly accepts the rematch")
	if await _until(func(): return starts_on_host.size() == 2 and starts_on_guest.size() == 2,
			_poll_pair, "both peers receive a rematch start"):
		_check_round(1, 91, 3, 9000)
		_check(starts_on_host[1].id != starts_on_host[0].id,
			"rematch has a different identity on the same control connection")
		await _pump_for_a_while(_poll_pair)
		_check(starts_on_host.size() == 2 and starts_on_guest.size() == 2,
			"each agreed start is delivered exactly once")
		guest.cancel()
		await _until(func(): return not host.connected and not host.error.is_empty(), _poll_pair,
			"closing the active peer is observed by the other side")
		_check(not host.active, "disconnection clears active control state")
	_close_pair()

func _cancel_pending_offer() -> void:
	if await _open_pair(base_port + 1):
		_check(host.offer_match(7, 2, 12000), "host offers a match before cancellation")
		if await _until(func(): return not guest.offer.is_empty(), _poll_pair,
				"guest receives the offer before cancellation"):
			host.cancel()
			await _until(func(): return not guest.connected and not guest.error.is_empty(),
				_poll_pair, "guest observes host cancellation")
			_check(guest.offer.is_empty() and not guest.accept_match(),
				"guest cannot accept the cancelled offer")
			_check(starts_on_host.is_empty() and starts_on_guest.is_empty(),
				"cancelling a pending offer never starts a match")
	_close_pair()

func _fragmented_acceptance(port: int, variant: String) -> void:
	host = Lobby.new()
	var peer := StreamPeerTCP.new()
	if not _check(host.open(true, port), "host listens for wire contract test"): return
	if not _check(peer.connect_to_host("127.0.0.1", port) == OK, "test guest connects"):
		host.cancel()
		return
	var starts: Array[Dictionary] = []
	var pump := func():
		peer.poll()
		var start: Dictionary = host.poll()
		if not start.is_empty(): starts.append(start)
	if await _until(func(): return host.connected and peer.get_status() == StreamPeerTCP.STATUS_CONNECTED,
			pump, "wire contract peers connect"):
		_check(host.offer_match(7, 2, 12000), "host issues offer for wire contract")
		if await _until(func(): return peer.get_available_bytes() >= FRAME_BYTES,
				pump, "test guest reads complete offer"):
			var data: Array = peer.get_data(FRAME_BYTES)
			var frame: PackedByteArray = data[1]
			if _check(data[0] == OK and frame.size() == FRAME_BYTES
					and frame.decode_u32(0) == FRAME_MAGIC and frame.decode_u32(4) == OFFER,
					"offer has the expected fixed frame"):
				frame.encode_u32(4, ACCEPT)
				if variant == "wrong version":
					frame.encode_u32(0, FRAME_MAGIC ^ 1)
				elif variant == "different settings":
					frame.encode_u32(20, 3)
				_check(peer.put_data(frame.slice(0, 12)) == OK, "guest sends a frame prefix")
				await _pump_for_a_while(pump)
				_check(host.connected and host.error.is_empty() and starts.is_empty(),
					"a partial TCP frame cannot consume consent or trigger a start")
				_check(peer.put_data(frame.slice(12)) == OK, "guest finishes acceptance")
				if variant == "valid":
					if await _until(func(): return starts.size() == 1 and peer.get_available_bytes() >= FRAME_BYTES,
							pump, "completing a split acceptance starts the match"):
						var reply_data: Array = peer.get_data(FRAME_BYTES)
						var reply: PackedByteArray = reply_data[1]
						frame.encode_u32(4, START)
						_check(reply_data[0] == OK and reply == frame and host.active,
							"start replies with exactly the accepted settings")
				else:
					await _until(func(): return not host.error.is_empty(), pump,
						"host rejects acceptance with " + variant)
					_check(starts.is_empty() and not host.active, "rejected acceptance never yields a start")
	peer.disconnect_from_host()
	host.cancel()

func _run() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--port="): base_port = int(argument.trim_prefix("--port="))
	if _check(base_port >= 1024 and base_port <= 65531, "valid base port"):
		await _consent_and_rematch()
		await _cancel_pending_offer()
		await _fragmented_acceptance(base_port + 2, "wrong version")
		await _fragmented_acceptance(base_port + 3, "different settings")
		await _fragmented_acceptance(base_port + 4, "valid")
	print(JSON.stringify({"ok": failures.is_empty(), "checks": checks, "failures": failures,
		"kind": "same-process loopback TCP lobby contract"}))
	quit(0 if failures.is_empty() else 1)

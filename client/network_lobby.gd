extends RefCounted

# Loopback setup and consent travel over a single reliable stream. Gameplay
# traffic stays in the native UDP session, with its own compatibility checks.
const MAGIC := 0x314c4656 # VFL1, fixed Glass Reach economy rules.
const SIZE := 28
const OFFER := 1
const ACCEPT := 2
const START := 3

var connected := false
var active := false
var error := ""
var offer: Dictionary = {}
var _host := false
var accepted := false
var _last_id := 0
var _listener: TCPServer
var _peer: StreamPeerTCP

func open(as_host: bool, host_port: int) -> bool:
	_host = as_host
	if as_host:
		_listener = TCPServer.new()
		if _listener.listen(host_port, "127.0.0.1") != OK:
			_fail("Cannot listen for a peer on TCP port %d." % host_port)
	else:
		_peer = StreamPeerTCP.new()
		if _peer.connect_to_host("127.0.0.1", host_port) != OK:
			_fail("Cannot connect to the host.")
	return error.is_empty()

func cancel() -> void:
	if _peer: _peer.disconnect_from_host()
	if _listener: _listener.stop()
	_peer = null
	_listener = null
	connected = false
	active = false
	offer.clear()
	accepted = false

func _fail(message: String) -> void:
	error = message
	cancel()

func match_finished() -> void:
	active = false

func offer_match(seed: int, delay: int, ticks: int) -> bool:
	if not (_host and connected) or active or not offer.is_empty(): return false
	var id := Crypto.new().generate_random_bytes(8).decode_u64(0) & 0x7fffffffffffffff
	if id == 0 or id == _last_id:
		_fail("Could not generate a fresh match identity.")
		return false
	var setup := {"id": id, "seed": seed, "delay": delay, "ticks": ticks}
	if not _valid(setup): return false
	offer = setup
	return _send(OFFER)

func accept_match() -> bool:
	if _host or not connected or active or offer.is_empty() or accepted: return false
	accepted = true
	return _send(ACCEPT)

func _valid(setup: Dictionary) -> bool:
	return setup.id > 0 and setup.seed >= 1 and setup.seed <= 999999 \
		and setup.delay >= 1 and setup.delay <= 16 and setup.ticks >= 1 and setup.ticks <= 100000

func _send(kind: int) -> bool:
	var frame := PackedByteArray()
	frame.resize(SIZE)
	frame.encode_u32(0, MAGIC)
	frame.encode_u32(4, kind)
	frame.encode_u64(8, offer.id)
	frame.encode_u32(16, offer.seed)
	frame.encode_u32(20, offer.delay)
	frame.encode_u32(24, offer.ticks)
	# A single small frame is sent per user action or accepted offer. Avoid a
	# blocking write in the render loop; a failed/partial write closes the lobby.
	var result := _peer.put_partial_data(frame)
	if result[0] != OK or result[1] != SIZE:
		_fail("Could not send match setup to the peer.")
		return false
	return true

func poll() -> Dictionary:
	if not _peer and _listener and _listener.is_connection_available():
		_peer = _listener.take_connection()
		if _peer:
			_listener.stop()
			_listener = null
	if not _peer: return {}
	if _peer.poll() != OK or _peer.get_status() in [StreamPeerTCP.STATUS_NONE, StreamPeerTCP.STATUS_ERROR]:
		_fail("Peer left or could not connect. Both players must reopen the lobby.")
		return {}
	if _peer.get_status() != StreamPeerTCP.STATUS_CONNECTED: return {}
	if not connected:
		_peer.set_no_delay(true)
		connected = true
	if _peer.get_available_bytes() < SIZE: return {}
	var packet := _peer.get_data(SIZE)
	if packet[0] != OK:
		_fail("Could not read match setup.")
		return {}
	var frame: PackedByteArray = packet[1]
	var kind := frame.decode_u32(4)
	var setup := {"id": frame.decode_u64(8), "seed": frame.decode_u32(16), "delay": frame.decode_u32(20), "ticks": frame.decode_u32(24)}
	if frame.decode_u32(0) != MAGIC or not _valid(setup) or setup.id == _last_id:
		_fail("Invalid or stale match setup.")
		return {}
	if not _host and kind == OFFER and offer.is_empty():
		# The host can finish confirming slightly before the guest. Show/store
		# the offer, but don't let the guest accept until their match is archived.
		offer = setup
		return {}
	if active or offer != setup or (_host and kind != ACCEPT) or (not _host and (kind != START or not accepted)):
		_fail("Unexpected match setup message.")
		return {}
	if _host and not _send(START): return {}
	active = true
	_last_id = setup.id
	offer.clear()
	accepted = false
	return setup

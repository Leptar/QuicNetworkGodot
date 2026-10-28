extends Node
class_name NetworkProber

signal rtt_updated(current_rtt_ms: float, smoothed_rtt_ms: float, packet_loss_percent: float)

# Configuration du test
@export var ping_interval: float = 0.1 # 10 pings par seconde
@export var log_to_file: bool = true

const PACKET_TYPE_PING = 254 # ID unique pour nos paquets de test
const BUFFER_SIZE = 100

# Variables d'état
var _timer: float = 0.0
var _current_sequence: int = 0
var _highest_received_sequence: int = 0
var _packets_received: int = 0
var _file: FileAccess

# Tampon circulaire pour le RTT lissé
var _rtt_buffer: Array[float] = []
var _buffer_index: int = 0
var _current_sum: float = 0.0
var _filled_count: int = 0

func _ready() -> void:
	_rtt_buffer.resize(BUFFER_SIZE)
	_rtt_buffer.fill(0.0)
	
	# On s'abonne aux paquets bruts depuis NetSocketManager
	NetSocketManager.raw_packet_received.connect(_on_raw_packet_received)
	
	if not NetSocketManager.is_server and log_to_file:
		var time_str = Time.get_time_string_from_system().replace(":", "-")
		var filename = "user://network_benchmark_%s.csv" % time_str
		_file = FileAccess.open(filename, FileAccess.WRITE)
		if _file:
			_file.store_line("timestamp_ms,sequence,rtt_ms,smoothed_rtt_ms,packet_loss_percent")
			print("[NetworkProber] Enregistrement démarré : ", filename)

func _process(delta: float) -> void:
	if NetSocketManager.is_server:
		return # Le serveur ne fait qu'écouter et répondre
		
	if NetSocketManager.local_peer_id == 1:
		return # Pas encore connecté au serveur (ou on est en train de fermer)
		
	_timer += delta
	if _timer >= ping_interval:
		_timer = 0.0
		_send_ping()

# ==========================================
# ENVOI ET RÉCEPTION
# ==========================================
func _send_ping() -> void:
	_current_sequence += 1
	
	var buffer = StreamPeerBuffer.new()
	buffer.put_u8(PACKET_TYPE_PING)
	buffer.put_u32(_current_sequence)
	buffer.put_u64(Time.get_ticks_usec())
	
	# On envoie toujours nos pings en mode NON-FIABLE (UDP) pour voir les vraies pertes
	NetSocketManager.send_packet(1, buffer.data_array, false)

func _on_raw_packet_received(sender_id: int, payload: PackedByteArray, is_reliable: bool) -> void:
	var buffer = StreamPeerBuffer.new()
	buffer.data_array = payload
	
	if buffer.get_u8() == PACKET_TYPE_PING:
		if NetSocketManager.is_server :
			# EFFET MIROIR : Le serveur renvoie exactement le même paquet
			NetSocketManager.send_packet(sender_id, payload, false)
		else:
			# ANALYSE : Le client reçoit son propre ping en retour
			_process_pong(buffer)


func _process_pong(buffer: StreamPeerBuffer) -> void:
	var sequence = buffer.get_u32()
	var send_time_usec = buffer.get_u64()
	var current_time_usec = Time.get_ticks_usec()

	var rtt_ms = (current_time_usec - send_time_usec) / 1000.0

	_current_sum -= _rtt_buffer[_buffer_index]
	_rtt_buffer[_buffer_index] = rtt_ms
	_current_sum += rtt_ms
	_buffer_index = (_buffer_index + 1) % BUFFER_SIZE
	
	if _filled_count < BUFFER_SIZE:
		_filled_count += 1
		
	var smoothed_rtt = _current_sum / _filled_count
	
	_packets_received += 1
	if sequence > _highest_received_sequence:
		_highest_received_sequence = sequence
		
	var expected_packets = _highest_received_sequence
	var loss_percent = 0.0
	if expected_packets > 0:
		loss_percent = (1.0 - (float(_packets_received) / float(expected_packets))) * 100.0
	
	rtt_updated.emit(rtt_ms, smoothed_rtt, loss_percent)
	
	if _file:
		_file.store_line("%d,%d,%.2f,%.2f,%.1f" % [Time.get_ticks_msec(), sequence, rtt_ms, smoothed_rtt, loss_percent])

func _exit_tree() -> void:
	if _file:
		_file.close()

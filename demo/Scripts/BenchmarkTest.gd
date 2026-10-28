extends Node

var udp_server = NetworkBenchmark.new()
var udp_client = NetworkBenchmark.new()

var tcp_server = NetworkBenchmarkTCP.new()
var tcp_client = NetworkBenchmarkTCP.new()

var timer: float = 0.0

func _ready():
	# Setup UDP (port 9998)
	udp_server.start_server(9998)
	udp_client.start_client("127.0.0.1", 9998)

	# Setup TCP (port 9999)
	tcp_server.start_server(9999)
	tcp_client.start_client("127.0.0.1", 9999)

func _process(delta: float):
	timer += delta
	if timer >= 0.016: # ~60 Hz
		timer = 0.0
		udp_client.send_ping()
		tcp_client.send_ping()

	# Polling des deux backends
	udp_server.poll()
	udp_client.poll()

	tcp_server.poll()
	tcp_client.poll()

	if Engine.get_process_frames() % 60 == 0:
		print("UDP -> RTT: %.3f ms | Jitter: %.3f ms" % [udp_client.get_last_rtt_ms(), udp_client.get_jitter_ms()])
		print("TCP -> RTT: %.3f ms | Jitter: %.3f ms" % [tcp_client.get_last_rtt_ms(), tcp_client.get_jitter_ms()])
		print("--------------------------------------------------")

func _exit_tree():
	udp_client.close()
	udp_server.close()
	tcp_client.close()
	tcp_server.close()

extends Node2D
var server = QuicServer.new()
var client = QuicClient.new()

func _ready() -> void:
	add_child(server)
	server.peer_connected.connect(func(id): server.send_packet(id, 0, "Bienvenue Joueur !".to_utf8_buffer(), true))
	server.packet_received.connect(func(id, channel, data, rel): print("Reçu: ", data.get_string_from_utf8()))
	server.start(4433, "res://certs/server.crt", "res://certs/server.key")

	add_child(client)
	client.packet_received.connect(func(channel, data, rel): print("Client a reçu: ", data.get_string_from_utf8()))
	client.connected.connect(func(): 
		print("Connexion OK !")
		client.send_packet(0, "Coucou Serveur !".to_utf8_buffer(), true) # Mode Fiable
		client.send_packet(1, "Ma position !".to_utf8_buffer(), false)  # Mode Datagramme
	)
	client.connect_to_host("127.0.0.1", 4433)
	
	#await get_tree().create_timer(2.0).timeout
	#client.disconnect_from_host()

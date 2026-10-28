extends Node

const PLAYER = preload("uid://n10r0frmosww")
#var enet_peer := ENetMultiplayerPeer.new()

var PORT = 9999
var IP_ADDRESS = "127.0.0.1"

func start_server():
	NetSocketManager.start_server(PORT)
	_connect_signals()
	#enet_peer.create_server(PORT)
	#multiplayer.multiplayer_peer = enet_peer
	#multiplayer.peer_connected.connect(add_player)
	#multiplayer.peer_disconnected.connect(remove_player)
	
func join_server() -> bool:
	if NetSocketManager.join_server(IP_ADDRESS, PORT) :
		_connect_signals()
		return true
	return false;
	#enet_peer.create_client(IP_ADDRESS, PORT)
	#multiplayer.peer_connected.connect(add_player)
	#multiplayer.peer_disconnected.connect(remove_player)
	#multiplayer.connected_to_server.connect(on_connected_to_server)
	#multiplayer.multiplayer_peer = enet_peer
	
func _connect_signals():
	if not NetSocketManager.peer_connected.is_connected(_on_peer_connected):
		NetSocketManager.peer_connected.connect(_on_peer_connected)
	if not NetSocketManager.peer_disconnected.is_connected(_on_peer_disconnected):
		NetSocketManager.peer_disconnected.connect(_on_peer_disconnected)

func _on_peer_connected(peer_id: int):
	if NetSocketManager.is_server:
		add_player(peer_id)
		
		for existing_id in NetSocketManager.peers.keys():
			if existing_id != peer_id:
				CustomRPC.send_rpc(existing_id, self, "add_player", [peer_id])
				CustomRPC.send_rpc(peer_id, self, "add_player", [existing_id])
		Global.multiplayer_spawner.sync_to_new_peer(peer_id)
	else:
		if peer_id == 1:
			add_player(NetSocketManager.local_peer_id)

func _on_peer_disconnected(peer_id: int):
	if NetSocketManager.is_server:
		remove_player(peer_id)
		# Le serveur dit à tout le monde de supprimer ce joueur
		for existing_id in NetSocketManager.peers.keys():
			CustomRPC.send_rpc(existing_id, self, "remove_player", [peer_id])

#func on_connected_to_server():
#	add_player(multiplayer.get_unique_id())

func add_player(peer_id: int):
	if peer_id == 1 and NetSocketManager.is_server:
		return
		
	var new_player = PLAYER.instantiate()
	
	new_player.name = str(peer_id)
	
	var rand_x = randf_range(-5.0, 5.0)
	var rand_z = randf_range(-5.0, 5.0)
	new_player.position = Vector3(rand_x, 1.0, rand_z)
	
	get_tree().current_scene.add_child(new_player, true)

func remove_player(peer_id: int):
	var players: Array[Node] = get_tree().get_nodes_in_group("Players")
	var player_to_remove = players.find_custom(func(item): return item.name == str(peer_id))
	if player_to_remove != -1:
		players[player_to_remove].queue_free()

func leave_server():
	#multiplayer.multiplayer_peer.close()
	#multiplayer.multiplayer_peer = null
	NetSocketManager.leave_server()
	clean_up_signals()
	get_tree().reload_current_scene()
	

func clean_up_signals():
	if NetSocketManager.peer_connected.is_connected(_on_peer_connected):
		NetSocketManager.peer_connected.disconnect(_on_peer_connected)
	if NetSocketManager.peer_disconnected.is_connected(_on_peer_disconnected):
		NetSocketManager.peer_disconnected.disconnect(_on_peer_disconnected)
	#multiplayer.peer_connected.disconnect(add_player)
	#multiplayer.peer_disconnected.disconnect(remove_player)
	#multiplayer.connected_to_server.disconnect(on_connected_to_server)

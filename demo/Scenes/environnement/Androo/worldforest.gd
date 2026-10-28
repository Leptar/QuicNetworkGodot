extends Node3D

@onready var spawn_container: Node3D = %SpawnContainer
@onready var custom_multiplayer_spawner: CustomMultiplayerSpawner = $CustomMultiplayerSpawner

func _ready() -> void:
	Global.forest = self
	Global.spawn_container = spawn_container
	Global.multiplayer_spawner = custom_multiplayer_spawner
	

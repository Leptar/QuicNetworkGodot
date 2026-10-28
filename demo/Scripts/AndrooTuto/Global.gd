extends Node

var forest: Node3D
var spawn_container: Node3D
var multiplayer_spawner: CustomMultiplayerSpawner

const BALL = preload("uid://xdlalboxmbew")

#@rpc("any_peer")
#func shoot_ball(pos, dir, force):
#	var new_ball: RigidBody3D = BALL.instantiate()
#	new_ball. position = pos + Vector3(0, 1.0, 0) + (dir * 2.0)
#	spawn_container.add_child(new_ball, true)
#	new_ball.apply_central_impulse(dir * force)

func shoot_ball(pos: Vector3, dir: Vector3, force: float) -> void:
	if not NetSocketManager.is_server:
		return

	var new_ball: RigidBody3D = BALL.instantiate()
	
	new_ball.name = "Ball_" + str(Time.get_ticks_usec())
	
	new_ball.position = pos + Vector3(0, 1.0, 0) + (dir * 2.0)
	
	spawn_container.add_child(new_ball, true)
	
	new_ball.apply_central_impulse(dir * force)

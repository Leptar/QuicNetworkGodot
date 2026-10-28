extends CharacterBody3D

@onready var camera_3d: Camera3D = %Camera3D
@onready var head: Node3D = %Head
@onready var nameplate: Label3D = $nameplate
@onready var menu: Control = %Menu
@onready var button_leave: Button = %ButtonLeave

@export var sensitivity: float = 0.002

const SPEED = 5.0
const JUMP_VELOCITY = 4.5

func is_local_authority() -> bool:
	# Dans notre implémentation, le nom du noeud EST le Peer ID
	return name == str(NetSocketManager.local_peer_id)

#func _enter_tree() -> void:
#	set_multiplayer_authority(int(name))

func _unhandled_input(event: InputEvent) -> void:
	if not is_local_authority(): #is_multiplayer_authority(): 
		return
	
	if event is InputEventMouseMotion:
		head.rotate_y(-event.relative.x * sensitivity)
		camera_3d.rotate_x(-event.relative.y * sensitivity)
		camera_3d.rotation.x = clamp(camera_3d.rotation.x, deg_to_rad(-90), deg_to_rad(90))

func _ready() -> void:
	menu.hide()
	add_to_group("Players")
	nameplate.text = name
	
	if not is_local_authority(): #is_multiplayer_authority(): 
		set_process(false)
		set_physics_process(false)
		return

	camera_3d.current = true
	Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)
	button_leave.pressed.connect(func(): Network.leave_server())

func _physics_process(delta: float) -> void:
	# Add the gravity.
	if not is_on_floor():
		velocity += get_gravity() * delta

	# Handle jump.
	if Input.is_action_just_pressed("jump") and is_on_floor():
		velocity.y = JUMP_VELOCITY

	# Get the input direction and handle the movement/deceleration.
	# As good practice, you should replace UI actions with custom gameplay actions.
	var input_dir := Input.get_vector("left", "right", "forward", "backward")
	var direction := (head.transform.basis * Vector3(input_dir.x, 0, input_dir.y)).normalized()
	if direction:
		velocity.x = direction.x * SPEED
		velocity.z = direction.z * SPEED
	else:
		velocity.x = move_toward(velocity.x, 0, SPEED)
		velocity.z = move_toward(velocity.z, 0, SPEED)

	move_and_slide()

func _process(_delta: float) -> void:
	if Input.is_action_just_pressed("menu") and menu.visible == false:
		Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)
		menu.show()
	elif Input.is_action_just_pressed("menu") and menu.visible == true:
		Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)
		menu.hide()
	
	if  Input.is_action_just_pressed("shoot") :
		shoot()
		

func shoot():
	var facing_dir = -head.transform.basis.z
	var force = 50
	var pos = global_position
	CustomRPC.send_rpc(1, Global, "shoot_ball", [pos, facing_dir, force])
	#Global.shoot_ball.rpc_id(1, pos, facing_dir, force)
	

extends CanvasLayer

@onready var button_host: Button = $Control/Center/MarginContainer/VBoxContainer/ButtonHost
@onready var button_join: Button = %ButtonJoin
@onready var button_quit: Button = %ButtonQuit
@onready var ip: LineEdit = $Control/Center/MarginContainer/VBoxContainer/IP
@onready var port: LineEdit = $Control/Center/MarginContainer/VBoxContainer/Port
@onready var control: Control = $Control
@onready var network_prober_ui: CanvasLayer = %NetworkProberUI
@onready var rtt: Label = %RTT
@onready var srtt: Label = %SRTT
@onready var jitter: Label = %Jitter

const WORLDFOREST = preload("uid://8xvcm3go1si3")
const PLAYER = preload("uid://n10r0frmosww")
const NETWORK_PROBER = preload("uid://b5il872qm2272")

func _ready() -> void:
	network_prober_ui.hide()
	button_join.pressed.connect(on_join)
	button_quit.pressed.connect(on_quit)
	button_host.pressed.connect(on_host)
	var network_prober: NetworkProber = NETWORK_PROBER.new()
	network_prober_ui.add_child(network_prober)
	network_prober.rtt_updated.connect(_on_rtt_updated)
	
	var arguments = get_parsed_arguments()
	print(arguments)
	if arguments.has("server"):
		var arg_port = arguments.get("server")
		Network.PORT = arg_port.to_int()
		on_host()

func on_host():
	Network.start_server()
	add_world()
	control.hide()

func on_join():
	if !ip.text.is_empty() : Network.IP_ADDRESS = ip.text
	if !port.text.is_empty() : Network.PORT = port.text.to_int()
	if Network.join_server() :
		add_world()
		control.hide()
		network_prober_ui.show()

func on_quit():
	get_tree().quit()

func add_world():
	var world = WORLDFOREST.instantiate()
	get_tree().current_scene.add_child.call_deferred(world)
	

func _on_rtt_updated(current_rtt_ms: float, smoothed_rtt_ms: float, packet_loss_percent: float):
	rtt.text = "RTT : %.1f ms" % current_rtt_ms
	srtt.text = "Lissé : %.1f ms" % smoothed_rtt_ms
	jitter.text = "Pertes : %.1f %%" % packet_loss_percent
	
	

func get_parsed_arguments() -> Dictionary:
	var argument_dict = {}
	print(OS.get_cmdline_args())
	for argument in OS.get_cmdline_args():
		if argument.contains("="):
			var parts = argument.split("=")
			if parts.size() == 2:
				var key = parts[0].lstrip("--")
				var value = parts[1]
				argument_dict[key] = value
				
	return argument_dict

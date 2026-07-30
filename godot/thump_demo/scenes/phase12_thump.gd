extends Node3D

@onready var host: DrossWorldHost = $DrossWorldHost
@onready var mouse_view: MeshInstance3D = $FieldMouse
@onready var door_view: Node3D = $SideDoor
@onready var status: Label = $UI/Status


func _ready() -> void:
	var thump := DrossAbilityDefinition.new()
	thump.ability_id = "thump_demo:thump"
	thump.range = 1
	thump.action_point_cost = 2
	thump.damage = 3
	thump.presentation_cue = "thump_demo:thump"
	if not host.start_thump_scenario(thump):
		status.text = "Failed to compile Thump"
	var door := DrossDoorDefinition.new()
	door.door_id = "thump_demo:side_door"
	door.region_id = "thump_demo:room"
	door.from_q = 0
	door.from_r = 0
	door.to_q = 1
	door.to_r = 0
	if not host.start_door_scenario(door):
		status.text = "Failed to compile side door"


func _unhandled_input(event: InputEvent) -> void:
	if event.is_action_pressed("ui_accept") and mouse_view.visible:
		if host.perform_thump():
			status.text = "THUMP!  Mouse HP: %d  Cue: %s" % [
				host.get_mouse_health(), host.get_last_presentation_cue()
			]
			if host.is_mouse_killed():
				mouse_view.visible = false
	if event is InputEventKey and event.pressed and not event.echo and event.keycode == KEY_D:
		var committed := host.close_door() if host.is_door_open() else host.open_door()
		if committed:
			door_view.rotation_degrees.y = 90.0 if host.is_door_open() else 0.0
			host.acknowledge_door_presentation(
					host.get_door_presentation_acknowledgement_id())
			status.text = "Door %s — edge %s" % [
				"open" if host.is_door_open() else "closed",
				"traversable" if host.is_door_edge_traversable() else "blocked"
			]

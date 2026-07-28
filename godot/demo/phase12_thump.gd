extends Node3D

@onready var host: DrossWorldHost = $DrossWorldHost
@onready var mouse_view: MeshInstance3D = $FieldMouse
@onready var status: Label = $UI/Status


func _ready() -> void:
	var thump := DrossAbilityDefinition.new()
	thump.ability_id = "dross_demo:thump"
	thump.range = 1
	thump.action_point_cost = 2
	thump.damage = 3
	thump.presentation_cue = "dross_demo:thump"
	if not host.start_thump_scenario(thump):
		status.text = "Failed to compile Thump"


func _unhandled_input(event: InputEvent) -> void:
	if event.is_action_pressed("ui_accept") and mouse_view.visible:
		if host.perform_thump():
			status.text = "THUMP!  Mouse HP: %d  Cue: %s" % [
				host.get_mouse_health(), host.get_last_presentation_cue()
			]
			if host.is_mouse_killed():
				mouse_view.visible = false

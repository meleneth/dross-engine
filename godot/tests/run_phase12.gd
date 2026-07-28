extends SceneTree

var failures: Array[String] = []


func check(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)


func _initialize() -> void:
	check(ClassDB.class_exists("DrossAbilityDefinition"), "ability definition is not registered")
	var thump := DrossAbilityDefinition.new()
	thump.ability_id = "dross_demo:thump"
	thump.range = 1
	thump.action_point_cost = 2
	thump.damage = 3
	thump.presentation_cue = "dross_demo:thump"
	check(thump.is_valid(), "typed Thump definition did not compile")

	thump.damage = 0
	check(not thump.is_valid(), "zero-damage ability definition was accepted")
	thump.damage = 3
	thump.ability_id = "Thump"
	check(not thump.is_valid(), "non-canonical ability identity was accepted")
	thump.ability_id = "dross_demo:thump"

	var host := DrossWorldHost.new()
	get_root().add_child(host)
	check(host.start_thump_scenario(thump), "Godot combat host rejected typed Thump")
	check(host.get_mouse_health() == 3, "mouse did not start with authoritative health")
	check(host.perform_thump(), "generic PerformAbility rejected Thump")
	check(host.get_mouse_health() == 0, "Thump presentation preceded committed damage")
	check(host.is_mouse_killed(), "lethal Thump did not commit mouse death")
	check(host.get_last_presentation_cue() == "dross_demo:thump",
			"committed Thump did not expose its presentation cue")
	host.queue_free()

	var demo: Node3D = load("res://demo/phase12_thump.tscn").instantiate()
	get_root().add_child(demo)
	await process_frame
	check(demo.get_node("FieldMouse").visible, "field mouse view did not start visible")
	demo.get_node("DrossWorldHost").perform_thump()
	check(demo.get_node("DrossWorldHost").is_mouse_killed(),
			"viewable demo did not use committed combat state")
	demo.queue_free()

	if failures.is_empty():
		print("phase12 ability resource ok")
		quit(0)
	else:
		for failure in failures:
			push_error(failure)
		quit(1)

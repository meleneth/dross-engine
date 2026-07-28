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
	var mouse_module := DrossScriptModuleDefinition.new()
	mouse_module.module_id = "demo:field_mouse"
	mouse_module.scope_kind = 1
	mouse_module.entity_sequence = 2
	mouse_module.state_schema_version = 1
	mouse_module.script = load("res://scripts/phase12_field_mouse.gd")
	var mouse_modules: Array[DrossScriptModuleDefinition] = [mouse_module]
	check(host.start_script_scenario(mouse_modules, 12345), "field mouse script failed to install")
	check(host.start_thump_scenario(thump), "Godot combat host rejected typed Thump")
	check(host.get_mouse_health() == 3, "mouse did not start with authoritative health")
	check(host.perform_thump(), "generic PerformAbility rejected Thump")
	check(host.get_script_state_bool("demo:field_mouse", 2, "rule_checked"),
			"field mouse did not contribute a typed pre-resolution ability rule")
	check(host.get_mouse_health() == 0, "Thump presentation preceded committed damage")
	check(host.is_mouse_killed(), "lethal Thump did not commit mouse death")
	check(host.get_last_presentation_cue() == "dross_demo:thump",
			"committed Thump did not expose its presentation cue")
	check(host.get_script_state_bool("demo:field_mouse", 2, "attacked"),
			"field mouse did not react to typed DamageApplied")
	check(host.get_script_state_bool("demo:field_mouse", 2, "killed"),
			"field mouse did not react to typed ActorKilled")
	check(host.get_script_state_int("demo:field_mouse", 2, "reaction_roll") >= 0,
			"field mouse reaction did not use deterministic RandomHub access")
	host.queue_free()

	var door_definition := DrossDoorDefinition.new()
	door_definition.door_id = "demo:side_door"
	door_definition.region_id = "demo:room"
	door_definition.from_q = 0
	door_definition.from_r = 0
	door_definition.to_q = 1
	door_definition.to_r = 0
	check(door_definition.is_valid(), "typed edge-anchored door did not compile")
	var door_host := DrossWorldHost.new()
	get_root().add_child(door_host)
	check(door_host.start_door_scenario(door_definition), "door scenario did not start")
	check(not door_host.is_door_edge_traversable(), "closed door allowed edge traversal")
	check(door_host.open_door(), "OpenDoor was rejected")
	check(door_host.is_door_open(), "OpenDoor did not commit state")
	check(door_host.is_door_edge_traversable(), "open door still blocked traversal")
	var door_ack := door_host.get_door_presentation_acknowledgement_id()
	check(door_host.is_door_presentation_pending(), "door presentation gate was not opened")
	check(not door_host.acknowledge_door_presentation(door_ack + 1),
			"wrong door presentation acknowledgement was accepted")
	check(door_host.is_door_open(), "wrong acknowledgement changed committed door state")
	check(not door_host.advance_door_presentation(), "door presentation timed out too early")
	check(door_host.advance_door_presentation(), "missing door animation did not time out")
	check(not door_host.is_door_presentation_pending(), "timeout did not release presentation gate")
	check(door_host.is_door_open(), "presentation timeout changed committed door state")
	check(not door_host.acknowledge_door_presentation(door_ack),
			"late door presentation acknowledgement was accepted")
	check(door_host.close_door(), "CloseDoor was rejected")
	check(not door_host.is_door_edge_traversable(), "closed door lost traversal contribution")
	door_host.queue_free()

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

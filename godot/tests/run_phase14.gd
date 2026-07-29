extends SceneTree

var failures: Array[String] = []


func check(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)


func _initialize() -> void:
	var demo: Node3D = load("res://demo/phase14_vertical_slice.tscn").instantiate()
	get_root().add_child(demo)
	await process_frame

	var overlay: DrossHexGridOverlay3D = demo.get_node("GridOverlay")
	var compiled_map: DrossCompiledHexMap = demo.get_node(
			"DrossWorldHost").get_movement_compiled_map()
	check(compiled_map.get_cell_count() == 4,
			"integrated overlay did not receive the authoritative movement map")
	check(overlay.get_cell_keys() == compiled_map.get_cell_keys(),
			"runtime grid differs from the authoritative movement map")
	check(demo.preview_destination(2), "integrated demo did not preview a reachable path")
	check(overlay.path_cell_keys == PackedStringArray([
		"dross:phase11:0,0,0",
		"dross:phase11:1,0,0",
		"dross:phase11:2,0,0",
	]), "path preview did not highlight authoritative map cells")
	check(demo.request_previewed_move(), "integrated demo rejected MoveTo")
	for tick in range(4):
		check(demo.advance_authoritative_tick(), "integrated movement tick failed")
	check(demo.get_node("DrossWorldHost").get_movement_tick() == 4,
			"diagnostic tick did not follow authoritative fixed ticks")
	check(demo.get_node("DrossWorldHost").get_movement_column() == 2,
			"integrated player did not reach the authoritative destination")
	check(is_equal_approx(demo.get_node("Player").position.x, sqrt(3.0) * 2.0),
			"player view did not follow the authoritative cell")
	check(demo.get_node("DrossWorldHost").get_movement_mode() == "combat_pending",
			"approaching the mouse did not request combat")
	check(not demo.perform_thump_action(),
			"Thump bypassed the combat-pending safe boundary")
	check(demo.advance_authoritative_tick(), "combat safe-boundary tick failed")
	check(demo.get_node("DrossWorldHost").get_movement_mode() == "combat",
			"safe boundary did not enter combat")

	check(demo.toggle_door(), "integrated demo rejected OpenDoor")
	check(demo.get_node("DrossWorldHost").is_door_open(),
			"integrated door view preceded authoritative door state")
	check(demo.perform_thump_action(), "integrated demo rejected Thump")
	check(demo.get_node("DrossWorldHost").is_mouse_killed(),
			"integrated demo did not commit mouse death")
	check(demo.get_node("DrossWorldHost").get_script_state_bool(
			"demo:field_mouse", 2, "rule_checked"),
			"field mouse did not contribute its typed ability rule")
	check(demo.get_node("DrossWorldHost").get_script_state_bool(
			"demo:field_mouse", 2, "attacked"),
			"field mouse did not react to committed damage")
	check(demo.get_node("DrossWorldHost").get_script_state_bool(
			"demo:field_mouse", 2, "killed"),
			"field mouse did not react to committed death")
	check(demo.get_node("DrossWorldHost").get_script_state_int(
			"demo:field_mouse", 2, "reaction_roll") >= 0,
			"field mouse reaction did not use deterministic RandomHub access")
	check(not demo.get_node("FieldMouse").visible,
			"mouse view did not react to committed death")
	check(not demo.get_node("UI/Diagnostics").text.is_empty(),
			"integrated diagnostics panel was empty")

	demo.queue_free()
	if failures.is_empty():
		print("phase14 integrated vertical slice ok")
		quit(0)
	else:
		for failure in failures:
			push_error(failure)
		quit(1)

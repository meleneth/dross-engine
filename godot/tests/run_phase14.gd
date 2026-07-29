extends SceneTree

var failures: Array[String] = []


func check(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)


func _initialize() -> void:
	var demo: Node3D = load("res://demo/phase14_vertical_slice.tscn").instantiate()
	get_root().add_child(demo)
	await process_frame

	check(demo.preview_destination(2), "integrated demo did not preview a reachable path")
	check(demo.request_previewed_move(), "integrated demo rejected MoveTo")
	for tick in range(4):
		check(demo.advance_authoritative_tick(), "integrated movement tick failed")
	check(demo.get_node("DrossWorldHost").get_movement_tick() == 4,
			"diagnostic tick did not follow authoritative fixed ticks")
	check(demo.get_node("DrossWorldHost").get_movement_column() == 2,
			"integrated player did not reach the authoritative destination")
	check(demo.get_node("Player").position.x == 1.0,
			"player view did not follow the authoritative cell")

	check(demo.toggle_door(), "integrated demo rejected OpenDoor")
	check(demo.get_node("DrossWorldHost").is_door_open(),
			"integrated door view preceded authoritative door state")
	check(demo.perform_thump_action(), "integrated demo rejected Thump")
	check(demo.get_node("DrossWorldHost").is_mouse_killed(),
			"integrated demo did not commit mouse death")
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

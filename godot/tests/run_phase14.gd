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
	var initial_hash: String = demo.get_node(
			"DrossWorldHost").get_canonical_capability_hash()
	check(initial_hash.length() == 64, "diagnostic canonical hash is not BLAKE3-256")
	check(demo.save_game(), "integrated exploration save was rejected")
	var exploration_save: PackedByteArray = demo.get_saved_state()
	check(not exploration_save.is_empty(), "integrated exploration save was empty")
	check(demo.save_game(), "repeated integrated exploration save was rejected")
	check(demo.get_saved_state() == exploration_save,
			"unchanged integrated state did not produce canonical save bytes")
	var invalid_save := exploration_save.slice(0, exploration_save.size() - 1)
	var before_invalid_load: String = demo.get_node(
			"DrossWorldHost").get_canonical_capability_hash()
	check(not demo.get_node("DrossWorldHost").restore_integrated_state(invalid_save),
			"truncated integrated save was accepted")
	check(not demo.get_node("DrossWorldHost").get_last_load_error().is_empty(),
			"rejected integrated save did not expose a useful diagnostic")
	check(demo.get_node("DrossWorldHost").get_canonical_capability_hash() ==
			before_invalid_load, "rejected integrated save mutated authoritative state")
	check(demo.hover_world_position(Vector3(sqrt(3.0), 0.0, 0.0)),
			"pointer hover did not query a reachable authoritative cell")
	check(overlay.path_cell_keys == PackedStringArray([
		"dross:phase11:0,0,0",
		"dross:phase11:1,0,0",
	]), "pointer hover did not update the authoritative path preview")
	check("selected facts: dross:phase11:1,0,0 traversable automatic" in
			demo.get_node("UI/Diagnostics").text,
			"pointer hover did not expose compiled cell facts in diagnostics")
	check(demo.preview_destination(2), "integrated demo did not preview a reachable path")
	check(overlay.path_cell_keys == PackedStringArray([
		"dross:phase11:0,0,0",
		"dross:phase11:1,0,0",
		"dross:phase11:2,0,0",
	]), "path preview did not highlight authoritative map cells")
	check(demo.request_previewed_move(), "integrated demo rejected MoveTo")
	check(demo.advance_authoritative_tick(), "first integrated movement tick failed")
	check(is_equal_approx(demo.get_node("Player").position.x, sqrt(3.0) * 0.5),
			"player view did not interpolate within the authoritative edge")
	check(demo.get_node("DrossWorldHost").save_integrated_state().is_empty(),
			"integrated save accepted a non-quiescent movement boundary")
	check(demo.load_game(), "integrated exploration reload was rejected: %s" %
			demo.get_node("DrossWorldHost").get_last_load_error())
	check(demo.get_node("DrossWorldHost").get_movement_tick() == 0,
			"integrated exploration reload did not restore the saved tick")
	check(demo.get_node("DrossWorldHost").get_movement_column() == 0,
			"integrated exploration reload did not restore the saved actor cell")
	check(is_zero_approx(demo.get_node("Player").position.x),
			"integrated exploration reload did not reconstruct the player view")
	check(demo.preview_destination(2), "reloaded demo did not preview a reachable path")
	check(demo.request_previewed_move(), "reloaded demo rejected MoveTo")
	check(demo.advance_authoritative_tick(), "reloaded first movement tick failed")
	for tick in range(3):
		check(demo.advance_authoritative_tick(), "integrated movement tick failed")
	check(demo.get_node("DrossWorldHost").get_movement_tick() == 4,
			"diagnostic tick did not follow authoritative fixed ticks")
	check(demo.get_node("DrossWorldHost").get_movement_column() == 2,
			"integrated player did not reach the authoritative destination")
	check(demo.get_node("DrossWorldHost").get_canonical_capability_hash() != initial_hash,
			"authoritative movement did not change the diagnostic hash")
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
	check(demo.get_node("DrossWorldHost").is_door_presentation_pending(),
			"integrated door animation was acknowledged before presentation")
	check(is_zero_approx(demo.get_node("SideDoor").rotation_degrees.y),
			"integrated door view changed before its post-commit animation")
	check(not demo.toggle_door(),
			"integrated door accepted another command during presentation gating")
	await create_timer(0.3).timeout
	check(is_equal_approx(demo.get_node("SideDoor").rotation_degrees.y, 90.0),
			"integrated door animation did not reach the committed open state")
	check(not demo.get_node("DrossWorldHost").is_door_presentation_pending(),
			"integrated door animation did not acknowledge completion")
	check(demo.get_node("DrossWorldHost").get_player_action_points() == 3,
			"integrated combat did not begin with authoritative player AP")
	check(demo.perform_thump_action(), "integrated demo rejected Thump")
	check(demo.get_node("DrossWorldHost").get_player_action_points() == 1,
			"integrated Thump did not spend authoritative player AP")
	check(demo.save_game(), "integrated post-combat save was rejected")
	check(demo.get_saved_state() != exploration_save,
			"changed integrated capabilities did not change canonical save bytes")
	check("player AP: 1" in demo.get_node("UI/Diagnostics").text,
			"integrated diagnostics did not expose committed player AP")
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
	var completed_hash: String = demo.get_node(
			"DrossWorldHost").get_canonical_capability_hash()
	check(demo.toggle_door(), "post-combat CloseDoor was rejected")
	await create_timer(0.3).timeout
	check(not demo.get_node("DrossWorldHost").is_door_open(),
			"post-combat door mutation did not commit before reload")
	check(demo.load_game(), "integrated completed-state reload was rejected: %s" %
			demo.get_node("DrossWorldHost").get_last_load_error())
	check(demo.get_node("DrossWorldHost").get_canonical_capability_hash() == completed_hash,
			"completed-state reload did not restore the canonical capability hash")
	check(demo.get_node("DrossWorldHost").is_door_open(),
			"completed-state reload did not restore the authoritative door")
	check(demo.get_node("DrossWorldHost").get_player_action_points() == 1,
			"completed-state reload did not restore combat AP")
	check(demo.get_node("DrossWorldHost").is_mouse_killed(),
			"completed-state reload did not restore mouse death")
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

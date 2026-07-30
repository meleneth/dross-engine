extends SceneTree

var failures: Array[String] = []


func check(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)


func _initialize() -> void:
	check(ClassDB.class_exists("DrossEntityView"), "entity view is not registered")
	check(ClassDB.class_exists("DrossViewRegistry"), "view registry is not registered")

	var registry := DrossViewRegistry.new()
	get_root().add_child(registry)
	var view := DrossEntityView.new()
	view.entity_sequence = 1
	registry.add_child(view)
	check(registry.register_view(view), "view registration failed")
	check(registry.find_view(1) == view, "registered view was not found")

	view.apply_presentation_snapshot(Vector3.ZERO, Vector3(4.0, 0.0, 0.0), 0.25)
	check(view.position.is_equal_approx(Vector3(1.0, 0.0, 0.0)),
			"presentation snapshot interpolation was incorrect")
	view.apply_presentation_snapshot(Vector3.ZERO, Vector3(4.0, 0.0, 0.0), 2.0)
	check(view.position.is_equal_approx(Vector3(4.0, 0.0, 0.0)),
			"presentation interpolation did not clamp frame alpha")

	view.queue_free()
	await process_frame
	check(registry.find_view(1) == null, "freed view remained discoverable")

	var recreated := DrossEntityView.new()
	recreated.entity_sequence = 1
	registry.add_child(recreated)
	check(registry.register_view(recreated), "recreated view did not replace the stale identity")
	recreated.apply_presentation_snapshot(Vector3(4.0, 0.0, 0.0),
			Vector3(6.0, 0.0, 0.0), 0.0)
	check(recreated.position.is_equal_approx(Vector3(4.0, 0.0, 0.0)),
			"recreated view did not reconstruct from its snapshot")

	var first_host := DrossWorldHost.new()
	var second_host := DrossWorldHost.new()
	get_root().add_child(first_host)
	get_root().add_child(second_host)
	check(first_host.start_movement_scenario(), "first movement host did not start")
	check(second_host.start_movement_scenario(), "second movement host did not start")
	var preview: DrossMovementPreview = first_host.preview_movement(3, 0)
	check(preview.is_accepted(), "core movement preview rejected the reachable destination")
	check(preview.get_cost() == 6, "preview returned the wrong authoritative cost")
	check(preview.get_duration_ticks() == 6, "preview returned the wrong fixed-tick duration")
	check(preview.get_path_columns() == PackedInt32Array([0, 1, 2, 3]),
			"preview did not expose the authoritative path")
	var path_overlay := DrossHexGridOverlay3D.new()
	path_overlay.path_cell_keys = preview.get_path_cell_keys()
	get_root().add_child(path_overlay)
	check(path_overlay.path_cell_keys == preview.get_path_cell_keys(),
			"path overlay did not consume the core preview")
	check(first_host.move_to(3, 0), "first typed MoveTo request was rejected")
	check(second_host.move_to(3, 0), "second typed MoveTo request was rejected")

	for tick in range(6):
		check(first_host.advance_movement_tick(), "first movement tick failed")
		# Presentation frames may vary; authoritative ticks do not.
		for frame in range((tick % 3) + 1):
			await process_frame
		check(second_host.advance_movement_tick(), "second movement tick failed")
	check(first_host.get_movement_column() == 3, "movement stopped before its destination")
	check(second_host.get_movement_column() == first_host.get_movement_column(),
			"frame cadence changed authoritative movement")
	check(first_host.get_movement_state() == "idle", "completed movement did not return to idle")

	var cancelled_host := DrossWorldHost.new()
	get_root().add_child(cancelled_host)
	check(cancelled_host.start_movement_scenario(), "cancel movement host did not start")
	check(cancelled_host.move_to(3, 0), "cancelled route was not accepted")
	check(cancelled_host.cancel_movement(), "typed CancelMovement request was rejected")
	check(cancelled_host.advance_movement_tick(), "cancelled route first tick failed")
	check(cancelled_host.get_movement_column() == 0, "cancel occurred at a fractional position")
	check(cancelled_host.advance_movement_tick(), "cancelled route boundary tick failed")
	check(cancelled_host.get_movement_column() == 1, "cancel did not finish the current edge")
	check(cancelled_host.get_movement_state() == "idle", "cancelled movement did not settle")

	var combat_host := DrossWorldHost.new()
	get_root().add_child(combat_host)
	check(combat_host.start_movement_scenario(), "combat movement host did not start")
	check(combat_host.move_to(3, 0), "combat-pending route was not accepted")
	check(combat_host.request_movement_combat(), "combat request was rejected")
	check(combat_host.get_movement_mode() == "combat_pending", "combat did not become pending")
	check(not combat_host.move_to(2, 0),
			"combat pending accepted replacement exploration movement")
	check(combat_host.advance_movement_tick(), "combat-pending first tick failed")
	check(combat_host.get_movement_mode() == "combat_pending",
			"combat began before the safe boundary")
	check(combat_host.advance_movement_tick(), "combat boundary tick failed")
	check(combat_host.get_movement_column() == 1, "combat pending did not finish the current edge")
	check(combat_host.get_movement_mode() == "combat", "safe boundary did not enter combat")

	registry.queue_free()
	first_host.queue_free()
	second_host.queue_free()
	cancelled_host.queue_free()
	combat_host.queue_free()
	path_overlay.queue_free()
	if failures.is_empty():
		print("phase11 movement boundary ok")
		quit(0)
	else:
		for failure in failures:
			push_error(failure)
		quit(1)

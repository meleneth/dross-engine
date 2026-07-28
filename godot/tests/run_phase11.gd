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

	registry.queue_free()
	if failures.is_empty():
		print("phase11 entity views ok")
		quit(0)
	else:
		for failure in failures:
			push_error(failure)
		quit(1)

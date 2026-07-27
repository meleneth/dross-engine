extends SceneTree

var failures: Array[String] = []


func check(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)


func _initialize() -> void:
	check(ClassDB.class_exists("DrossValidationError"), "DrossValidationError is not registered")
	check(ClassDB.class_exists("DrossFootprintDefinition"), "DrossFootprintDefinition is not registered")
	check(ClassDB.class_exists("DrossActorDefinition"), "DrossActorDefinition is not registered")
	check(ClassDB.class_exists("DrossWorldHost"), "DrossWorldHost is not registered")

	var invalid := DrossFootprintDefinition.new()
	invalid.content_id = "Invalid"
	invalid.offsets = PackedVector2Array([Vector2.ZERO])
	var invalid_error = invalid.validate()
	check(invalid_error != null, "invalid ContentId was accepted")
	if invalid_error != null:
		check(invalid_error.resource_path == "<memory>", "validation error omitted resource path")
		check(invalid_error.property_name == "content_id", "validation error omitted property name")

	var missing_origin := DrossFootprintDefinition.new()
	missing_origin.content_id = "demo:missing_origin"
	missing_origin.offsets = PackedVector2Array([Vector2(1, 0)])
	var origin_error = missing_origin.validate()
	check(origin_error != null and origin_error.property_name == "offsets",
			"missing footprint origin was accepted")

	var footprint := DrossFootprintDefinition.new()
	footprint.content_id = "demo:single"
	footprint.offsets = PackedVector2Array([Vector2.ZERO])
	check(footprint.validate() == null, "valid footprint was rejected")
	check(footprint.compile_summary() == "demo:single:1", "footprint compiled incorrectly")

	var actor := DrossActorDefinition.new()
	actor.content_id = "demo:mouse"
	actor.footprint = footprint
	check(actor.validate() == null, "valid actor was rejected")
	check(actor.compile_summary() == "demo:mouse:1", "actor compiled incorrectly")

	var host := DrossWorldHost.new()
	get_root().add_child(host)
	check(host.start_synthetic_world(actor) == null, "world host rejected valid actor")
	check(host.is_running(), "world host did not start")
	check(host.get_tick() == 0, "world host did not start at tick zero")
	check(host.get_entity_count() == 1, "synthetic authoritative entity was not created")
	check(host.get_actor_id() == "demo:mouse", "compiled actor ID was not retained")
	check(host.get_footprint_cell_count() == 1, "compiled footprint was not retained")

	footprint.content_id = "demo:mutated"
	footprint.offsets = PackedVector2Array([Vector2.ZERO, Vector2(1, 0)])
	actor.content_id = "demo:mutated_actor"
	actor.footprint = null
	footprint = null
	actor = null
	check(host.get_actor_id() == "demo:mouse", "Resource mutation changed compiled actor")
	check(host.get_footprint_cell_count() == 1, "Resource mutation changed compiled footprint")
	check(host.get_entity_count() == 1, "freeing Resources destroyed an authoritative entity")

	check(host.advance_test_tick(), "first explicit tick failed")
	check(host.advance_test_tick(), "second explicit tick failed")
	check(host.get_tick() == 2, "fixed-tick runtime advanced nondeterministically")
	host.stop_world()
	check(not host.is_running(), "world host did not stop")
	host.queue_free()

	if failures.is_empty():
		print("phase08 godot boundary ok")
		quit(0)
	else:
		for failure in failures:
			push_error(failure)
		quit(1)

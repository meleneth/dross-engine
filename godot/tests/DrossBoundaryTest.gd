class_name DrossBoundaryTest
extends GdUnitTestSuite


func test_registered_boundary_classes_exist() -> void:
	assert_bool(ClassDB.class_exists("DrossValidationError")).is_true()
	assert_bool(ClassDB.class_exists("DrossFootprintDefinition")).is_true()
	assert_bool(ClassDB.class_exists("DrossActorDefinition")).is_true()
	assert_bool(ClassDB.class_exists("DrossWorldHost")).is_true()


func test_definitions_compile_with_structured_validation() -> void:
	var invalid := DrossFootprintDefinition.new()
	invalid.content_id = "Invalid"
	invalid.offsets = PackedVector2Array([Vector2.ZERO])
	var invalid_error = invalid.validate()
	assert_object(invalid_error).is_not_null()
	assert_str(invalid_error.resource_path).is_equal("<memory>")
	assert_str(invalid_error.property_name).is_equal("content_id")

	var missing_origin := DrossFootprintDefinition.new()
	missing_origin.content_id = "demo:missing_origin"
	missing_origin.offsets = PackedVector2Array([Vector2(1, 0)])
	var origin_error = missing_origin.validate()
	assert_object(origin_error).is_not_null()
	assert_str(origin_error.property_name).is_equal("offsets")

	var fractional := DrossFootprintDefinition.new()
	fractional.content_id = "demo:fractional"
	fractional.offsets = PackedVector2Array([Vector2.ZERO, Vector2(0.5, 0)])
	var fractional_error = fractional.validate()
	assert_object(fractional_error).is_not_null()
	assert_str(fractional_error.property_name).is_equal("offsets")

	var footprint := DrossFootprintDefinition.new()
	footprint.content_id = "demo:single"
	footprint.offsets = PackedVector2Array([Vector2.ZERO])
	assert_object(footprint.validate()).is_null()
	assert_str(footprint.compile_summary()).is_equal("demo:single:1")

	var actor := DrossActorDefinition.new()
	actor.content_id = "demo:mouse"
	actor.footprint = footprint
	assert_object(actor.validate()).is_null()
	assert_str(actor.compile_summary()).is_equal("demo:mouse:1")


func test_compilation_isolates_authoritative_world_from_resources() -> void:
	var footprint := DrossFootprintDefinition.new()
	footprint.content_id = "demo:single"
	footprint.offsets = PackedVector2Array([Vector2.ZERO])
	var actor := DrossActorDefinition.new()
	actor.content_id = "demo:mouse"
	actor.footprint = footprint

	var host := auto_free(DrossWorldHost.new()) as DrossWorldHost
	assert_object(host.start_synthetic_world(actor)).is_null()
	assert_bool(host.is_running()).is_true()
	assert_int(host.get_tick()).is_equal(0)
	assert_int(host.get_entity_count()).is_equal(1)
	assert_str(host.get_actor_id()).is_equal("demo:mouse")
	assert_int(host.get_footprint_cell_count()).is_equal(1)

	footprint.content_id = "demo:mutated"
	footprint.offsets = PackedVector2Array([Vector2.ZERO, Vector2(1, 0)])
	actor.content_id = "demo:mutated_actor"
	actor.footprint = null
	footprint = null
	actor = null

	assert_str(host.get_actor_id()).is_equal("demo:mouse")
	assert_int(host.get_footprint_cell_count()).is_equal(1)
	assert_int(host.get_entity_count()).is_equal(1)
	assert_bool(host.advance_test_tick()).is_true()
	assert_bool(host.advance_test_tick()).is_true()
	assert_int(host.get_tick()).is_equal(2)

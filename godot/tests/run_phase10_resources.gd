extends SceneTree

var failures: Array[String] = []


func check(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)


func _initialize() -> void:
	for type in [
		"DrossHexBakeProfile", "DrossHexGridBake", "DrossHexGridOverrides",
		"DrossCompiledHexMap"
	]:
		check(ClassDB.class_exists(type), type + " is not registered")

	var profile := DrossHexBakeProfile.new()
	profile.required_sample_count = 7
	var bake := DrossHexGridBake.new()
	bake.region_id = "demo:room"
	bake.radius_mm = 1000
	bake.cell_coordinates = PackedInt32Array([1, 0, 0, 0, 0, 0])
	bake.surface_samples_mm = PackedInt32Array([
		1000, 1000, 1000, 1000, 1000, 1000, 1000,
		1001, 1000, 999, 1000, 1001, 999, 1000,
	])
	bake.standing_clearance = PackedByteArray([1, 0])
	bake.edge_coordinates = PackedInt32Array([0, 0, 0, 1, 0, 0])
	bake.edge_clearance = PackedByteArray([1, 0])
	check(bake.cell_count == 2, "bake resource rejected its typed evidence")

	var overrides := DrossHexGridOverrides.new()
	overrides.region_id = "demo:room"
	overrides.radius_mm = 1000
	check(overrides.set_cell_override(0, 0, 0, 1), "manual override was rejected")
	var compiled := DrossCompiledHexMap.new()
	check(compiled.compile_from(bake, overrides, profile), "typed resources did not compile")
	check(compiled.cell_count == 2, "compiled map lost cells")
	check(compiled.edge_count == 1, "compiled map lost directional edge evidence")
	check(compiled.cell_keys == PackedStringArray(["demo:room:0,0,0", "demo:room:1,0,0"]),
			"compiled map order was not canonical")
	check(compiled.traversability == PackedByteArray([1, 1]),
			"manual force-traversable did not win")
	check(compiled.provenance == PackedByteArray([1, 0]),
			"compiled provenance did not distinguish manual intent")

	bake.standing_clearance = PackedByteArray([1, 1])
	var rebaked := DrossCompiledHexMap.new()
	check(rebaked.compile_from(bake, overrides, profile), "rebake failed")
	check(rebaked.provenance == compiled.provenance, "rebake erased manual provenance")

	overrides.radius_mm = 900
	check(not DrossCompiledHexMap.new().compile_from(bake, overrides, profile),
			"grid identity mismatch did not surface an orphan conflict")

	if failures.is_empty():
		print("phase10 typed grid resources ok")
		quit(0)
	else:
		for failure in failures:
			push_error(failure)
		quit(1)

extends SceneTree

var failures: Array[String] = []


func check(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)


func static_box(parent: Node3D, position: Vector3, size: Vector3) -> void:
	var body := StaticBody3D.new()
	var shape := CollisionShape3D.new()
	var box := BoxShape3D.new()
	box.size = size
	shape.shape = box
	body.position = position
	body.add_child(shape)
	parent.add_child(body)


func _initialize() -> void:
	check(ClassDB.class_exists("DrossHexGridRegion3D"), "grid region is not registered")
	var room := Node3D.new()
	get_root().add_child(room)
	static_box(room, Vector3(0, -0.1, 0), Vector3(8, 0.2, 5))
	# Narrow geometry affects the center sample but not the six inset samples.
	static_box(room, Vector3(0, 0.5, 0), Vector3(0.35, 1.0, 0.35))

	var region := DrossHexGridRegion3D.new()
	region.region_id = "demo:room"
	region.cell_radius = 1.0
	region.q_min = 0
	region.q_max = 1
	region.r_min = 0
	region.r_max = 0
	region.bake_profile = DrossHexBakeProfile.new()
	region.overrides = DrossHexGridOverrides.new()
	region.overrides.region_id = "demo:room"
	region.overrides.radius_mm = 1000
	room.add_child(region)

	await physics_frame
	await physics_frame
	var bake: DrossHexGridBake = region.bake_geometry()
	check(bake.cell_count == 2, "physics analyzer did not find both supported cells")
	check(bake.surface_samples_mm.size() == 14, "analyzer did not retain seven samples per cell")
	check(bake.surface_samples_mm[0] != bake.surface_samples_mm[1],
			"geometric obstruction did not affect sampled evidence")

	check(region.overrides.set_cell_override(0, 0, 0, 1), "manual override was rejected")
	var compiled: DrossCompiledHexMap = region.compile_map()
	check(compiled.cell_count == 2, "geometry bake did not compile")
	check(compiled.edge_count == 1, "analyzer did not compile the neighboring transition")
	check(compiled.traversability == PackedByteArray([1, 1]),
			"manual override did not correct the geometric obstruction")
	check(compiled.provenance == PackedByteArray([1, 0]),
			"compiled map lost automatic/manual provenance")

	var rebake: DrossHexGridBake = region.bake_geometry()
	check(rebake.cell_count == bake.cell_count, "unchanged geometry rebake changed cell identity")
	var recompiled: DrossCompiledHexMap = region.compile_map()
	check(recompiled.cell_keys == compiled.cell_keys, "rebake changed canonical cell identity")
	check(recompiled.provenance == compiled.provenance, "rebake erased the manual override")

	room.queue_free()
	if failures.is_empty():
		print("phase10 geometry analyzer ok")
		quit(0)
	else:
		for failure in failures:
			push_error(failure)
		quit(1)

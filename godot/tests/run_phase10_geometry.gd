extends SceneTree

var failures: Array[String] = []


func check(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)


func _initialize() -> void:
	check(ClassDB.class_exists("DrossHexGridRegion3D"), "grid region is not registered")
	var room: Node3D = load("res://thump_demo/scenes/phase10_room.tscn").instantiate()
	get_root().add_child(room)
	var region: DrossHexGridRegion3D = room.get_node("GridRegion")
	check(region.optional_door_edge ==
			"thump_demo:room:0,0,0|thump_demo:room:1,0,0",
			"ThumpDemo room did not retain its optional side-door edge")

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
	check(region.select_cell_at_local(region.cell_center(1, 0)) == "thump_demo:room:1,0,0",
			"viewport selection did not map to the correct HexCellId")
	var overlay := DrossHexGridOverlay3D.new()
	overlay.compiled_map = recompiled
	room.add_child(overlay)
	check(overlay.cell_keys == recompiled.cell_keys,
			"runtime overlay did not use the complete compiled cell set")
	check(overlay.mesh != null and overlay.mesh.get_surface_count() == 1,
			"runtime overlay did not build visible grid geometry")

	var gizmo_script: Script = load("res://addons/dross_editor/grid_gizmo.gd")
	check(gizmo_script != null and gizmo_script.can_instantiate(),
			"editor grid gizmo script did not load")

	room.queue_free()
	if failures.is_empty():
		print("phase10 geometry analyzer ok")
		quit(0)
	else:
		for failure in failures:
			push_error(failure)
		quit(1)

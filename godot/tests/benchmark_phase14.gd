extends SceneTree

const SAMPLE_BATCHES := 100


func median(values: Array[float]) -> float:
	values.sort()
	return values[values.size() / 2]


func percentile_95(values: Array[float]) -> float:
	values.sort()
	return values[ceili(float(values.size()) * 0.95) - 1]


func report(label: String, values: Array[float]) -> void:
	print("%s median_us=%.3f p95_us=%.3f samples=%d" % [
		label,
		median(values),
		percentile_95(values),
		values.size(),
	])


func actor_definition() -> DrossActorDefinition:
	var footprint := DrossFootprintDefinition.new()
	footprint.content_id = "demo:benchmark_single"
	footprint.offsets = PackedVector2Array([Vector2.ZERO])
	var actor := DrossActorDefinition.new()
	actor.content_id = "demo:benchmark_actor"
	actor.footprint = footprint
	return actor


func script_modules() -> Array[DrossScriptModuleDefinition]:
	var definition := DrossScriptModuleDefinition.new()
	definition.module_id = "demo:benchmark_entity"
	definition.scope_kind = 1
	definition.entity_sequence = 1
	definition.state_schema_version = 1
	definition.script = load("res://scripts/phase09_entity.gd")
	return [definition]


func _initialize() -> void:
	var tick_host := DrossWorldHost.new()
	get_root().add_child(tick_host)
	if tick_host.start_synthetic_world(actor_definition()) != null:
		push_error("benchmark synthetic world failed to start")
		quit(1)
		return
	var tick_samples: Array[float] = []
	for sample in SAMPLE_BATCHES:
		var started := Time.get_ticks_usec()
		for iteration in 100:
			tick_host.advance_test_tick()
		tick_samples.append(float(Time.get_ticks_usec() - started) / 100.0)
	report("headless_fixed_tick", tick_samples)

	var movement_host := DrossWorldHost.new()
	get_root().add_child(movement_host)
	movement_host.start_movement_scenario()
	var preview_samples: Array[float] = []
	var hash_samples: Array[float] = []
	for sample in SAMPLE_BATCHES:
		var preview_started := Time.get_ticks_usec()
		for iteration in 100:
			movement_host.preview_movement(3)
		preview_samples.append(float(Time.get_ticks_usec() - preview_started) / 100.0)
		var hash_started := Time.get_ticks_usec()
		for iteration in 100:
			movement_host.get_canonical_capability_hash()
		hash_samples.append(float(Time.get_ticks_usec() - hash_started) / 100.0)
	report("path_preview_demo", preview_samples)
	report("canonical_hash", hash_samples)

	var script_host := DrossWorldHost.new()
	get_root().add_child(script_host)
	script_host.start_script_scenario(script_modules(), 12345)
	var script_samples: Array[float] = []
	for sample in SAMPLE_BATCHES:
		var script_started := Time.get_ticks_usec()
		for iteration in 10:
			script_host.run_script_scenario()
		script_samples.append(float(Time.get_ticks_usec() - script_started) / 10.0)
	report("script_callback_event", script_samples)

	var demo: Node3D = load("res://demo/phase14_vertical_slice.tscn").instantiate()
	get_root().add_child(demo)
	await process_frame
	var integrated_host: DrossWorldHost = demo.get_node("DrossWorldHost")
	var saved := integrated_host.save_integrated_state()
	var save_samples: Array[float] = []
	var load_samples: Array[float] = []
	for sample in SAMPLE_BATCHES:
		var save_started := Time.get_ticks_usec()
		for iteration in 10:
			integrated_host.save_integrated_state()
		save_samples.append(float(Time.get_ticks_usec() - save_started) / 10.0)
		var load_started := Time.get_ticks_usec()
		for iteration in 10:
			integrated_host.restore_integrated_state(saved)
		load_samples.append(float(Time.get_ticks_usec() - load_started) / 10.0)
	report("integrated_save", save_samples)
	report("integrated_load", load_samples)
	print("integrated_save bytes=%d" % saved.size())

	var room: Node3D = load("res://demo/phase10_room.tscn").instantiate()
	get_root().add_child(room)
	await physics_frame
	await physics_frame
	var region: DrossHexGridRegion3D = room.get_node("GridRegion")
	var bake_samples: Array[float] = []
	for sample in SAMPLE_BATCHES:
		var bake_started := Time.get_ticks_usec()
		region.bake_geometry()
		bake_samples.append(float(Time.get_ticks_usec() - bake_started))
	report("editor_grid_bake_2_cells", bake_samples)

	print("phase14 performance baseline complete")
	quit(0)

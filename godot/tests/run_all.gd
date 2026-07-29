extends SceneTree

func _initialize() -> void:
	var executable := OS.get_executable_path()
	var project_path := ProjectSettings.globalize_path("res://")
	var test_scripts := PackedStringArray()
	for file_name in DirAccess.get_files_at("res://tests"):
		if file_name.begins_with("run_phase") and file_name.ends_with(".gd"):
			test_scripts.append("res://tests/%s" % file_name)
	test_scripts.sort()
	if test_scripts.is_empty():
		push_error("No Godot phase test scripts were discovered")
		quit(1)
		return
	for script_path in test_scripts:
		var output: Array = []
		var exit_code := OS.execute(executable, [
			"--headless",
			"--path",
			project_path,
			"--script",
			script_path,
		], output, true)
		for line in output:
			print(str(line).strip_edges())
		if exit_code != 0:
			push_error("Godot test failed: %s (exit %d)" % [script_path, exit_code])
			quit(exit_code)
			return
	print("all Godot integration scripts passed (%d)" % test_scripts.size())
	quit(0)

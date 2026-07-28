extends SceneTree

var failures: Array[String] = []


func check(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)


func module(id: String, scope: int, sequence: int, path: String) -> DrossScriptModuleDefinition:
	var result := DrossScriptModuleDefinition.new()
	result.module_id = id
	result.scope_kind = scope
	result.entity_sequence = sequence
	result.state_schema_version = 1
	result.script = load(path)
	return result


func live_modules() -> Array[DrossScriptModuleDefinition]:
	# Deliberately shuffled: runtime order must ignore resource load order.
	return [
		module("demo:observer", 1, 2, "res://scripts/phase09_observer.gd"),
		module("demo:entity", 1, 1, "res://scripts/phase09_entity.gd"),
		module("demo:region_module", 0, 0, "res://scripts/phase09_region.gd"),
	]


func run_once(seed_value: int) -> DrossWorldHost:
	var host := DrossWorldHost.new()
	get_root().add_child(host)
	check(host.start_script_scenario(live_modules(), seed_value), "script modules failed to install")
	check(host.run_script_scenario(), "live typed callbacks failed")
	return host


func _initialize() -> void:
	for type in [
		"DrossScriptModuleDefinition", "DrossScriptContext", "DrossScriptStateApi",
		"DrossRandomApi", "DrossCommandApi", "DrossQueryApi",
		"DrossEntityPlacedEvent", "DrossPlacementRuleQuery"
	]:
		check(ClassDB.class_exists(type), type + " is not registered")

	var first := run_once(12345)
	check(first.get_script_mode() == "combat", "region command was not deferred to combat mode")
	check(first.get_script_state_bool("demo:region_module", 0, "condition_met"),
			"region durable state was not committed")
	check(first.get_script_state_bool("demo:entity", 1, "observed"),
			"typed entity event did not commit state")
	check(first.get_script_state_bool("demo:observer", 2, "observed"),
			"second entity callback did not run")
	check(first.get_script_state_int("demo:entity", 1, "roll") >= 0,
			"deterministic random result was not stored")
	var expected_order := (
		"demo:region_module:contribute_placement_rules,"
		+ "demo:entity:contribute_placement_rules,"
		+ "demo:entity:on_entity_placed,"
		+ "demo:observer:on_entity_placed"
	)
	check(first.get_script_call_order() == expected_order,
			"callbacks ignored canonical scope/entity/module order: " + first.get_script_call_order())

	var saved := first.save_script_state()
	check(not saved.is_empty(), "script state save was empty")
	var replay := run_once(12345)
	check(replay.save_script_state() == saved, "same-seed script replay diverged")
	check(replay.get_script_call_order() == first.get_script_call_order(),
			"same-seed callback trace diverged")

	var restored := DrossWorldHost.new()
	get_root().add_child(restored)
	check(restored.start_script_scenario(live_modules(), 999), "reload modules failed")
	check(restored.restore_script_state(saved), "script state restore failed")
	check(restored.get_script_state_bool("demo:entity", 1, "observed"),
			"durable state did not survive fresh script instances")
	check(restored.get_script_state_int("demo:entity", 1, "roll")
			== first.get_script_state_int("demo:entity", 1, "roll"),
			"restored random-authored state changed")

	var fault := DrossWorldHost.new()
	get_root().add_child(fault)
	var fault_modules: Array[DrossScriptModuleDefinition] = [
		module("demo:fault", 1, 1, "res://scripts/phase09_fault.gd")
	]
	check(fault.start_script_scenario(fault_modules, 7), "fault module failed to install")
	check(not fault.run_script_scenario(), "script runtime error continued silently")
	check(fault.is_script_world_faulted(), "event callback fault did not fault the world")
	check(not fault.get_script_state_bool("demo:fault", 1, "event_must_discard"),
			"faulted callback committed buffered state")

	var rule_fault := DrossWorldHost.new()
	get_root().add_child(rule_fault)
	var rule_fault_modules: Array[DrossScriptModuleDefinition] = [
		module("demo:rule_fault", 0, 0, "res://scripts/phase09_rule_fault.gd")
	]
	check(rule_fault.start_script_scenario(rule_fault_modules, 7),
			"rule fault module failed to install")
	check(not rule_fault.run_script_scenario(), "rule callback fault continued silently")
	check(not rule_fault.is_script_world_faulted(), "pre-commit rule fault faulted the world")
	check(rule_fault.get_script_mode() == "exploration",
			"faulted rule callback committed its deferred mode command")
	check(not rule_fault.get_script_state_bool("demo:rule_fault", 0, "must_discard"),
			"faulted rule callback committed buffered state")

	first.queue_free()
	replay.queue_free()
	restored.queue_free()
	fault.queue_free()
	rule_fault.queue_free()
	if failures.is_empty():
		print("phase09 typed gdscript runtime ok")
		quit(0)
	else:
		for failure in failures:
			push_error(failure)
		quit(1)

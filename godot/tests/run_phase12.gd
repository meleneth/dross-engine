extends SceneTree

var failures: Array[String] = []


func check(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)


func _initialize() -> void:
	check(ClassDB.class_exists("DrossAbilityDefinition"), "ability definition is not registered")
	var thump := DrossAbilityDefinition.new()
	thump.ability_id = "dross_demo:thump"
	thump.range = 1
	thump.action_point_cost = 2
	thump.damage = 3
	thump.presentation_cue = "dross_demo:thump"
	check(thump.is_valid(), "typed Thump definition did not compile")

	thump.damage = 0
	check(not thump.is_valid(), "zero-damage ability definition was accepted")
	thump.damage = 3
	thump.ability_id = "Thump"
	check(not thump.is_valid(), "non-canonical ability identity was accepted")

	if failures.is_empty():
		print("phase12 ability resource ok")
		quit(0)
	else:
		for failure in failures:
			push_error(failure)
		quit(1)

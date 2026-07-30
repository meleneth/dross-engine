extends DrossRegionScript


func on_actor_killed(event: DrossActorKilledEvent, context: DrossScriptContext) -> void:
	if event.target_sequence != 2:
		return
	if context.query.quest_status("thump_demo:mouse_quest") != "active":
		return
	if context.query.quest_stage("thump_demo:mouse_quest") != "thump_demo:hunt_mouse":
		return
	var submitted := context.commands.advance_quest(
		"thump_demo:mouse_quest",
		"thump_demo:hunt_mouse",
		"thump_demo:return_tail"
	)
	context.state.set_bool("return_stage_submitted", submitted)

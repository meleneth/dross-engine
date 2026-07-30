extends DrossEntityScript


func on_dialogue_option_chosen(
		event: DrossDialogueOptionChosenEvent, context: DrossScriptContext) -> void:
	if event.dialogue != "thump_demo:caretaker_dialogue":
		return
	if event.option != "thump_demo:accept_mouse_quest":
		return
	if not context.query.is_owner(event.partner_lineage, event.partner_sequence):
		return
	if context.query.quest_status("thump_demo:mouse_quest") != "inactive":
		return
	var submitted := context.commands.start_quest(
		"thump_demo:mouse_quest", "thump_demo:hunt_mouse"
	)
	context.state.set_bool("accepted_mouse_quest", submitted)

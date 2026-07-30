extends DrossEntityScript


func on_dialogue_option_chosen(
		event: DrossDialogueOptionChosenEvent, context: DrossScriptContext) -> void:
	if event.dialogue != "thump_demo:caretaker_dialogue":
		return
	if not context.query.is_owner(event.partner_lineage, event.partner_sequence):
		return
	if event.option == "thump_demo:accept_mouse_quest":
		if context.query.quest_status("thump_demo:mouse_quest") != "inactive":
			return
		var submitted := context.commands.start_quest(
			"thump_demo:mouse_quest", "thump_demo:hunt_mouse"
		)
		context.state.set_bool("accepted_mouse_quest", submitted)
	elif event.option == "thump_demo:hand_over_mouse_tail":
		if context.query.quest_stage("thump_demo:mouse_quest") != "thump_demo:return_tail":
			return
		if not context.query.has_item(
				event.initiator_lineage,
				event.initiator_sequence,
				"thump_demo:mouse_tail",
				1
		):
			return
		var removed := context.commands.remove_item(
			event.initiator_lineage,
			event.initiator_sequence,
			"thump_demo:mouse_tail",
			1
		)
		var completed := context.commands.complete_quest(
			"thump_demo:mouse_quest", "thump_demo:return_tail"
		)
		context.state.set_bool("tail_hand_in_submitted", removed and completed)

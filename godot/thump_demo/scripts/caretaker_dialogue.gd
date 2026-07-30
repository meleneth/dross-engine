extends DrossEntityScript


func contribute_dialogue_options(
		query: DrossDialogueOptionQuery, context: DrossScriptContext) -> void:
	if query.dialogue != "thump_demo:caretaker_dialogue":
		return
	if not context.query.is_owner(query.partner_lineage, query.partner_sequence):
		return
	query.add_option("thump_demo:leave")
	var quest_status := context.query.quest_status("thump_demo:mouse_quest")
	if (
			quest_status == "active"
			and context.query.quest_stage("thump_demo:mouse_quest") == "thump_demo:return_tail"
			and context.query.has_item(
				query.initiator_lineage,
				query.initiator_sequence,
				"thump_demo:mouse_tail",
				1
			)
	):
		query.add_option("thump_demo:hand_over_mouse_tail")


func on_dialogue_option_chosen(
		event: DrossDialogueOptionChosenEvent, context: DrossScriptContext) -> void:
	if event.dialogue != "thump_demo:caretaker_dialogue":
		return
	if not context.query.is_owner(event.partner_lineage, event.partner_sequence):
		return
	if event.option == "thump_demo:hand_over_mouse_tail":
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


func on_quest_started(event: DrossQuestStartedEvent, context: DrossScriptContext) -> void:
	if event.quest != "thump_demo:mouse_quest":
		return
	context.state.set_bool("fsm_hunt_assigned", true)


func on_quest_advanced(event: DrossQuestAdvancedEvent, context: DrossScriptContext) -> void:
	if event.quest != "thump_demo:mouse_quest":
		return
	if event.current_stage == "thump_demo:return_tail":
		context.state.set_bool("fsm_waiting_for_tail", true)


func on_quest_completed(event: DrossQuestCompletedEvent, context: DrossScriptContext) -> void:
	if event.quest == "thump_demo:mouse_quest":
		context.state.set_bool("fsm_settled", true)

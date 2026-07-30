extends DrossEntityScript


func contribute_ability_rules(
		_query: DrossAbilityRuleQuery, context: DrossScriptContext) -> void:
	context.state.set_bool("rule_checked", true)


func on_damage_applied(event: DrossDamageAppliedEvent, context: DrossScriptContext) -> void:
	if event.target_sequence != context.owner_sequence:
		return
	context.state.set_bool("attacked", true)
	context.state.set_int("reaction_roll", context.random.below(100))


func on_actor_killed(event: DrossActorKilledEvent, context: DrossScriptContext) -> void:
	if event.target_sequence == context.owner_sequence:
		context.state.set_bool("killed", true)
		context.state.set_int(
			"tails_before_reward",
			context.query.inventory_count(
				event.killer_lineage, event.killer_sequence, "thump_demo:mouse_tail"
			)
		)
		context.state.set_bool(
			"mouse_quest_was_active",
			context.query.quest_status("thump_demo:mouse_quest") == "active"
		)
		var submitted := context.commands.grant_item(
			event.killer_lineage, event.killer_sequence, "thump_demo:mouse_tail", 1
		)
		context.state.set_bool("reward_submitted", submitted)

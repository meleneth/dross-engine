extends RefCounted


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

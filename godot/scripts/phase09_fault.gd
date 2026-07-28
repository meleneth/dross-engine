extends DrossEntityScript


func contribute_placement_rules(
		_query: DrossPlacementRuleQuery, ctx: DrossScriptContext) -> void:
	ctx.state.set_bool("must_discard", true)


func on_entity_placed(_event: DrossEntityPlacedEvent, ctx: DrossScriptContext) -> void:
	ctx.state.set_bool("event_must_discard", true)
	var empty: Array[int] = []
	var _unreachable := empty[2]

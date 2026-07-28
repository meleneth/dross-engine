extends DrossRegionScript


func contribute_placement_rules(
		_query: DrossPlacementRuleQuery, ctx: DrossScriptContext) -> void:
	ctx.state.set_bool("must_discard", true)
	ctx.commands.request_combat()
	var empty: Array[int] = []
	var _unreachable := empty[2]

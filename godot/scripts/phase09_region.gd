extends DrossRegionScript

var ephemeral_callback_count := 0


func contribute_placement_rules(
		_query: DrossPlacementRuleQuery, ctx: DrossScriptContext) -> void:
	ephemeral_callback_count += 1
	if ctx.tick == 0:
		ctx.commands.request_combat()
	ctx.state.set_bool("condition_met", true)

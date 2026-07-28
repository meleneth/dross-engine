extends DrossEntityScript

var ephemeral_callback_count := 0


func contribute_placement_rules(
		_query: DrossPlacementRuleQuery, ctx: DrossScriptContext) -> void:
	ephemeral_callback_count += 1
	ctx.state.set_int("roll", ctx.random.below(1000000))
	if not ctx.query.is_owner(ctx.owner_lineage, ctx.owner_sequence):
		_query.reject("demo:invalid_owner")


func on_entity_placed(event: DrossEntityPlacedEvent, ctx: DrossScriptContext) -> void:
	if event.entity_sequence == 1:
		ctx.state.set_bool("observed", true)

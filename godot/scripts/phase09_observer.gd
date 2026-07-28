extends DrossEntityScript


func on_entity_placed(_event: DrossEntityPlacedEvent, ctx: DrossScriptContext) -> void:
	ctx.state.set_bool("observed", true)

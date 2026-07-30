extends SceneTree

var failures: Array[String] = []


func check(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)


func check_asset_palette(node: Node, palette: Array[Color]) -> void:
	if node is MeshInstance3D:
		var mesh_instance := node as MeshInstance3D
		for surface in range(mesh_instance.get_surface_override_material_count()):
			var material := mesh_instance.get_active_material(surface) as StandardMaterial3D
			check(material != null and palette.has(material.albedo_color),
					"%s retained a color outside LoSpec500" % mesh_instance.get_path())
			check(material != null and
					material.shading_mode == BaseMaterial3D.SHADING_MODE_UNSHADED,
					"%s does not preserve its exact palette color" % mesh_instance.get_path())
	for child in node.get_children():
		check_asset_palette(child, palette)


func _initialize() -> void:
	var demo: Node3D = load(
			"res://thump_demo/scenes/phase14_vertical_slice.tscn").instantiate()
	get_root().add_child(demo)
	await process_frame

	var overlay: DrossHexGridOverlay3D = demo.get_node("GridOverlay")
	var camera: Camera3D = demo.get_node("Camera3D")
	var hud: Control = demo.get_node("UI/HUD")
	check(hud.get_node("BottomBar/PlayerLog/Content/Messages") is RichTextLabel,
			"ThumpDemo HUD does not reserve a player log/chat region")
	check(hud.get_node("BottomBar/CombatActions/Content/Actions/EndTurn") is Button,
			"ThumpDemo HUD does not reserve an End Turn action slot")
	check("entered the caretaker's room" in hud.get_log_text(),
			"player log did not describe entering the room")
	check(demo.get_node("Room/Walls").get_child_count() >= 2,
			"playable room is missing enclosing walls")
	check(demo.has_node("Room/Carpet"), "playable room is missing its carpet")
	check(demo.get_node("Room/Plants").get_child_count() >= 2,
			"playable room is missing lived-in plant dressing")
	check(demo.get_node("Room/Furnishings/Bookcase").scene_file_path.ends_with(
			"/kenney/furniture-kit/models/bookcaseOpen.glb"),
			"playable room does not use the recognizable Kenney furniture collection")
	check(demo.get_node("Room/NatureProps/MouseBush").scene_file_path.ends_with(
			"/kenney/nature-kit/models/plant_bush.glb"),
			"mouse side does not use the recognizable Kenney nature collection")
	for asset_group in [
		"Room/Carpet",
		"Room/Plants",
		"Room/Furnishings",
		"Room/NatureProps",
	]:
		check_asset_palette(demo.get_node(asset_group), demo.LOSPEC500_COLORS)
	check(demo.has_node("Room/Walls/DividerNorth") and
			demo.has_node("Room/Walls/DividerSouth"),
			"route door is not visibly installed in a dividing wall")
	check(camera.projection == Camera3D.PROJECTION_ORTHOGONAL,
			"playable demo camera is not orthographic")
	check(is_equal_approx(camera.rotation_degrees.x, -30.0) and
			is_equal_approx(camera.rotation_degrees.y, 45.0),
			"playable demo camera lost its classic diagonal dimetric framing")
	var compiled_map: DrossCompiledHexMap = demo.get_node(
			"DrossWorldHost").get_movement_compiled_map()
	check(compiled_map.get_cell_count() == 12,
			"integrated overlay did not receive the authoritative movement map")
	check(overlay.get_cell_keys() == compiled_map.get_cell_keys(),
			"runtime grid differs from the authoritative movement map")
	var overlay_material: StandardMaterial3D = overlay.mesh.surface_get_material(0)
	check(overlay_material != null and overlay_material.vertex_color_use_as_albedo,
			"runtime grid material ignores hover and path vertex colors")
	check(overlay_material != null and
			overlay_material.shading_mode == BaseMaterial3D.SHADING_MODE_UNSHADED,
			"runtime grid material does not preserve exact palette colors")
	var player_material: StandardMaterial3D = demo.get_node("Player/Visual").mesh.material
	check(player_material.albedo_color.is_equal_approx(Color8(51, 136, 222)),
			"player material is outside the LoSpec500 palette")
	var initial_hash: String = demo.get_node(
			"DrossWorldHost").get_canonical_capability_hash()
	check(initial_hash.length() == 64, "diagnostic canonical hash is not BLAKE3-256")
	check(demo.save_game(), "integrated exploration save was rejected")
	var exploration_save: PackedByteArray = demo.get_saved_state()
	check(not exploration_save.is_empty(), "integrated exploration save was empty")
	check(demo.save_game(), "repeated integrated exploration save was rejected")
	check(demo.get_saved_state() == exploration_save,
			"unchanged integrated state did not produce canonical save bytes")
	var invalid_save := exploration_save.slice(0, exploration_save.size() - 1)
	var before_invalid_load: String = demo.get_node(
			"DrossWorldHost").get_canonical_capability_hash()
	check(not demo.get_node("DrossWorldHost").restore_integrated_state(invalid_save),
			"truncated integrated save was accepted")
	check(not demo.get_node("DrossWorldHost").get_last_load_error().is_empty(),
			"rejected integrated save did not expose a useful diagnostic")
	check(demo.get_node("DrossWorldHost").get_canonical_capability_hash() ==
			before_invalid_load, "rejected integrated save mutated authoritative state")
	check(demo.hover_world_position(Vector3(sqrt(3.0), 0.0, 0.0)),
			"pointer hover did not query a reachable authoritative cell")
	check(overlay.get_hover_cell_key() == "dross:phase11:1,0,0" and
			overlay.get_hover_state() == 1,
			"reachable hover did not expose a green destination outline")
	check(overlay.path_cell_keys == PackedStringArray([
		"dross:phase11:0,0,0",
		"dross:phase11:1,0,0",
	]), "pointer hover did not update the authoritative path preview")
	check("selected facts: dross:phase11:1,0,0 traversable automatic" in
			demo.get_node("UI/Diagnostics").text,
			"pointer hover did not expose compiled cell facts in diagnostics")
	check(not demo.preview_destination(0, -1),
			"blocked in-world destination unexpectedly produced a valid move")
	check(overlay.get_hover_cell_key() == "dross:phase11:0,-1,0" and
			overlay.get_hover_state() == 2,
			"blocked in-world hover did not expose a red destination outline")
	check(not demo.hover_world_position(Vector3(-100.0, 0.0, -100.0)),
			"off-world pointer position unexpectedly resolved to a destination")
	check(overlay.get_hover_cell_key().is_empty() and overlay.get_hover_state() == 0,
			"off-world pointer position retained a destination outline")
	check(overlay.path_cell_keys.is_empty(),
			"off-world pointer position retained a path highlight")
	check(demo.preview_destination(1, 1),
			"integrated demo did not preview movement into another hex row")
	check(overlay.path_cell_keys.has("dross:phase11:1,1,0"),
			"multi-row path preview did not highlight its authoritative destination")
	check(not demo.preview_destination(2),
			"closed doorway did not block the only route to the mouse side")
	check(overlay.get_hover_state() == 2,
			"closed doorway destination was not presented as invalid")
	check(demo.toggle_door(), "integrated demo rejected opening the route door")
	check(demo.get_node("DrossWorldHost").is_door_open(),
			"route door did not commit open before presentation")
	check("open the side door" in hud.get_log_text(),
			"player log did not describe the committed door opening")
	await create_timer(0.3).timeout
	check(demo.preview_destination(2), "open doorway did not reveal the mouse route")
	check(overlay.path_cell_keys == PackedStringArray([
		"dross:phase11:0,0,0",
		"dross:phase11:1,0,0",
		"dross:phase11:2,0,0",
	]), "path preview did not highlight authoritative map cells")
	check(demo.request_previewed_move(), "integrated demo rejected MoveTo")
	check(demo.get_node("DrossWorldHost").get_recent_movement_events() ==
			PackedStringArray(["dross:movement_started"]),
			"movement diagnostics did not consume the committed start event")
	check(demo.advance_authoritative_tick(), "first integrated movement tick failed")
	check(is_equal_approx(demo.get_node("Player").position.x, sqrt(3.0) * 0.5),
			"player view did not interpolate within the authoritative edge")
	check(demo.get_node("DrossWorldHost").save_integrated_state().is_empty(),
			"integrated save accepted a non-quiescent movement boundary")
	check(demo.load_game(), "integrated exploration reload was rejected: %s" %
			demo.get_node("DrossWorldHost").get_last_load_error())
	check(demo.get_node("DrossWorldHost").get_movement_tick() == 0,
			"integrated exploration reload did not restore the saved tick")
	check(demo.get_node("DrossWorldHost").get_movement_column() == 0,
			"integrated exploration reload did not restore the saved actor cell")
	check(is_zero_approx(demo.get_node("Player").position.x),
			"integrated exploration reload did not reconstruct the player view")
	check(demo.talk_to_caretaker(), "playable scene rejected the caretaker quest offer")
	check(demo.get_node("DrossWorldHost").get_quest_stage(
			"thump_demo:mouse_quest") == "thump_demo:hunt_mouse",
			"playable scene did not project the accepted quest stage")
	check(demo.save_game(), "integrated active-quest save was rejected")
	check(demo.load_game(), "integrated active-quest reload was rejected")
	check(demo.get_node("DrossWorldHost").get_quest_stage(
			"thump_demo:mouse_quest") == "thump_demo:hunt_mouse",
			"active quest stage did not survive save and reload")
	check(not demo.preview_destination(2),
			"reloaded closed door did not restore traversal blocking")
	check(demo.toggle_door(), "reloaded route door could not be opened")
	await create_timer(0.3).timeout
	check(demo.preview_destination(2), "reloaded open door did not reveal the mouse route")
	check(demo.request_previewed_move(), "reloaded demo rejected MoveTo")
	check(demo.advance_authoritative_tick(), "reloaded first movement tick failed")
	for tick in range(3):
		check(demo.advance_authoritative_tick(), "integrated movement tick failed")
	check(demo.get_node("DrossWorldHost").get_movement_tick() == 4,
			"diagnostic tick did not follow authoritative fixed ticks")
	check(demo.get_node("DrossWorldHost").get_movement_column() == 2,
			"integrated player did not reach the authoritative destination")
	check(demo.get_node("DrossWorldHost").get_recent_movement_events() ==
			PackedStringArray([
				"dross:movement_started",
				"dross:actor_entered_cell",
				"dross:actor_entered_cell",
				"dross:movement_completed",
			]), "movement diagnostics did not preserve committed event order")
	check(demo.get_node("DrossWorldHost").get_canonical_capability_hash() != initial_hash,
			"authoritative movement did not change the diagnostic hash")
	check(is_equal_approx(demo.get_node("Player").position.x, sqrt(3.0) * 2.0),
			"player view did not follow the authoritative cell")
	check(demo.get_node("DrossWorldHost").get_movement_mode() == "combat_pending",
			"approaching the mouse did not request combat")
	check(not demo.perform_thump_action(),
			"Thump bypassed the combat-pending safe boundary")
	check(demo.advance_authoritative_tick(), "combat safe-boundary tick failed")
	check(demo.get_node("DrossWorldHost").get_movement_mode() == "combat",
			"safe boundary did not enter combat")
	check(hud.get_node("BottomBar/CombatActions").visible,
			"combat did not reveal the turn controls")
	check("Your turn" in
			hud.get_node("BottomBar/CombatActions/Content/TurnStatus").text,
			"combat UI did not project the authoritative active turn")
	check("Combat starts" in hud.get_log_text(),
			"player log did not announce the combat transition")
	check(demo.end_combat_turn(), "combat UI could not end the player turn")
	check(demo.get_node("DrossWorldHost").get_player_action_points() == 3,
			"enemy pass did not begin a fresh authoritative player turn")

	check(demo.get_node("DrossWorldHost").get_last_door_event() == "dross:door_opened",
			"door diagnostics did not consume the committed open event")
	check(is_equal_approx(demo.get_node("SideDoor").rotation_degrees.y, 90.0),
			"integrated door animation did not reach the committed open state")
	check(not demo.get_node("DrossWorldHost").is_door_presentation_pending(),
			"integrated door animation did not acknowledge completion")
	check(demo.get_node("DrossWorldHost").get_player_action_points() == 3,
			"integrated combat did not begin with authoritative player AP")
	check(demo.perform_thump_action(), "integrated demo rejected Thump")
	check(demo.get_node("DrossWorldHost").get_player_action_points() == 1,
			"integrated Thump did not spend authoritative player AP")
	check(demo.get_node("DrossWorldHost").get_recent_combat_events() ==
			PackedStringArray([
				"dross:ability_committed",
				"dross:damage_applied",
				"dross:actor_killed",
			]), "combat diagnostics did not preserve committed event order")
	check(demo.save_game(), "integrated post-combat save was rejected")
	check(demo.get_saved_state() != exploration_save,
			"changed integrated capabilities did not change canonical save bytes")
	check("player AP: 1" in demo.get_node("UI/Diagnostics").text,
			"integrated diagnostics did not expose committed player AP")
	check(demo.get_node("DrossWorldHost").is_mouse_killed(),
			"integrated demo did not commit mouse death")
	check(demo.get_node("DrossWorldHost").get_inventory_count(
			1, "thump_demo:mouse_tail") == 1,
			"tail-held inventory boundary did not survive save")
	check(demo.get_node("DrossWorldHost").get_quest_stage(
			"thump_demo:mouse_quest") == "thump_demo:return_tail",
			"return-tail quest boundary did not survive save")
	check(demo.get_node("DrossWorldHost").get_script_state_bool(
			"thump_demo:field_mouse", 2, "rule_checked"),
			"field mouse did not contribute its typed ability rule")
	check(demo.get_node("DrossWorldHost").get_script_state_bool(
			"thump_demo:field_mouse", 2, "attacked"),
			"field mouse did not react to committed damage")
	check(demo.get_node("DrossWorldHost").get_script_state_bool(
			"thump_demo:field_mouse", 2, "killed"),
			"field mouse did not react to committed death")
	check(demo.get_node("DrossWorldHost").get_script_state_int(
			"thump_demo:field_mouse", 2, "reaction_roll") >= 0,
			"field mouse reaction did not use deterministic RandomHub access")
	check("return_tail" in demo.get_node("UI/QuestStatus").text,
			"playable quest UI did not project the return stage")
	check("×1" in demo.get_node("UI/InventoryStatus").text,
			"playable inventory UI did not project the mouse tail")
	check(demo.talk_to_caretaker(), "playable scene rejected the valid mouse-tail hand-in")
	check(demo.get_node("DrossWorldHost").get_quest_status(
			"thump_demo:mouse_quest") == "completed",
			"playable scene did not complete the mouse quest")
	check("×0" in demo.get_node("UI/InventoryStatus").text,
			"playable inventory UI did not project the removed mouse tail")
	check(demo.save_game(), "integrated completed-quest save was rejected")
	var completed_hash: String = demo.get_node(
			"DrossWorldHost").get_canonical_capability_hash()
	check(demo.toggle_door(), "post-combat CloseDoor was rejected")
	check(demo.get_node("DrossWorldHost").get_last_door_event() == "dross:door_closed",
			"door diagnostics did not consume the committed close event")
	check("close the side door" in hud.get_log_text(),
			"player log did not describe the committed door closing")
	await create_timer(0.3).timeout
	check(not demo.get_node("DrossWorldHost").is_door_open(),
			"post-combat door mutation did not commit before reload")
	check(demo.load_game(), "integrated completed-state reload was rejected: %s" %
			demo.get_node("DrossWorldHost").get_last_load_error())
	check(demo.get_node("DrossWorldHost").get_canonical_capability_hash() == completed_hash,
			"completed-state reload did not restore the canonical capability hash")
	check(demo.get_node("DrossWorldHost").is_door_open(),
			"completed-state reload did not restore the authoritative door")
	check(demo.get_node("DrossWorldHost").get_player_action_points() == 1,
			"completed-state reload did not restore combat AP")
	check(demo.get_node("DrossWorldHost").is_mouse_killed(),
			"completed-state reload did not restore mouse death")
	check(demo.get_node("DrossWorldHost").get_quest_status(
			"thump_demo:mouse_quest") == "completed",
			"completed-state reload did not restore quest completion")
	check(demo.get_node("DrossWorldHost").get_inventory_count(
			1, "thump_demo:mouse_tail") == 0,
			"completed-state reload did not restore consumed inventory")
	check(not demo.get_node("FieldMouse").visible,
			"mouse view did not react to committed death")
	check(not demo.get_node("UI/Diagnostics").text.is_empty(),
			"integrated diagnostics panel was empty")

	demo.queue_free()
	if failures.is_empty():
		print("phase14 integrated vertical slice ok")
		quit(0)
	else:
		for failure in failures:
			push_error(failure)
		quit(1)

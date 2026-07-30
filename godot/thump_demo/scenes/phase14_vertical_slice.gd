extends Node3D

const TICK_SECONDS := 1.0 / 30.0
const MAX_CATCH_UP_TICKS := 4
const DEMO_SEED := 12345
const DOOR_ANIMATION_SECONDS := 0.25

@onready var host: DrossWorldHost = $DrossWorldHost
@onready var grid_overlay: DrossHexGridOverlay3D = $GridOverlay
@onready var player_view: DrossEntityView = $Player
@onready var mouse_view: MeshInstance3D = $FieldMouse
@onready var door_view: Node3D = $SideDoor
@onready var camera: Camera3D = $Camera3D
@onready var path_preview: Label = $UI/PathPreview
@onready var diagnostics: Label = $UI/Diagnostics
@onready var status: Label = $UI/Status
@onready var quest_status: Label = $UI/QuestStatus
@onready var inventory_status: Label = $UI/InventoryStatus
@onready var combat_panel: PanelContainer = $UI/CombatPanel
@onready var turn_status: Label = $UI/CombatPanel/Content/TurnStatus
@onready var thump_button: Button = $UI/CombatPanel/Content/Actions/Thump
@onready var end_turn_button: Button = $UI/CombatPanel/Content/Actions/EndTurn

var _accumulator := 0.0
var _destination_q := 0
var _destination_r := 0
var _last_command := "startup"
var _last_event := "world initialized"
var _selected_cell_facts := "none"
var _door_tween: Tween
var _combat_started := false
var _saved_state := PackedByteArray()


func _ready() -> void:
	if not host.start_movement_scenario():
		_fail_startup("movement scenario")
		return
	grid_overlay.compiled_map = host.get_movement_compiled_map()

	var mouse_module := DrossScriptModuleDefinition.new()
	mouse_module.module_id = "thump_demo:field_mouse"
	mouse_module.scope_kind = 1
	mouse_module.entity_sequence = 2
	mouse_module.state_schema_version = 1
	mouse_module.script = load("res://thump_demo/scripts/field_mouse.gd")
	var caretaker_module := DrossScriptModuleDefinition.new()
	caretaker_module.module_id = "thump_demo:caretaker_dialogue"
	caretaker_module.scope_kind = 1
	caretaker_module.entity_sequence = 3
	caretaker_module.state_schema_version = 1
	caretaker_module.script = load("res://thump_demo/scripts/caretaker_dialogue.gd")
	var quest_module := DrossScriptModuleDefinition.new()
	quest_module.module_id = "thump_demo:mouse_quest"
	quest_module.scope_kind = 0
	quest_module.state_schema_version = 1
	quest_module.script = load("res://thump_demo/scripts/mouse_quest.gd")
	var modules: Array[DrossScriptModuleDefinition] = [
		mouse_module, caretaker_module, quest_module
	]
	if not host.start_script_scenario(modules, DEMO_SEED):
		_fail_startup("ThumpDemo scripts")
		return

	var door := DrossDoorDefinition.new()
	door.door_id = "thump_demo:side_door"
	door.region_id = "thump_demo:room"
	door.from_q = 0
	door.from_r = 0
	door.to_q = 1
	door.to_r = 0
	if not host.start_door_scenario(door):
		_fail_startup("side door definition")
		return

	thump_button.pressed.connect(perform_thump_action)
	end_turn_button.pressed.connect(end_combat_turn)
	preview_destination(3, 0)
	_refresh_views()


func _process(delta: float) -> void:
	_accumulator += delta
	var ticks := 0
	while _accumulator >= TICK_SECONDS and ticks < MAX_CATCH_UP_TICKS:
		_accumulator -= TICK_SECONDS
		ticks += 1
		if host.get_movement_state() != "idle" or host.get_movement_mode() == "combat_pending":
			advance_authoritative_tick()
	if ticks == MAX_CATCH_UP_TICKS and _accumulator >= TICK_SECONDS:
		_accumulator = 0.0
		_last_event = "presentation catch-up budget exceeded"
	_refresh_diagnostics()


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion:
		var origin := camera.project_ray_origin(event.position)
		var direction := camera.project_ray_normal(event.position)
		var intersection = Plane(Vector3.UP, 0.0).intersects_ray(origin, direction)
		if intersection != null:
			hover_world_position(intersection)
	elif event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT:
		if event.pressed:
			request_previewed_move()
	elif event.is_action_pressed("ui_left"):
		preview_destination(maxi(0, _destination_q - 1), _destination_r)
	elif event.is_action_pressed("ui_right"):
		preview_destination(mini(3, _destination_q + 1), _destination_r)
	elif event.is_action_pressed("ui_up"):
		preview_destination(_destination_q, maxi(-1, _destination_r - 1))
	elif event.is_action_pressed("ui_down"):
		preview_destination(_destination_q, mini(1, _destination_r + 1))
	elif event.is_action_pressed("ui_accept"):
		request_previewed_move()
	elif event is InputEventKey and event.pressed and not event.echo:
		if event.keycode == KEY_T:
			perform_thump_action()
		elif event.keycode == KEY_D:
			toggle_door()
		elif event.keycode == KEY_S:
			save_game()
		elif event.keycode == KEY_L:
			load_game()
		elif event.keycode == KEY_C:
			talk_to_caretaker()
		elif event.keycode == KEY_ESCAPE:
			cancel_movement()


func hover_world_position(world_position: Vector3) -> bool:
	var destination := _world_to_hex(world_position)
	return preview_destination(clampi(destination.x, 0, 3), clampi(destination.y, -1, 1))


func preview_destination(destination_q: int, destination_r: int = 0) -> bool:
	var preview: DrossMovementPreview = host.preview_movement(destination_q, destination_r)
	_destination_q = destination_q
	_destination_r = destination_r
	if not preview.is_accepted():
		path_preview.text = "Cell (%d, %d) is unreachable" % [destination_q, destination_r]
		_selected_cell_facts = "unreachable"
		_refresh_diagnostics()
		return false
	var cells: PackedStringArray = []
	var columns := preview.get_path_columns()
	var rows := preview.get_path_rows()
	for index in range(columns.size()):
		cells.append("(%d,%d)" % [columns[index], rows[index]])
	path_preview.text = "Preview: %s  cost=%d  ticks=%d" % [
		" → ".join(cells),
		preview.get_cost(),
		preview.get_duration_ticks(),
	]
	grid_overlay.path_cell_keys = preview.get_path_cell_keys()
	_selected_cell_facts = _compiled_cell_facts(destination_q, destination_r)
	_refresh_diagnostics()
	return true


func request_previewed_move() -> bool:
	_last_command = "MoveTo(%d, %d)" % [_destination_q, _destination_r]
	var accepted := host.move_to(_destination_q, _destination_r)
	_last_event = "movement accepted" if accepted else "movement rejected"
	_refresh_diagnostics()
	return accepted


func cancel_movement() -> bool:
	_last_command = "CancelMovement"
	var accepted := host.cancel_movement()
	_last_event = "cancel queued for safe boundary" if accepted else "cancel rejected"
	return accepted


func advance_authoritative_tick() -> bool:
	var advanced := host.advance_movement_tick()
	if advanced:
		_last_event = "authoritative movement tick"
		_refresh_views()
		if host.get_movement_mode() == "combat" and not _ensure_combat_started():
			_last_event = "combat definition rejected"
	return advanced


func perform_thump_action() -> bool:
	_last_command = "PerformAbility(thump_demo:thump)"
	if not _ensure_combat_started():
		_last_event = "Thump unavailable outside combat"
		_refresh_diagnostics()
		return false
	var accepted := host.perform_thump()
	if accepted:
		_last_event = "Thump committed before presentation"
		mouse_view.visible = not host.is_mouse_killed()
		status.text = "THUMP — mouse HP %d; cue %s" % [
			host.get_mouse_health(), host.get_last_presentation_cue()
		]
	else:
		_last_event = "Thump rejected"
	_refresh_diagnostics()
	return accepted


func end_combat_turn() -> bool:
	_last_command = "EndTurn"
	var accepted := host.end_player_turn()
	_last_event = "player turn ended; mouse passed" if accepted else "End Turn rejected"
	_refresh_diagnostics()
	return accepted


func talk_to_caretaker() -> bool:
	_last_command = "TalkToCaretaker"
	var quest_state := host.get_quest_status("thump_demo:mouse_quest")
	var accepted := false
	if quest_state == "inactive":
		accepted = host.accept_mouse_quest_dialogue()
		_last_event = "mouse quest accepted" if accepted else "quest offer rejected"
	elif (
			quest_state == "active"
			and host.get_quest_stage("thump_demo:mouse_quest") == "thump_demo:return_tail"
			and host.get_inventory_count(1, "thump_demo:mouse_tail") > 0
	):
		accepted = host.hand_in_mouse_tail_dialogue()
		_last_event = "mouse quest completed" if accepted else "tail hand-in rejected"
	else:
		_last_event = "caretaker has no available option"
	_refresh_diagnostics()
	return accepted


func _ensure_combat_started() -> bool:
	if _combat_started:
		return true
	var thump := DrossAbilityDefinition.new()
	thump.ability_id = "thump_demo:thump"
	thump.range = 1
	thump.action_point_cost = 2
	thump.damage = 3
	thump.presentation_cue = "thump_demo:thump"
	if not host.start_thump_scenario(thump):
		return false
	_combat_started = true
	_refresh_diagnostics()
	return true


func toggle_door() -> bool:
	if host.is_door_presentation_pending():
		_last_event = "door presentation still running"
		_refresh_diagnostics()
		return false
	_last_command = "CloseDoor" if host.is_door_open() else "OpenDoor"
	var committed := host.close_door() if host.is_door_open() else host.open_door()
	if not committed:
		_last_event = "door command rejected"
		return false
	var target_degrees := 90.0 if host.is_door_open() else 0.0
	var acknowledgement_id := host.get_door_presentation_acknowledgement_id()
	_door_tween = create_tween()
	_door_tween.tween_property(
			door_view, "rotation_degrees:y", target_degrees, DOOR_ANIMATION_SECONDS)
	_door_tween.tween_callback(
			_finish_door_presentation.bind(target_degrees, acknowledgement_id))
	_last_event = "door state committed; presentation running"
	_refresh_diagnostics()
	return true


func _finish_door_presentation(target_degrees: float, acknowledgement_id: int) -> void:
	door_view.rotation_degrees.y = target_degrees
	if host.acknowledge_door_presentation(acknowledgement_id):
		_last_event = "door presentation complete"
	else:
		_last_event = "door presentation acknowledgement rejected"
	_refresh_diagnostics()


func save_game() -> bool:
	_last_command = "SaveGame"
	_saved_state = host.save_integrated_state()
	if _saved_state.is_empty():
		_last_event = "save rejected outside a supported boundary"
		_refresh_diagnostics()
		return false
	_last_event = "saved %d canonical bytes" % _saved_state.size()
	status.text = "Saved integrated state (%d bytes)" % _saved_state.size()
	_refresh_diagnostics()
	return true


func get_saved_state() -> PackedByteArray:
	return _saved_state


func load_game() -> bool:
	_last_command = "LoadGame"
	if not host.restore_integrated_state(_saved_state):
		_last_event = "load rejected: %s" % host.get_last_load_error()
		status.text = _last_event
		_refresh_diagnostics()
		return false
	_accumulator = 0.0
	_combat_started = host.get_player_action_points() >= 0
	mouse_view.visible = not host.is_mouse_killed()
	door_view.rotation_degrees.y = 90.0 if host.is_door_open() else 0.0
	_last_event = "save loaded; authoritative views reconstructed"
	status.text = "Loaded integrated state"
	_refresh_views()
	return true


func _refresh_views() -> void:
	var from := _hex_to_world(
		host.get_movement_column(), host.get_movement_row(), player_view.position.y)
	var to := _hex_to_world(
		host.get_movement_presentation_to_column(),
		host.get_movement_presentation_to_row(),
		player_view.position.y)
	player_view.apply_presentation_snapshot(
			from, to, host.get_movement_presentation_alpha())
	if (
		host.get_movement_column() == 2
		and host.get_movement_row() == 0
		and host.get_movement_mode() == "exploration"
	):
		if host.request_movement_combat():
			_last_command = "RequestCombatStart"
			_last_event = "combat pending until safe boundary"
	_refresh_diagnostics()


func _refresh_diagnostics() -> void:
	var quest_state := host.get_quest_status("thump_demo:mouse_quest")
	var quest_stage := host.get_quest_stage("thump_demo:mouse_quest")
	quest_status.text = "Quest: %s%s" % [
		quest_state,
		(" — %s" % quest_stage) if not quest_stage.is_empty() else "",
	]
	inventory_status.text = "Inventory: mouse tail ×%d" % host.get_inventory_count(
		1, "thump_demo:mouse_tail"
	)
	combat_panel.visible = _combat_started
	if _combat_started:
		if host.is_mouse_killed():
			turn_status.text = "Combat won — field mouse defeated"
		else:
			turn_status.text = "Your turn — %d AP" % host.get_player_action_points()
		thump_button.disabled = not host.is_player_turn()
		end_turn_button.disabled = not host.is_player_turn()
	diagnostics.text = "\n".join([
		"tick: %d" % host.get_movement_tick(),
		"mode: %s" % host.get_movement_mode(),
		"selected cell: q=%d r=%d" % [_destination_q, _destination_r],
		"selected facts: %s" % _selected_cell_facts,
		"actor cell: q=%d r=%d" % [
			host.get_movement_column(), host.get_movement_row()],
		"last command: %s" % _last_command,
		"last status: %s" % _last_event,
		"domain events: %s" % ", ".join(host.get_recent_movement_events()),
		"combat events: %s" % ", ".join(host.get_recent_combat_events()),
		"door event: %s" % host.get_last_door_event(),
		"script callback: %s" % host.get_script_call_order(),
		"seed: %d" % DEMO_SEED,
		"hash: %s" % host.get_canonical_capability_hash(),
		"door: %s" % ("open" if host.is_door_open() else "closed"),
		"player AP: %d" % host.get_player_action_points(),
		"mouse HP: %d" % host.get_mouse_health(),
	])


func _compiled_cell_facts(destination_q: int, destination_r: int) -> String:
	var compiled_map := host.get_movement_compiled_map()
	var cell_keys := compiled_map.get_cell_keys()
	var traversability := compiled_map.get_traversability()
	var provenance := compiled_map.get_provenance()
	var wanted := "dross:phase11:%d,%d,0" % [destination_q, destination_r]
	var index := cell_keys.find(wanted)
	if index < 0:
		return "unavailable"
	var traversal_text := "traversable" if traversability[index] != 0 else "blocked"
	var provenance_text := "automatic" if provenance[index] == 0 else "override"
	return "%s %s %s" % [cell_keys[index], traversal_text, provenance_text]


func _hex_to_world(q: int, r: int, y: float) -> Vector3:
	return Vector3(sqrt(3.0) * (float(q) + float(r) / 2.0), y, 1.5 * float(r))


func _world_to_hex(world_position: Vector3) -> Vector2i:
	var fractional_q := sqrt(3.0) / 3.0 * world_position.x - world_position.z / 3.0
	var fractional_r := 2.0 / 3.0 * world_position.z
	var cube_x := roundi(fractional_q)
	var cube_z := roundi(fractional_r)
	var cube_y := roundi(-fractional_q - fractional_r)
	var x_error := absf(float(cube_x) - fractional_q)
	var y_error := absf(float(cube_y) + fractional_q + fractional_r)
	var z_error := absf(float(cube_z) - fractional_r)
	if x_error > y_error and x_error > z_error:
		cube_x = -cube_y - cube_z
	elif y_error > z_error:
		cube_y = -cube_x - cube_z
	else:
		cube_z = -cube_x - cube_y
	return Vector2i(cube_x, cube_z)


func _fail_startup(surface: String) -> void:
	status.text = "Unable to start %s" % surface
	set_process(false)

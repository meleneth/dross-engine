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

var _accumulator := 0.0
var _destination_q := 0
var _last_command := "startup"
var _last_event := "world initialized"
var _selected_cell_facts := "none"
var _door_tween: Tween
var _combat_started := false


func _ready() -> void:
	if not host.start_movement_scenario():
		_fail_startup("movement scenario")
		return
	grid_overlay.compiled_map = host.get_movement_compiled_map()

	var mouse_module := DrossScriptModuleDefinition.new()
	mouse_module.module_id = "demo:field_mouse"
	mouse_module.scope_kind = 1
	mouse_module.entity_sequence = 2
	mouse_module.state_schema_version = 1
	mouse_module.script = load("res://scripts/phase12_field_mouse.gd")
	var mouse_modules: Array[DrossScriptModuleDefinition] = [mouse_module]
	if not host.start_script_scenario(mouse_modules, DEMO_SEED):
		_fail_startup("field mouse script")
		return

	var door := DrossDoorDefinition.new()
	door.door_id = "demo:side_door"
	door.region_id = "demo:room"
	door.from_q = 0
	door.from_r = 0
	door.to_q = 1
	door.to_r = 0
	if not host.start_door_scenario(door):
		_fail_startup("side door definition")
		return

	preview_destination(3)
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
		preview_destination(maxi(0, _destination_q - 1))
	elif event.is_action_pressed("ui_right"):
		preview_destination(mini(3, _destination_q + 1))
	elif event.is_action_pressed("ui_accept"):
		request_previewed_move()
	elif event is InputEventKey and event.pressed and not event.echo:
		if event.keycode == KEY_T:
			perform_thump_action()
		elif event.keycode == KEY_D:
			toggle_door()
		elif event.keycode == KEY_ESCAPE:
			cancel_movement()


func hover_world_position(world_position: Vector3) -> bool:
	var destination_q := clampi(roundi(world_position.x / sqrt(3.0)), 0, 3)
	return preview_destination(destination_q)


func preview_destination(destination_q: int) -> bool:
	var preview: DrossMovementPreview = host.preview_movement(destination_q)
	_destination_q = destination_q
	if not preview.is_accepted():
		path_preview.text = "Cell %d is unreachable" % destination_q
		_selected_cell_facts = "unreachable"
		_refresh_diagnostics()
		return false
	var columns: PackedStringArray = []
	for column in preview.get_path_columns():
		columns.append(str(column))
	path_preview.text = "Preview: %s  cost=%d  ticks=%d" % [
		" → ".join(columns),
		preview.get_cost(),
		preview.get_duration_ticks(),
	]
	grid_overlay.path_cell_keys = preview.get_path_cell_keys()
	_selected_cell_facts = _compiled_cell_facts(destination_q)
	_refresh_diagnostics()
	return true


func request_previewed_move() -> bool:
	_last_command = "MoveTo(%d)" % _destination_q
	var accepted := host.move_to(_destination_q)
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
	_last_command = "PerformAbility(dross_demo:thump)"
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


func _ensure_combat_started() -> bool:
	if _combat_started:
		return true
	var thump := DrossAbilityDefinition.new()
	thump.ability_id = "dross_demo:thump"
	thump.range = 1
	thump.action_point_cost = 2
	thump.damage = 3
	thump.presentation_cue = "dross_demo:thump"
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


func _refresh_views() -> void:
	var from := Vector3(
			sqrt(3.0) * float(host.get_movement_column()), player_view.position.y, 0.0)
	var to := Vector3(
			sqrt(3.0) * float(host.get_movement_presentation_to_column()),
			player_view.position.y,
			0.0)
	player_view.apply_presentation_snapshot(
			from, to, host.get_movement_presentation_alpha())
	if host.get_movement_column() == 2 and host.get_movement_mode() == "exploration":
		if host.request_movement_combat():
			_last_command = "RequestCombatStart"
			_last_event = "combat pending until safe boundary"
	_refresh_diagnostics()


func _refresh_diagnostics() -> void:
	diagnostics.text = "\n".join([
		"tick: %d" % host.get_movement_tick(),
		"mode: %s" % host.get_movement_mode(),
		"selected cell: q=%d r=0" % _destination_q,
		"selected facts: %s" % _selected_cell_facts,
		"actor cell: q=%d r=0" % host.get_movement_column(),
		"last command: %s" % _last_command,
		"last event: %s" % _last_event,
		"script callback: %s" % host.get_script_call_order(),
		"seed: %d" % DEMO_SEED,
		"hash: %s" % host.get_canonical_capability_hash(),
		"door: %s" % ("open" if host.is_door_open() else "closed"),
		"player AP: %d" % host.get_player_action_points(),
		"mouse HP: %d" % host.get_mouse_health(),
	])


func _compiled_cell_facts(destination_q: int) -> String:
	var compiled_map := host.get_movement_compiled_map()
	var cell_keys := compiled_map.get_cell_keys()
	var traversability := compiled_map.get_traversability()
	var provenance := compiled_map.get_provenance()
	if destination_q < 0 or destination_q >= cell_keys.size():
		return "unavailable"
	var traversal_text := "traversable" if traversability[destination_q] != 0 else "blocked"
	var provenance_text := "automatic" if provenance[destination_q] == 0 else "override"
	return "%s %s %s" % [cell_keys[destination_q], traversal_text, provenance_text]


func _fail_startup(surface: String) -> void:
	status.text = "Unable to start %s" % surface
	set_process(false)

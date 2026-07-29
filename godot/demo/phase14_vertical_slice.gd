extends Node3D

const TICK_SECONDS := 1.0 / 30.0
const MAX_CATCH_UP_TICKS := 4
const DEMO_SEED := 12345

@onready var host: DrossWorldHost = $DrossWorldHost
@onready var grid_overlay: DrossHexGridOverlay3D = $GridOverlay
@onready var player_view: MeshInstance3D = $Player
@onready var mouse_view: MeshInstance3D = $FieldMouse
@onready var door_view: Node3D = $SideDoor
@onready var path_preview: Label = $UI/PathPreview
@onready var diagnostics: Label = $UI/Diagnostics
@onready var status: Label = $UI/Status

var _accumulator := 0.0
var _destination_q := 0
var _last_command := "startup"
var _last_event := "world initialized"


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
	if event.is_action_pressed("ui_left"):
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


func preview_destination(destination_q: int) -> bool:
	var preview: DrossMovementPreview = host.preview_movement(destination_q)
	_destination_q = destination_q
	if not preview.is_accepted():
		path_preview.text = "Cell %d is unreachable" % destination_q
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
	return advanced


func perform_thump_action() -> bool:
	_last_command = "PerformAbility(dross_demo:thump)"
	var thump := DrossAbilityDefinition.new()
	thump.ability_id = "dross_demo:thump"
	thump.range = 1
	thump.action_point_cost = 2
	thump.damage = 3
	thump.presentation_cue = "dross_demo:thump"
	if not host.start_thump_scenario(thump):
		_last_event = "Thump definition rejected"
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


func toggle_door() -> bool:
	_last_command = "CloseDoor" if host.is_door_open() else "OpenDoor"
	var committed := host.close_door() if host.is_door_open() else host.open_door()
	if not committed:
		_last_event = "door command rejected"
		return false
	door_view.rotation_degrees.y = 90.0 if host.is_door_open() else 0.0
	host.acknowledge_door_presentation(host.get_door_presentation_acknowledgement_id())
	_last_event = "door state committed"
	_refresh_diagnostics()
	return true


func _refresh_views() -> void:
	player_view.position.x = sqrt(3.0) * float(host.get_movement_column())
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
		"actor cell: q=%d r=0" % host.get_movement_column(),
		"last command: %s" % _last_command,
		"last event: %s" % _last_event,
		"script callback: %s" % host.get_script_call_order(),
		"seed: %d" % DEMO_SEED,
		"door: %s" % ("open" if host.is_door_open() else "closed"),
		"mouse HP: %d" % host.get_mouse_health(),
	])


func _fail_startup(surface: String) -> void:
	status.text = "Unable to start %s" % surface
	set_process(false)

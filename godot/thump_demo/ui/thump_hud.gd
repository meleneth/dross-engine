extends Control

signal thump_requested
signal end_turn_requested

const MAX_LOG_ENTRIES := 8

@onready var messages: RichTextLabel = $BottomBar/PlayerLog/Content/Messages
@onready var combat_actions: PanelContainer = $BottomBar/CombatActions
@onready var turn_status: Label = $BottomBar/CombatActions/Content/TurnStatus
@onready var thump_button: Button = $BottomBar/CombatActions/Content/Actions/Thump
@onready var end_turn_button: Button = $BottomBar/CombatActions/Content/Actions/EndTurn

var _entries: Array[String] = []


func _ready() -> void:
	thump_button.pressed.connect(func() -> void: thump_requested.emit())
	end_turn_button.pressed.connect(func() -> void: end_turn_requested.emit())


func add_log(message: String) -> void:
	if message.is_empty():
		return
	_entries.append(message)
	if _entries.size() > MAX_LOG_ENTRIES:
		_entries.pop_front()
	messages.text = "\n".join(_entries)
	messages.scroll_to_line(maxi(0, messages.get_line_count() - 1))


func get_log_text() -> String:
	return "\n".join(_entries)


func set_combat_state(active: bool, summary: String, can_act: bool) -> void:
	combat_actions.visible = active
	if not active:
		return
	turn_status.text = summary
	thump_button.disabled = not can_act
	end_turn_button.disabled = not can_act

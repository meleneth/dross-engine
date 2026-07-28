@tool
extends EditorPlugin

var grid_gizmo: EditorNode3DGizmoPlugin
var panel: VBoxContainer
var status: Label
var q_input: SpinBox
var r_input: SpinBox
var selected_region: DrossHexGridRegion3D


func _enter_tree() -> void:
	grid_gizmo = preload("res://addons/dross_editor/grid_gizmo.gd").new()
	add_node_3d_gizmo_plugin(grid_gizmo)
	panel = VBoxContainer.new()
	panel.name = "Dross Grid"
	status = Label.new()
	status.text = "Select a DrossHexGridRegion3D"
	panel.add_child(status)
	var bake_button := Button.new()
	bake_button.text = "Bake Geometry"
	bake_button.pressed.connect(_bake)
	panel.add_child(bake_button)
	q_input = SpinBox.new()
	q_input.prefix = "Cell q: "
	panel.add_child(q_input)
	r_input = SpinBox.new()
	r_input.prefix = "Cell r: "
	panel.add_child(r_input)
	var traversable_button := Button.new()
	traversable_button.text = "Force Traversable"
	traversable_button.pressed.connect(_force_traversable)
	panel.add_child(traversable_button)
	var blocked_button := Button.new()
	blocked_button.text = "Force Blocked"
	blocked_button.pressed.connect(_force_blocked)
	panel.add_child(blocked_button)
	var compile_button := Button.new()
	compile_button.text = "Compile Runtime Map"
	compile_button.pressed.connect(_compile)
	panel.add_child(compile_button)
	add_control_to_dock(DOCK_SLOT_RIGHT_UL, panel)
	get_editor_interface().get_selection().selection_changed.connect(_selection_changed)


func _exit_tree() -> void:
	if grid_gizmo != null:
		remove_node_3d_gizmo_plugin(grid_gizmo)
		grid_gizmo = null
	if panel != null:
		remove_control_from_docks(panel)
		panel.queue_free()
		panel = null


func is_grid_gizmo_registered() -> bool:
	return grid_gizmo != null


func _selection_changed() -> void:
	selected_region = null
	for node in get_editor_interface().get_selection().get_selected_nodes():
		if node is DrossHexGridRegion3D:
			selected_region = node
			break
	status.text = (
			"Ready: " + selected_region.region_id
			if selected_region != null
			else "Select a DrossHexGridRegion3D"
	)


func _bake() -> void:
	if selected_region == null:
		status.text = "Bake rejected: no grid region selected"
		return
	var bake: DrossHexGridBake = selected_region.bake_geometry()
	status.text = "Baked %d cells" % bake.cell_count
	selected_region.update_gizmos()


func _force_traversable() -> void:
	_set_override(1)


func _force_blocked() -> void:
	_set_override(2)


func _set_override(value: int) -> void:
	if selected_region == null:
		status.text = "Override rejected: no grid region selected"
		return
	if not selected_region.overrides.set_cell_override(
			int(q_input.value), int(r_input.value), 0, value
	):
		status.text = "Override rejected: invalid cell address"
		return
	status.text = "Stored override for %s" % selected_region.select_cell_at_local(
			selected_region.cell_center(int(q_input.value), int(r_input.value)))


func _compile() -> void:
	if selected_region == null:
		status.text = "Compile rejected: no grid region selected"
		return
	var compiled: DrossCompiledHexMap = selected_region.compile_map()
	status.text = (
			"Compiled %d cells and %d edges" % [compiled.cell_count, compiled.edge_count]
			if compiled.cell_count > 0
			else "Compile rejected: inspect bake identity and orphan overrides"
	)

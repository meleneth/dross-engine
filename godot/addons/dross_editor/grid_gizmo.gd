@tool
extends EditorNode3DGizmoPlugin

const GRID_MATERIAL := "dross_grid"


func _init() -> void:
	create_material(GRID_MATERIAL, Color(0.25, 0.8, 1.0, 0.9))


func _get_gizmo_name() -> String:
	return "Dross Hex Grid"


func _has_gizmo(for_node_3d: Node3D) -> bool:
	return for_node_3d is DrossHexGridRegion3D


func _redraw(gizmo: EditorNode3DGizmo) -> void:
	gizmo.clear()
	var region := gizmo.get_node_3d() as DrossHexGridRegion3D
	if region == null:
		return
	var lines := PackedVector3Array()
	for r in range(region.r_min, region.r_max + 1):
		for q in range(region.q_min, region.q_max + 1):
			var center: Vector3 = region.cell_center(q, r)
			for side in range(6):
				var first_angle := PI / 6.0 + PI / 3.0 * side
				var second_angle := PI / 6.0 + PI / 3.0 * ((side + 1) % 6)
				lines.push_back(center + Vector3(
						cos(first_angle) * region.cell_radius, 0.02,
						sin(first_angle) * region.cell_radius))
				lines.push_back(center + Vector3(
						cos(second_angle) * region.cell_radius, 0.02,
						sin(second_angle) * region.cell_radius))
	gizmo.add_lines(lines, get_material(GRID_MATERIAL, gizmo), false)

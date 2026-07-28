#pragma once

#include "dross_grid_resources.hpp"

#include <godot_cpp/classes/immediate_mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>

namespace dross::godot_adapter {

class DrossHexGridOverlay3D final : public godot::MeshInstance3D {
  GDCLASS(DrossHexGridOverlay3D, godot::MeshInstance3D)

public:
  DrossHexGridOverlay3D();
  void set_compiled_map(const godot::Ref<DrossCompiledHexMap>& value);
  [[nodiscard]] godot::Ref<DrossCompiledHexMap> get_compiled_map() const { return compiled_; }
  void set_cell_radius(double value);
  [[nodiscard]] double get_cell_radius() const { return cell_radius_; }
  void rebuild();
  [[nodiscard]] godot::PackedStringArray get_cell_keys() const;

protected:
  static void _bind_methods();

private:
  godot::Ref<DrossCompiledHexMap> compiled_;
  godot::Ref<godot::ImmediateMesh> mesh_;
  double cell_radius_{1.0};
};

} // namespace dross::godot_adapter

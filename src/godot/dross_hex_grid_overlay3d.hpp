#pragma once

#include "dross_grid_resources.hpp"

#include <godot_cpp/classes/immediate_mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>

#include <cstdint>

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
  void set_path_cell_keys(const godot::PackedStringArray& value);
  [[nodiscard]] godot::PackedStringArray get_path_cell_keys() const;
  void set_hover_cell(const godot::String& cell_key, std::int64_t state);
  [[nodiscard]] godot::String get_hover_cell_key() const { return hover_cell_key_; }
  [[nodiscard]] std::int64_t get_hover_state() const { return hover_state_; }

protected:
  static void _bind_methods();

private:
  godot::Ref<DrossCompiledHexMap> compiled_;
  godot::Ref<godot::ImmediateMesh> mesh_;
  godot::PackedStringArray path_cell_keys_;
  godot::String hover_cell_key_;
  std::int64_t hover_state_{0};
  double cell_radius_{1.0};
};

} // namespace dross::godot_adapter

#include "dross_hex_grid_overlay3d.hpp"

#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <array>
#include <cmath>

namespace dross::godot_adapter {
namespace {

constexpr double pi = 3.14159265358979323846;

godot::Vector3 center_for(const HexCellId& cell, const double radius) {
  return godot::Vector3{
      static_cast<godot::real_t>(
          std::sqrt(3.0) * radius *
          (static_cast<double>(cell.coord.q) + static_cast<double>(cell.coord.r) / 2.0)),
      static_cast<godot::real_t>(static_cast<double>(cell.layer) * 0.02),
      static_cast<godot::real_t>(1.5 * radius * static_cast<double>(cell.coord.r))};
}

} // namespace

DrossHexGridOverlay3D::DrossHexGridOverlay3D() {
  mesh_.instantiate();
  set_mesh(mesh_);
}

void DrossHexGridOverlay3D::set_compiled_map(const godot::Ref<DrossCompiledHexMap>& value) {
  compiled_ = value;
  rebuild();
}

void DrossHexGridOverlay3D::set_cell_radius(const double value) {
  if (value > 0.0) {
    cell_radius_ = value;
    rebuild();
  }
}

void DrossHexGridOverlay3D::rebuild() {
  mesh_->clear_surfaces();
  if (compiled_.is_null() || !compiled_->core()) {
    return;
  }
  mesh_->surface_begin(godot::Mesh::PRIMITIVE_LINES);
  for (const auto& cell : compiled_->core()->map.cell_ids()) {
    const auto center = center_for(cell, cell_radius_);
    const auto facts = compiled_->core()->map.cell(cell).value();
    const auto color = facts.traversable ? godot::Color{0.2F, 0.9F, 0.4F, 1.0F}
                                         : godot::Color{0.9F, 0.2F, 0.2F, 1.0F};
    std::array<godot::Vector3, 6> corners{};
    for (std::size_t index = 0; index < corners.size(); ++index) {
      const auto angle = pi / 6.0 + pi / 3.0 * static_cast<double>(index);
      corners[index] =
          center + godot::Vector3{static_cast<godot::real_t>(std::cos(angle) * cell_radius_), 0.02F,
                                  static_cast<godot::real_t>(std::sin(angle) * cell_radius_)};
    }
    for (std::size_t index = 0; index < corners.size(); ++index) {
      mesh_->surface_set_color(color);
      mesh_->surface_add_vertex(corners[index]);
      mesh_->surface_set_color(color);
      mesh_->surface_add_vertex(corners[(index + 1) % corners.size()]);
    }
  }
  mesh_->surface_end();
}

godot::PackedStringArray DrossHexGridOverlay3D::get_cell_keys() const {
  return compiled_.is_valid() ? compiled_->get_cell_keys() : godot::PackedStringArray{};
}

void DrossHexGridOverlay3D::_bind_methods() {
  godot::ClassDB::bind_method(godot::D_METHOD("set_compiled_map", "value"),
                              &DrossHexGridOverlay3D::set_compiled_map);
  godot::ClassDB::bind_method(godot::D_METHOD("get_compiled_map"),
                              &DrossHexGridOverlay3D::get_compiled_map);
  godot::ClassDB::bind_method(godot::D_METHOD("set_cell_radius", "value"),
                              &DrossHexGridOverlay3D::set_cell_radius);
  godot::ClassDB::bind_method(godot::D_METHOD("get_cell_radius"),
                              &DrossHexGridOverlay3D::get_cell_radius);
  godot::ClassDB::bind_method(godot::D_METHOD("rebuild"), &DrossHexGridOverlay3D::rebuild);
  godot::ClassDB::bind_method(godot::D_METHOD("get_cell_keys"),
                              &DrossHexGridOverlay3D::get_cell_keys);
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::OBJECT, "compiled_map",
                                   godot::PROPERTY_HINT_RESOURCE_TYPE, "DrossCompiledHexMap"),
               "set_compiled_map", "get_compiled_map");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT, "cell_radius"), "set_cell_radius",
               "get_cell_radius");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_STRING_ARRAY, "cell_keys"), "",
               "get_cell_keys");
}

} // namespace dross::godot_adapter

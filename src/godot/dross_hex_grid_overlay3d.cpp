#include "dross_hex_grid_overlay3d.hpp"

#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

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

godot::String key_for(const HexCellId& cell) {
  const auto key = std::string{cell.region.content_id().canonical()} + ":" +
                   std::to_string(cell.coord.q) + "," + std::to_string(cell.coord.r) + "," +
                   std::to_string(cell.layer);
  return godot::String{key.c_str()};
}

} // namespace

DrossHexGridOverlay3D::DrossHexGridOverlay3D() {
  mesh_.instantiate();
  material_.instantiate();
  material_->set_shading_mode(godot::BaseMaterial3D::SHADING_MODE_UNSHADED);
  material_->set_flag(godot::BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
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
  mesh_->surface_begin(godot::Mesh::PRIMITIVE_LINES, material_);
  for (const auto& cell : compiled_->core()->map.cell_ids()) {
    const auto center = center_for(cell, cell_radius_);
    const auto facts = compiled_->core()->map.cell(cell).value();
    const auto key = key_for(cell);
    const auto hovered = key == hover_cell_key_;
    const auto on_path = path_cell_keys_.has(key);
    const auto color = hovered && hover_state_ == 1   ? godot::Color::html("5ab552")
                       : hovered && hover_state_ == 2 ? godot::Color::html("ec273f")
                       : on_path                      ? godot::Color::html("f3a833")
                       : facts.traversable            ? godot::Color::html("5e5b8c")
                                                      : godot::Color::html("3e3b65");
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

void DrossHexGridOverlay3D::set_path_cell_keys(const godot::PackedStringArray& value) {
  path_cell_keys_ = value;
  rebuild();
}

godot::PackedStringArray DrossHexGridOverlay3D::get_path_cell_keys() const {
  return path_cell_keys_;
}

void DrossHexGridOverlay3D::set_hover_cell(const godot::String& cell_key,
                                           const std::int64_t state) {
  hover_cell_key_ = cell_key;
  hover_state_ = cell_key.is_empty() ? 0 : std::clamp<std::int64_t>(state, 0, 2);
  rebuild();
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
  godot::ClassDB::bind_method(godot::D_METHOD("set_path_cell_keys", "value"),
                              &DrossHexGridOverlay3D::set_path_cell_keys);
  godot::ClassDB::bind_method(godot::D_METHOD("get_path_cell_keys"),
                              &DrossHexGridOverlay3D::get_path_cell_keys);
  godot::ClassDB::bind_method(godot::D_METHOD("set_hover_cell", "cell_key", "state"),
                              &DrossHexGridOverlay3D::set_hover_cell);
  godot::ClassDB::bind_method(godot::D_METHOD("get_hover_cell_key"),
                              &DrossHexGridOverlay3D::get_hover_cell_key);
  godot::ClassDB::bind_method(godot::D_METHOD("get_hover_state"),
                              &DrossHexGridOverlay3D::get_hover_state);
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::OBJECT, "compiled_map",
                                   godot::PROPERTY_HINT_RESOURCE_TYPE, "DrossCompiledHexMap"),
               "set_compiled_map", "get_compiled_map");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT, "cell_radius"), "set_cell_radius",
               "get_cell_radius");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_STRING_ARRAY, "cell_keys"), "",
               "get_cell_keys");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_STRING_ARRAY, "path_cell_keys"),
               "set_path_cell_keys", "get_path_cell_keys");
}

} // namespace dross::godot_adapter

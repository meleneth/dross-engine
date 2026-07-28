#include "dross_hex_grid_region3d.hpp"

#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <array>
#include <cmath>
#include <cstdint>

namespace dross::godot_adapter {
namespace {

constexpr double millimeters_per_meter = 1000.0;
constexpr double probe_height = 5.0;
constexpr double inset_fraction = 0.55;
constexpr double pi = 3.14159265358979323846;

godot::Vector3 axial_center(const std::int64_t q, const std::int64_t r, const double radius) {
  return godot::Vector3{
      static_cast<godot::real_t>(std::sqrt(3.0) * radius *
                                 (static_cast<double>(q) + static_cast<double>(r) / 2.0)),
      0.0F, static_cast<godot::real_t>(1.5 * radius * static_cast<double>(r))};
}

} // namespace

godot::Ref<DrossHexGridBake> RaycastGeometryAnalyzer::bake(DrossHexGridRegion3D& region) const {
  godot::Ref<DrossHexGridBake> output;
  output.instantiate();
  output->set_region_id(region.region_id_);
  output->set_radius_mm(
      static_cast<std::int64_t>(std::llround(region.cell_radius_ * millimeters_per_meter)));
  if (!region.is_inside_tree() || region.profile_.is_null()) {
    return output;
  }
  const auto profile = region.profile_->compile_core();
  auto* state = region.get_world_3d()->get_direct_space_state();
  if (!profile || !state || profile->required_sample_count != 7) {
    return output;
  }

  godot::PackedInt32Array coordinates;
  godot::PackedInt32Array samples;
  godot::PackedByteArray clearance;
  for (auto r = region.r_min_; r <= region.r_max_; ++r) {
    for (auto q = region.q_min_; q <= region.q_max_; ++q) {
      const auto center = axial_center(q, r, region.cell_radius_);
      std::array<godot::Vector3, 7> offsets{};
      offsets[0] = {};
      for (std::size_t index = 0; index < 6; ++index) {
        const auto angle = pi / 3.0 * static_cast<double>(index);
        offsets[index + 1] = godot::Vector3{
            static_cast<godot::real_t>(std::cos(angle) * region.cell_radius_ * inset_fraction),
            0.0F,
            static_cast<godot::real_t>(std::sin(angle) * region.cell_radius_ * inset_fraction)};
      }
      std::array<std::int32_t, 7> cell_samples{};
      bool supported = true;
      bool standing_clearance = true;
      for (std::size_t index = 0; index < offsets.size(); ++index) {
        const auto local = center + offsets[index];
        const auto from = region.to_global(local + godot::Vector3{0.0F, probe_height, 0.0F});
        const auto to = region.to_global(local - godot::Vector3{0.0F, probe_height, 0.0F});
        auto query = godot::PhysicsRayQueryParameters3D::create(
            from, to, static_cast<std::uint32_t>(region.collision_mask_));
        const auto hit = state->intersect_ray(query);
        if (hit.is_empty()) {
          supported = false;
          break;
        }
        const godot::Vector3 position = hit["position"];
        const auto local_hit = region.to_local(position);
        cell_samples[index] = static_cast<std::int32_t>(
            std::llround(static_cast<double>(local_hit.y) * millimeters_per_meter));

        const auto clearance_from =
            position + region.get_global_transform().basis.get_column(1).normalized() * 0.02F;
        const auto clearance_to =
            clearance_from +
            region.get_global_transform().basis.get_column(1).normalized() *
                static_cast<godot::real_t>(
                    static_cast<double>(region.profile_->get_required_clearance_mm()) /
                    millimeters_per_meter);
        auto clearance_query = godot::PhysicsRayQueryParameters3D::create(
            clearance_from, clearance_to, static_cast<std::uint32_t>(region.collision_mask_));
        if (!state->intersect_ray(clearance_query).is_empty()) {
          standing_clearance = false;
        }
      }
      if (!supported) {
        continue;
      }
      coordinates.push_back(static_cast<std::int32_t>(q));
      coordinates.push_back(static_cast<std::int32_t>(r));
      coordinates.push_back(0);
      for (const auto sample : cell_samples) {
        samples.push_back(sample);
      }
      clearance.push_back(standing_clearance ? 1U : 0U);
    }
  }
  output->set_cell_coordinates(coordinates);
  output->set_surface_samples_mm(samples);
  output->set_standing_clearance(clearance);
  return output;
}

DrossHexGridRegion3D::DrossHexGridRegion3D()
    : analyzer_{std::make_unique<RaycastGeometryAnalyzer>()} {
  profile_.instantiate();
  overrides_.instantiate();
}

DrossHexGridRegion3D::~DrossHexGridRegion3D() = default;

godot::Ref<DrossHexGridBake> DrossHexGridRegion3D::bake_geometry() {
  last_bake_ = analyzer_->bake(*this);
  return last_bake_;
}

godot::Ref<DrossCompiledHexMap> DrossHexGridRegion3D::compile_map() {
  godot::Ref<DrossCompiledHexMap> compiled;
  compiled.instantiate();
  if (last_bake_.is_null() || profile_.is_null() || overrides_.is_null()) {
    return compiled;
  }
  static_cast<void>(compiled->compile_from(last_bake_, overrides_, profile_));
  return compiled;
}

void DrossHexGridRegion3D::_bind_methods() {
  godot::ClassDB::bind_method(godot::D_METHOD("set_region_id", "value"),
                              &DrossHexGridRegion3D::set_region_id);
  godot::ClassDB::bind_method(godot::D_METHOD("get_region_id"),
                              &DrossHexGridRegion3D::get_region_id);
  godot::ClassDB::bind_method(godot::D_METHOD("set_cell_radius", "value"),
                              &DrossHexGridRegion3D::set_cell_radius);
  godot::ClassDB::bind_method(godot::D_METHOD("get_cell_radius"),
                              &DrossHexGridRegion3D::get_cell_radius);
  godot::ClassDB::bind_method(godot::D_METHOD("set_q_min", "value"),
                              &DrossHexGridRegion3D::set_q_min);
  godot::ClassDB::bind_method(godot::D_METHOD("get_q_min"), &DrossHexGridRegion3D::get_q_min);
  godot::ClassDB::bind_method(godot::D_METHOD("set_q_max", "value"),
                              &DrossHexGridRegion3D::set_q_max);
  godot::ClassDB::bind_method(godot::D_METHOD("get_q_max"), &DrossHexGridRegion3D::get_q_max);
  godot::ClassDB::bind_method(godot::D_METHOD("set_r_min", "value"),
                              &DrossHexGridRegion3D::set_r_min);
  godot::ClassDB::bind_method(godot::D_METHOD("get_r_min"), &DrossHexGridRegion3D::get_r_min);
  godot::ClassDB::bind_method(godot::D_METHOD("set_r_max", "value"),
                              &DrossHexGridRegion3D::set_r_max);
  godot::ClassDB::bind_method(godot::D_METHOD("get_r_max"), &DrossHexGridRegion3D::get_r_max);
  godot::ClassDB::bind_method(godot::D_METHOD("set_collision_mask", "value"),
                              &DrossHexGridRegion3D::set_collision_mask);
  godot::ClassDB::bind_method(godot::D_METHOD("get_collision_mask"),
                              &DrossHexGridRegion3D::get_collision_mask);
  godot::ClassDB::bind_method(godot::D_METHOD("set_bake_profile", "value"),
                              &DrossHexGridRegion3D::set_bake_profile);
  godot::ClassDB::bind_method(godot::D_METHOD("get_bake_profile"),
                              &DrossHexGridRegion3D::get_bake_profile);
  godot::ClassDB::bind_method(godot::D_METHOD("set_overrides", "value"),
                              &DrossHexGridRegion3D::set_overrides);
  godot::ClassDB::bind_method(godot::D_METHOD("get_overrides"),
                              &DrossHexGridRegion3D::get_overrides);
  godot::ClassDB::bind_method(godot::D_METHOD("bake_geometry"),
                              &DrossHexGridRegion3D::bake_geometry);
  godot::ClassDB::bind_method(godot::D_METHOD("compile_map"), &DrossHexGridRegion3D::compile_map);
  godot::ClassDB::bind_method(godot::D_METHOD("get_last_bake"),
                              &DrossHexGridRegion3D::get_last_bake);
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::STRING, "region_id"), "set_region_id",
               "get_region_id");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT, "cell_radius"), "set_cell_radius",
               "get_cell_radius");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "q_min"), "set_q_min", "get_q_min");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "q_max"), "set_q_max", "get_q_max");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "r_min"), "set_r_min", "get_r_min");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "r_max"), "set_r_max", "get_r_max");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "collision_mask",
                                   godot::PROPERTY_HINT_LAYERS_3D_PHYSICS),
               "set_collision_mask", "get_collision_mask");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::OBJECT, "bake_profile",
                                   godot::PROPERTY_HINT_RESOURCE_TYPE, "DrossHexBakeProfile"),
               "set_bake_profile", "get_bake_profile");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::OBJECT, "overrides",
                                   godot::PROPERTY_HINT_RESOURCE_TYPE, "DrossHexGridOverrides"),
               "set_overrides", "get_overrides");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::OBJECT, "last_bake",
                                   godot::PROPERTY_HINT_RESOURCE_TYPE, "DrossHexGridBake"),
               "", "get_last_bake");
}

} // namespace dross::godot_adapter

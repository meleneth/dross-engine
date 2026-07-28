#pragma once

#include "dross_grid_resources.hpp"

#include <godot_cpp/classes/node3d.hpp>

#include <cstdint>
#include <memory>

namespace dross::godot_adapter {

class GeometryAnalyzer {
public:
  virtual ~GeometryAnalyzer() = default;
  [[nodiscard]] virtual godot::Ref<DrossHexGridBake>
  bake(class DrossHexGridRegion3D& region) const = 0;
};

class RaycastGeometryAnalyzer final : public GeometryAnalyzer {
public:
  [[nodiscard]] godot::Ref<DrossHexGridBake> bake(DrossHexGridRegion3D& region) const override;
};

class DrossHexGridRegion3D final : public godot::Node3D {
  GDCLASS(DrossHexGridRegion3D, godot::Node3D)

public:
  DrossHexGridRegion3D();
  ~DrossHexGridRegion3D() override;

  void set_region_id(const godot::String& value) { region_id_ = value; }
  [[nodiscard]] godot::String get_region_id() const { return region_id_; }
  void set_cell_radius(double value) { cell_radius_ = value; }
  [[nodiscard]] double get_cell_radius() const { return cell_radius_; }
  void set_q_min(std::int64_t value) { q_min_ = value; }
  [[nodiscard]] std::int64_t get_q_min() const { return q_min_; }
  void set_q_max(std::int64_t value) { q_max_ = value; }
  [[nodiscard]] std::int64_t get_q_max() const { return q_max_; }
  void set_r_min(std::int64_t value) { r_min_ = value; }
  [[nodiscard]] std::int64_t get_r_min() const { return r_min_; }
  void set_r_max(std::int64_t value) { r_max_ = value; }
  [[nodiscard]] std::int64_t get_r_max() const { return r_max_; }
  void set_collision_mask(std::int64_t value) { collision_mask_ = value; }
  [[nodiscard]] std::int64_t get_collision_mask() const { return collision_mask_; }
  void set_optional_door_edge(const godot::String& value) { optional_door_edge_ = value; }
  [[nodiscard]] godot::String get_optional_door_edge() const { return optional_door_edge_; }
  void set_bake_profile(const godot::Ref<DrossHexBakeProfile>& value) { profile_ = value; }
  [[nodiscard]] godot::Ref<DrossHexBakeProfile> get_bake_profile() const { return profile_; }
  void set_overrides(const godot::Ref<DrossHexGridOverrides>& value) { overrides_ = value; }
  [[nodiscard]] godot::Ref<DrossHexGridOverrides> get_overrides() const { return overrides_; }
  [[nodiscard]] godot::Ref<DrossHexGridBake> bake_geometry();
  [[nodiscard]] godot::Ref<DrossCompiledHexMap> compile_map();
  [[nodiscard]] godot::Ref<DrossHexGridBake> get_last_bake() const { return last_bake_; }
  [[nodiscard]] godot::Vector3 cell_center(std::int64_t q, std::int64_t r) const;
  [[nodiscard]] godot::String select_cell_at_local(const godot::Vector3& point) const;

protected:
  static void _bind_methods();

private:
  friend class RaycastGeometryAnalyzer;
  godot::String region_id_{"demo:room"};
  double cell_radius_{1.0};
  std::int64_t q_min_{-1};
  std::int64_t q_max_{1};
  std::int64_t r_min_{-1};
  std::int64_t r_max_{1};
  std::int64_t collision_mask_{1};
  godot::String optional_door_edge_;
  godot::Ref<DrossHexBakeProfile> profile_;
  godot::Ref<DrossHexGridOverrides> overrides_;
  godot::Ref<DrossHexGridBake> last_bake_;
  std::unique_ptr<GeometryAnalyzer> analyzer_;
};

} // namespace dross::godot_adapter

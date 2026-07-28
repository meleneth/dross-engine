#pragma once

#include <dross/hex/grid_bake.hpp>

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include <cstdint>
#include <optional>

namespace dross::godot_adapter {

class DrossHexBakeProfile final : public godot::Resource {
  GDCLASS(DrossHexBakeProfile, godot::Resource)

public:
  void set_quantization_mm(std::int64_t value) { quantization_mm_ = value; }
  [[nodiscard]] std::int64_t get_quantization_mm() const { return quantization_mm_; }
  void set_maximum_height_variance_mm(std::int64_t value) { maximum_height_variance_mm_ = value; }
  [[nodiscard]] std::int64_t get_maximum_height_variance_mm() const {
    return maximum_height_variance_mm_;
  }
  void set_required_sample_count(std::int64_t value) { required_sample_count_ = value; }
  [[nodiscard]] std::int64_t get_required_sample_count() const { return required_sample_count_; }
  void set_required_clearance_mm(std::int64_t value) { required_clearance_mm_ = value; }
  [[nodiscard]] std::int64_t get_required_clearance_mm() const { return required_clearance_mm_; }
  [[nodiscard]] std::optional<HexBakeProfile> compile_core() const;

protected:
  static void _bind_methods();

private:
  std::int64_t quantization_mm_{10};
  std::int64_t maximum_height_variance_mm_{30};
  std::int64_t required_sample_count_{7};
  std::int64_t required_clearance_mm_{1800};
};

class DrossHexGridBake final : public godot::Resource {
  GDCLASS(DrossHexGridBake, godot::Resource)

public:
  void set_region_id(const godot::String& value) { region_id_ = value; }
  [[nodiscard]] godot::String get_region_id() const { return region_id_; }
  void set_radius_mm(std::int64_t value) { radius_mm_ = value; }
  [[nodiscard]] std::int64_t get_radius_mm() const { return radius_mm_; }
  void set_cell_coordinates(const godot::PackedInt32Array& value) { cell_coordinates_ = value; }
  [[nodiscard]] godot::PackedInt32Array get_cell_coordinates() const { return cell_coordinates_; }
  void set_surface_samples_mm(const godot::PackedInt32Array& value) { surface_samples_mm_ = value; }
  [[nodiscard]] godot::PackedInt32Array get_surface_samples_mm() const {
    return surface_samples_mm_;
  }
  void set_standing_clearance(const godot::PackedByteArray& value) { standing_clearance_ = value; }
  [[nodiscard]] godot::PackedByteArray get_standing_clearance() const {
    return standing_clearance_;
  }
  [[nodiscard]] std::int64_t get_cell_count() const;
  [[nodiscard]] std::optional<GridBake> compile_core(std::uint32_t sample_count) const;

protected:
  static void _bind_methods();

private:
  godot::String region_id_;
  std::int64_t radius_mm_{1000};
  godot::PackedInt32Array cell_coordinates_;
  godot::PackedInt32Array surface_samples_mm_;
  godot::PackedByteArray standing_clearance_;
};

class DrossHexGridOverrides final : public godot::Resource {
  GDCLASS(DrossHexGridOverrides, godot::Resource)

public:
  void set_region_id(const godot::String& value) { region_id_ = value; }
  [[nodiscard]] godot::String get_region_id() const { return region_id_; }
  void set_radius_mm(std::int64_t value) { radius_mm_ = value; }
  [[nodiscard]] std::int64_t get_radius_mm() const { return radius_mm_; }
  void set_cell_coordinates(const godot::PackedInt32Array& value) { cell_coordinates_ = value; }
  [[nodiscard]] godot::PackedInt32Array get_cell_coordinates() const { return cell_coordinates_; }
  void set_traversability(const godot::PackedByteArray& value) { traversability_ = value; }
  [[nodiscard]] godot::PackedByteArray get_traversability() const { return traversability_; }
  [[nodiscard]] bool set_cell_override(std::int64_t q, std::int64_t r, std::int64_t layer,
                                       std::int64_t override_value);
  [[nodiscard]] std::int64_t get_override_count() const;
  [[nodiscard]] std::optional<GridOverrides> compile_core() const;

protected:
  static void _bind_methods();

private:
  godot::String region_id_;
  std::int64_t radius_mm_{1000};
  godot::PackedInt32Array cell_coordinates_;
  godot::PackedByteArray traversability_;
};

class DrossCompiledHexMap final : public godot::Resource {
  GDCLASS(DrossCompiledHexMap, godot::Resource)

public:
  [[nodiscard]] bool compile_from(const godot::Ref<DrossHexGridBake>& bake,
                                  const godot::Ref<DrossHexGridOverrides>& overrides,
                                  const godot::Ref<DrossHexBakeProfile>& profile);
  void set_from_core(CompiledGridBake value);
  [[nodiscard]] std::int64_t get_cell_count() const;
  [[nodiscard]] godot::PackedStringArray get_cell_keys() const;
  [[nodiscard]] godot::PackedByteArray get_traversability() const;
  [[nodiscard]] godot::PackedByteArray get_provenance() const;
  [[nodiscard]] const std::optional<CompiledGridBake>& core() const noexcept { return core_; }

protected:
  static void _bind_methods();

private:
  std::optional<CompiledGridBake> core_;
};

} // namespace dross::godot_adapter

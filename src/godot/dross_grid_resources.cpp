#include "dross_grid_resources.hpp"

#include <godot_cpp/core/class_db.hpp>

#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace dross::godot_adapter {
namespace {

std::optional<ContentId> parse_id(const godot::String& value) {
  const auto text = value.utf8();
  auto parsed =
      ContentId::parse(std::string_view{text.get_data(), static_cast<std::size_t>(text.length())});
  return parsed ? std::optional<ContentId>{std::move(*parsed)} : std::nullopt;
}

std::optional<GridIdentity> grid_identity(const godot::String& region_id,
                                          const std::int64_t radius_mm) {
  auto parsed = parse_id(region_id);
  if (!parsed || radius_mm <= 0) {
    return std::nullopt;
  }
  return GridIdentity{.region = RegionId{std::move(*parsed)},
                      .origin_x_mm = 0,
                      .origin_y_mm = 0,
                      .origin_z_mm = 0,
                      .radius_mm = radius_mm};
}

} // namespace

std::optional<HexBakeProfile> DrossHexBakeProfile::compile_core() const {
  if (quantization_mm_ <= 0 || maximum_height_variance_mm_ < 0 || required_sample_count_ < 4 ||
      required_sample_count_ > std::numeric_limits<std::uint32_t>::max() ||
      required_clearance_mm_ <= 0) {
    return std::nullopt;
  }
  return HexBakeProfile{.algorithm_version = 1,
                        .quantization_mm = quantization_mm_,
                        .maximum_height_variance_mm = maximum_height_variance_mm_,
                        .required_sample_count = static_cast<std::uint32_t>(required_sample_count_),
                        .terrain = ContentId::parse("dross:floor").value(),
                        .movement_cost = MovementCost{1}};
}

void DrossHexBakeProfile::_bind_methods() {
  godot::ClassDB::bind_method(godot::D_METHOD("set_quantization_mm", "value"),
                              &DrossHexBakeProfile::set_quantization_mm);
  godot::ClassDB::bind_method(godot::D_METHOD("get_quantization_mm"),
                              &DrossHexBakeProfile::get_quantization_mm);
  godot::ClassDB::bind_method(godot::D_METHOD("set_maximum_height_variance_mm", "value"),
                              &DrossHexBakeProfile::set_maximum_height_variance_mm);
  godot::ClassDB::bind_method(godot::D_METHOD("get_maximum_height_variance_mm"),
                              &DrossHexBakeProfile::get_maximum_height_variance_mm);
  godot::ClassDB::bind_method(godot::D_METHOD("set_required_sample_count", "value"),
                              &DrossHexBakeProfile::set_required_sample_count);
  godot::ClassDB::bind_method(godot::D_METHOD("get_required_sample_count"),
                              &DrossHexBakeProfile::get_required_sample_count);
  godot::ClassDB::bind_method(godot::D_METHOD("set_required_clearance_mm", "value"),
                              &DrossHexBakeProfile::set_required_clearance_mm);
  godot::ClassDB::bind_method(godot::D_METHOD("get_required_clearance_mm"),
                              &DrossHexBakeProfile::get_required_clearance_mm);
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "quantization_mm"), "set_quantization_mm",
               "get_quantization_mm");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "maximum_height_variance_mm"),
               "set_maximum_height_variance_mm", "get_maximum_height_variance_mm");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "required_sample_count"),
               "set_required_sample_count", "get_required_sample_count");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "required_clearance_mm"),
               "set_required_clearance_mm", "get_required_clearance_mm");
}

std::int64_t DrossHexGridBake::get_cell_count() const {
  return cell_coordinates_.size() % 3 == 0 ? cell_coordinates_.size() / 3 : 0;
}

std::optional<GridBake> DrossHexGridBake::compile_core(const std::uint32_t sample_count) const {
  auto identity = grid_identity(region_id_, radius_mm_);
  const auto cell_count = get_cell_count();
  if (!identity || sample_count < 4 || standing_clearance_.size() != cell_count ||
      surface_samples_mm_.size() != cell_count * static_cast<std::int64_t>(sample_count)) {
    return std::nullopt;
  }
  std::vector<CellBakeEvidence> cells;
  cells.reserve(static_cast<std::size_t>(cell_count));
  for (std::int64_t index = 0; index < cell_count; ++index) {
    std::vector<std::int64_t> samples;
    samples.reserve(sample_count);
    for (std::uint32_t sample = 0; sample < sample_count; ++sample) {
      const auto offset =
          index * static_cast<std::int64_t>(sample_count) + static_cast<std::int64_t>(sample);
      samples.push_back(surface_samples_mm_[offset]);
    }
    cells.push_back(CellBakeEvidence{
        .id = HexCellId{.region = identity->region,
                        .coord = {.q = cell_coordinates_[index * 3],
                                  .r = cell_coordinates_[index * 3 + 1]},
                        .layer = cell_coordinates_[index * 3 + 2]},
        .surface_samples_mm = std::move(samples),
        .standing_clearance = standing_clearance_[index] != 0,
        .source = ContentId::parse("godot:physics_geometry").value(),
    });
  }
  return GridBake{.identity = std::move(*identity),
                  .profile_version = 1,
                  .cells = std::move(cells),
                  .edges = {}};
}

void DrossHexGridBake::_bind_methods() {
  godot::ClassDB::bind_method(godot::D_METHOD("set_region_id", "value"),
                              &DrossHexGridBake::set_region_id);
  godot::ClassDB::bind_method(godot::D_METHOD("get_region_id"), &DrossHexGridBake::get_region_id);
  godot::ClassDB::bind_method(godot::D_METHOD("set_radius_mm", "value"),
                              &DrossHexGridBake::set_radius_mm);
  godot::ClassDB::bind_method(godot::D_METHOD("get_radius_mm"), &DrossHexGridBake::get_radius_mm);
  godot::ClassDB::bind_method(godot::D_METHOD("set_cell_coordinates", "value"),
                              &DrossHexGridBake::set_cell_coordinates);
  godot::ClassDB::bind_method(godot::D_METHOD("get_cell_coordinates"),
                              &DrossHexGridBake::get_cell_coordinates);
  godot::ClassDB::bind_method(godot::D_METHOD("set_surface_samples_mm", "value"),
                              &DrossHexGridBake::set_surface_samples_mm);
  godot::ClassDB::bind_method(godot::D_METHOD("get_surface_samples_mm"),
                              &DrossHexGridBake::get_surface_samples_mm);
  godot::ClassDB::bind_method(godot::D_METHOD("set_standing_clearance", "value"),
                              &DrossHexGridBake::set_standing_clearance);
  godot::ClassDB::bind_method(godot::D_METHOD("get_standing_clearance"),
                              &DrossHexGridBake::get_standing_clearance);
  godot::ClassDB::bind_method(godot::D_METHOD("get_cell_count"), &DrossHexGridBake::get_cell_count);
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::STRING, "region_id"), "set_region_id",
               "get_region_id");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "radius_mm"), "set_radius_mm",
               "get_radius_mm");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_INT32_ARRAY, "cell_coordinates"),
               "set_cell_coordinates", "get_cell_coordinates");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_INT32_ARRAY, "surface_samples_mm"),
               "set_surface_samples_mm", "get_surface_samples_mm");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_BYTE_ARRAY, "standing_clearance"),
               "set_standing_clearance", "get_standing_clearance");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "cell_count"), "", "get_cell_count");
}

bool DrossHexGridOverrides::set_cell_override(const std::int64_t q, const std::int64_t r,
                                              const std::int64_t layer,
                                              const std::int64_t override_value) {
  if (q < std::numeric_limits<std::int32_t>::min() ||
      q > std::numeric_limits<std::int32_t>::max() ||
      r < std::numeric_limits<std::int32_t>::min() ||
      r > std::numeric_limits<std::int32_t>::max() ||
      layer < std::numeric_limits<std::int32_t>::min() ||
      layer > std::numeric_limits<std::int32_t>::max() || override_value < 0 ||
      override_value > 2) {
    return false;
  }
  for (std::int64_t index = 0; index < get_override_count(); ++index) {
    if (cell_coordinates_[index * 3] == q && cell_coordinates_[index * 3 + 1] == r &&
        cell_coordinates_[index * 3 + 2] == layer) {
      traversability_.set(index, static_cast<std::uint8_t>(override_value));
      return true;
    }
  }
  cell_coordinates_.push_back(static_cast<std::int32_t>(q));
  cell_coordinates_.push_back(static_cast<std::int32_t>(r));
  cell_coordinates_.push_back(static_cast<std::int32_t>(layer));
  traversability_.push_back(static_cast<std::uint8_t>(override_value));
  return true;
}

std::int64_t DrossHexGridOverrides::get_override_count() const {
  return cell_coordinates_.size() % 3 == 0 && traversability_.size() == cell_coordinates_.size() / 3
             ? traversability_.size()
             : 0;
}

std::optional<GridOverrides> DrossHexGridOverrides::compile_core() const {
  auto identity = grid_identity(region_id_, radius_mm_);
  const auto count = get_override_count();
  if (!identity || (count == 0 && (!cell_coordinates_.is_empty() || !traversability_.is_empty()))) {
    return std::nullopt;
  }
  std::vector<std::pair<HexCellId, CellTraversabilityOverride>> cells;
  cells.reserve(static_cast<std::size_t>(count));
  for (std::int64_t index = 0; index < count; ++index) {
    if (traversability_[index] > 2) {
      return std::nullopt;
    }
    cells.emplace_back(HexCellId{.region = identity->region,
                                 .coord = {.q = cell_coordinates_[index * 3],
                                           .r = cell_coordinates_[index * 3 + 1]},
                                 .layer = cell_coordinates_[index * 3 + 2]},
                       static_cast<CellTraversabilityOverride>(traversability_[index]));
  }
  return GridOverrides{.identity = std::move(*identity), .cells = std::move(cells)};
}

void DrossHexGridOverrides::_bind_methods() {
  godot::ClassDB::bind_method(godot::D_METHOD("set_region_id", "value"),
                              &DrossHexGridOverrides::set_region_id);
  godot::ClassDB::bind_method(godot::D_METHOD("get_region_id"),
                              &DrossHexGridOverrides::get_region_id);
  godot::ClassDB::bind_method(godot::D_METHOD("set_radius_mm", "value"),
                              &DrossHexGridOverrides::set_radius_mm);
  godot::ClassDB::bind_method(godot::D_METHOD("get_radius_mm"),
                              &DrossHexGridOverrides::get_radius_mm);
  godot::ClassDB::bind_method(godot::D_METHOD("set_cell_coordinates", "value"),
                              &DrossHexGridOverrides::set_cell_coordinates);
  godot::ClassDB::bind_method(godot::D_METHOD("get_cell_coordinates"),
                              &DrossHexGridOverrides::get_cell_coordinates);
  godot::ClassDB::bind_method(godot::D_METHOD("set_traversability", "value"),
                              &DrossHexGridOverrides::set_traversability);
  godot::ClassDB::bind_method(godot::D_METHOD("get_traversability"),
                              &DrossHexGridOverrides::get_traversability);
  godot::ClassDB::bind_method(
      godot::D_METHOD("set_cell_override", "q", "r", "layer", "override_value"),
      &DrossHexGridOverrides::set_cell_override);
  godot::ClassDB::bind_method(godot::D_METHOD("get_override_count"),
                              &DrossHexGridOverrides::get_override_count);
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::STRING, "region_id"), "set_region_id",
               "get_region_id");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "radius_mm"), "set_radius_mm",
               "get_radius_mm");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_INT32_ARRAY, "cell_coordinates"),
               "set_cell_coordinates", "get_cell_coordinates");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_BYTE_ARRAY, "traversability"),
               "set_traversability", "get_traversability");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "override_count"), "",
               "get_override_count");
}

void DrossCompiledHexMap::set_from_core(CompiledGridBake value) { core_ = std::move(value); }

bool DrossCompiledHexMap::compile_from(const godot::Ref<DrossHexGridBake>& bake,
                                       const godot::Ref<DrossHexGridOverrides>& overrides,
                                       const godot::Ref<DrossHexBakeProfile>& profile) {
  if (bake.is_null() || overrides.is_null() || profile.is_null()) {
    return false;
  }
  const auto core_profile = profile->compile_core();
  if (!core_profile) {
    return false;
  }
  const auto core_bake = bake->compile_core(core_profile->required_sample_count);
  const auto core_overrides = overrides->compile_core();
  if (!core_bake || !core_overrides) {
    return false;
  }
  auto compiled = compile_grid_bake(*core_bake, *core_overrides, *core_profile);
  if (!compiled) {
    return false;
  }
  set_from_core(std::move(*compiled));
  return true;
}

std::int64_t DrossCompiledHexMap::get_cell_count() const {
  return core_ ? static_cast<std::int64_t>(core_->map.cell_ids().size()) : 0;
}

godot::PackedStringArray DrossCompiledHexMap::get_cell_keys() const {
  godot::PackedStringArray result;
  if (!core_) {
    return result;
  }
  for (const auto& cell : core_->map.cell_ids()) {
    const auto key = std::string{cell.region.content_id().canonical()} + ":" +
                     std::to_string(cell.coord.q) + "," + std::to_string(cell.coord.r) + "," +
                     std::to_string(cell.layer);
    result.push_back(godot::String{key.c_str()});
  }
  return result;
}

godot::PackedByteArray DrossCompiledHexMap::get_traversability() const {
  godot::PackedByteArray result;
  if (core_) {
    for (const auto& cell : core_->map.cell_ids()) {
      result.push_back(core_->map.cell(cell)->traversable ? 1U : 0U);
    }
  }
  return result;
}

godot::PackedByteArray DrossCompiledHexMap::get_provenance() const {
  godot::PackedByteArray result;
  if (core_) {
    for (const auto& cell : core_->map.cell_ids()) {
      result.push_back(static_cast<std::uint8_t>(core_->provenance.at(cell)));
    }
  }
  return result;
}

void DrossCompiledHexMap::_bind_methods() {
  godot::ClassDB::bind_method(godot::D_METHOD("compile_from", "bake", "overrides", "profile"),
                              &DrossCompiledHexMap::compile_from);
  godot::ClassDB::bind_method(godot::D_METHOD("get_cell_count"),
                              &DrossCompiledHexMap::get_cell_count);
  godot::ClassDB::bind_method(godot::D_METHOD("get_cell_keys"),
                              &DrossCompiledHexMap::get_cell_keys);
  godot::ClassDB::bind_method(godot::D_METHOD("get_traversability"),
                              &DrossCompiledHexMap::get_traversability);
  godot::ClassDB::bind_method(godot::D_METHOD("get_provenance"),
                              &DrossCompiledHexMap::get_provenance);
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_STRING_ARRAY, "cell_keys"), "",
               "get_cell_keys");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_BYTE_ARRAY, "traversability"), "",
               "get_traversability");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_BYTE_ARRAY, "provenance"), "",
               "get_provenance");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "cell_count"), "", "get_cell_count");
}

} // namespace dross::godot_adapter

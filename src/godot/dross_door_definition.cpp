#include "dross_door_definition.hpp"

#include <godot_cpp/core/class_db.hpp>

#include <limits>
#include <string_view>
#include <utility>

namespace dross::godot_adapter {
namespace {

std::optional<ContentId> parse_id(const godot::String& value) {
  const auto utf8 = value.utf8();
  auto parsed =
      ContentId::parse(std::string_view{utf8.get_data(), static_cast<std::size_t>(utf8.length())});
  return parsed ? std::optional<ContentId>{std::move(*parsed)} : std::nullopt;
}

bool coordinate_valid(const std::int64_t value) {
  return value >= std::numeric_limits<std::int32_t>::min() &&
         value <= std::numeric_limits<std::int32_t>::max();
}

} // namespace

std::optional<CompiledDoorDefinition> DrossDoorDefinition::compile_core() const {
  auto door_id = parse_id(door_id_);
  auto region_id = parse_id(region_id_);
  if (!door_id || !region_id || !coordinate_valid(from_q_) || !coordinate_valid(from_r_) ||
      !coordinate_valid(to_q_) || !coordinate_valid(to_r_)) {
    return std::nullopt;
  }
  const auto region = RegionId{std::move(*region_id)};
  auto edge = EdgeKey::between(
      {.region = region,
       .coord = {.q = static_cast<std::int32_t>(from_q_), .r = static_cast<std::int32_t>(from_r_)},
       .layer = 0},
      {.region = region,
       .coord = {.q = static_cast<std::int32_t>(to_q_), .r = static_cast<std::int32_t>(to_r_)},
       .layer = 0});
  if (!edge) {
    return std::nullopt;
  }
  auto footprint = EdgeFootprint::create({std::move(*edge)});
  if (!footprint) {
    return std::nullopt;
  }
  return CompiledDoorDefinition{.id = std::move(*door_id), .footprint = std::move(*footprint)};
}

bool DrossDoorDefinition::is_valid() const { return compile_core().has_value(); }

void DrossDoorDefinition::_bind_methods() {
  godot::ClassDB::bind_method(godot::D_METHOD("set_door_id", "value"),
                              &DrossDoorDefinition::set_door_id);
  godot::ClassDB::bind_method(godot::D_METHOD("get_door_id"), &DrossDoorDefinition::get_door_id);
  godot::ClassDB::bind_method(godot::D_METHOD("set_region_id", "value"),
                              &DrossDoorDefinition::set_region_id);
  godot::ClassDB::bind_method(godot::D_METHOD("get_region_id"),
                              &DrossDoorDefinition::get_region_id);
  godot::ClassDB::bind_method(godot::D_METHOD("set_from_q", "value"),
                              &DrossDoorDefinition::set_from_q);
  godot::ClassDB::bind_method(godot::D_METHOD("get_from_q"), &DrossDoorDefinition::get_from_q);
  godot::ClassDB::bind_method(godot::D_METHOD("set_from_r", "value"),
                              &DrossDoorDefinition::set_from_r);
  godot::ClassDB::bind_method(godot::D_METHOD("get_from_r"), &DrossDoorDefinition::get_from_r);
  godot::ClassDB::bind_method(godot::D_METHOD("set_to_q", "value"), &DrossDoorDefinition::set_to_q);
  godot::ClassDB::bind_method(godot::D_METHOD("get_to_q"), &DrossDoorDefinition::get_to_q);
  godot::ClassDB::bind_method(godot::D_METHOD("set_to_r", "value"), &DrossDoorDefinition::set_to_r);
  godot::ClassDB::bind_method(godot::D_METHOD("get_to_r"), &DrossDoorDefinition::get_to_r);
  godot::ClassDB::bind_method(godot::D_METHOD("is_valid"), &DrossDoorDefinition::is_valid);
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::STRING, "door_id"), "set_door_id",
               "get_door_id");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::STRING, "region_id"), "set_region_id",
               "get_region_id");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "from_q"), "set_from_q", "get_from_q");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "from_r"), "set_from_r", "get_from_r");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "to_q"), "set_to_q", "get_to_q");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "to_r"), "set_to_r", "get_to_r");
}

} // namespace dross::godot_adapter

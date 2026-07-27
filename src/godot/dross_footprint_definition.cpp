#include "dross_footprint_definition.hpp"

#include <godot_cpp/core/class_db.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace dross::godot_adapter {

void DrossFootprintDefinition::set_content_id(const godot::String& value) { content_id_ = value; }

godot::String DrossFootprintDefinition::get_content_id() const { return content_id_; }

void DrossFootprintDefinition::set_offsets(const godot::PackedVector2Array& value) {
  offsets_ = value;
}

godot::PackedVector2Array DrossFootprintDefinition::get_offsets() const { return offsets_; }

Result<FootprintDefinition, FootprintCompileFailure>
DrossFootprintDefinition::compile_core() const {
  const auto utf8 = content_id_.utf8();
  auto parsed_id =
      ContentId::parse(std::string_view{utf8.get_data(), static_cast<std::size_t>(utf8.length())});
  if (!parsed_id) {
    return tl::unexpected{FootprintCompileFailure{
        .property_name = "content_id",
        .message = godot::String{"invalid ContentId at byte "} +
                   godot::String::num_int64(static_cast<std::int64_t>(parsed_id.error().position)),
    }};
  }

  std::vector<HexCoord> offsets;
  offsets.reserve(static_cast<std::size_t>(offsets_.size()));
  for (std::int64_t index = 0; index < offsets_.size(); ++index) {
    const auto offset = offsets_[index];
    const auto x = static_cast<double>(offset.x);
    const auto y = static_cast<double>(offset.y);
    if (x != std::trunc(x) || y != std::trunc(y) ||
        x < static_cast<double>(std::numeric_limits<std::int32_t>::min()) ||
        x > static_cast<double>(std::numeric_limits<std::int32_t>::max()) ||
        y < static_cast<double>(std::numeric_limits<std::int32_t>::min()) ||
        y > static_cast<double>(std::numeric_limits<std::int32_t>::max())) {
      return tl::unexpected{FootprintCompileFailure{
          .property_name = "offsets",
          .message = "offsets must contain signed 32-bit integer coordinates",
      }};
    }
    offsets.push_back(HexCoord{.q = static_cast<std::int32_t>(offset.x),
                               .r = static_cast<std::int32_t>(offset.y)});
  }

  auto footprint =
      FootprintDefinition::create(FootprintId{std::move(*parsed_id)}, std::move(offsets));
  if (!footprint) {
    const auto message = footprint.error() == FootprintError::missing_origin
                             ? godot::String{"offsets must contain the origin (0, 0)"}
                             : godot::String{"offsets must not contain duplicates"};
    return tl::unexpected{FootprintCompileFailure{.property_name = "offsets", .message = message}};
  }
  return std::move(*footprint);
}

godot::Ref<DrossValidationError> DrossFootprintDefinition::validate() const {
  const auto compiled = compile_core();
  if (compiled) {
    return {};
  }
  return DrossValidationError::create(validation_path(), compiled.error().property_name,
                                      compiled.error().message);
}

godot::String DrossFootprintDefinition::compile_summary() const {
  const auto compiled = compile_core();
  if (!compiled) {
    return {};
  }
  return godot::String{compiled->id().content_id().canonical().data()} + ":" +
         godot::String::num_int64(static_cast<std::int64_t>(compiled->offsets().size()));
}

godot::String DrossFootprintDefinition::validation_path() const {
  const auto path = get_path();
  return path.is_empty() ? godot::String{"<memory>"} : path;
}

void DrossFootprintDefinition::_bind_methods() {
  godot::ClassDB::bind_method(godot::D_METHOD("set_content_id", "value"),
                              &DrossFootprintDefinition::set_content_id);
  godot::ClassDB::bind_method(godot::D_METHOD("get_content_id"),
                              &DrossFootprintDefinition::get_content_id);
  godot::ClassDB::bind_method(godot::D_METHOD("set_offsets", "value"),
                              &DrossFootprintDefinition::set_offsets);
  godot::ClassDB::bind_method(godot::D_METHOD("get_offsets"),
                              &DrossFootprintDefinition::get_offsets);
  godot::ClassDB::bind_method(godot::D_METHOD("validate"), &DrossFootprintDefinition::validate);
  godot::ClassDB::bind_method(godot::D_METHOD("compile_summary"),
                              &DrossFootprintDefinition::compile_summary);

  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::STRING, "content_id"), "set_content_id",
               "get_content_id");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_VECTOR2_ARRAY, "offsets"), "set_offsets",
               "get_offsets");
}

} // namespace dross::godot_adapter

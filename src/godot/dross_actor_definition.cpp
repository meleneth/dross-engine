#include "dross_actor_definition.hpp"

#include <godot_cpp/core/class_db.hpp>

#include <cstdint>
#include <string>
#include <utility>

namespace dross::godot_adapter {

void DrossActorDefinition::set_content_id(const godot::String& value) { content_id_ = value; }

godot::String DrossActorDefinition::get_content_id() const { return content_id_; }

void DrossActorDefinition::set_footprint(const godot::Ref<DrossFootprintDefinition>& value) {
  footprint_ = value;
}

godot::Ref<DrossFootprintDefinition> DrossActorDefinition::get_footprint() const {
  return footprint_;
}

Result<CompiledActorDefinition, ActorCompileFailure> DrossActorDefinition::compile_core() const {
  const auto utf8 = content_id_.utf8();
  auto parsed_id =
      ContentId::parse(std::string_view{utf8.get_data(), static_cast<std::size_t>(utf8.length())});
  if (!parsed_id) {
    return tl::unexpected{ActorCompileFailure{
        .property_name = "content_id",
        .message = godot::String{"invalid ContentId at byte "} +
                   godot::String::num_int64(static_cast<std::int64_t>(parsed_id.error().position)),
        .resource_path = validation_path(),
    }};
  }
  if (footprint_.is_null()) {
    return tl::unexpected{ActorCompileFailure{
        .property_name = "footprint",
        .message = "footprint is required",
        .resource_path = validation_path(),
    }};
  }
  auto compiled_footprint = footprint_->compile_core();
  if (!compiled_footprint) {
    const auto footprint_path = footprint_->get_path();
    return tl::unexpected{ActorCompileFailure{
        .property_name = compiled_footprint.error().property_name,
        .message = compiled_footprint.error().message,
        .resource_path = footprint_path.is_empty() ? godot::String{"<memory>"} : footprint_path,
    }};
  }
  return CompiledActorDefinition{
      .id = std::move(*parsed_id),
      .footprint = std::move(*compiled_footprint),
  };
}

godot::Ref<DrossValidationError> DrossActorDefinition::validate() const {
  const auto compiled = compile_core();
  if (compiled) {
    return {};
  }
  return DrossValidationError::create(compiled.error().resource_path,
                                      compiled.error().property_name, compiled.error().message);
}

godot::String DrossActorDefinition::compile_summary() const {
  const auto compiled = compile_core();
  if (!compiled) {
    return {};
  }
  return godot::String{compiled->id.canonical().data()} + ":" +
         godot::String::num_int64(static_cast<std::int64_t>(compiled->footprint.offsets().size()));
}

godot::String DrossActorDefinition::validation_path() const {
  const auto path = get_path();
  return path.is_empty() ? godot::String{"<memory>"} : path;
}

void DrossActorDefinition::_bind_methods() {
  godot::ClassDB::bind_method(godot::D_METHOD("set_content_id", "value"),
                              &DrossActorDefinition::set_content_id);
  godot::ClassDB::bind_method(godot::D_METHOD("get_content_id"),
                              &DrossActorDefinition::get_content_id);
  godot::ClassDB::bind_method(godot::D_METHOD("set_footprint", "value"),
                              &DrossActorDefinition::set_footprint);
  godot::ClassDB::bind_method(godot::D_METHOD("get_footprint"),
                              &DrossActorDefinition::get_footprint);
  godot::ClassDB::bind_method(godot::D_METHOD("validate"), &DrossActorDefinition::validate);
  godot::ClassDB::bind_method(godot::D_METHOD("compile_summary"),
                              &DrossActorDefinition::compile_summary);

  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::STRING, "content_id"), "set_content_id",
               "get_content_id");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::OBJECT, "footprint",
                                   godot::PROPERTY_HINT_RESOURCE_TYPE, "DrossFootprintDefinition"),
               "set_footprint", "get_footprint");
}

} // namespace dross::godot_adapter

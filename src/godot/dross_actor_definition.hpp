#pragma once

#include "compiled_actor_definition.hpp"
#include "dross_footprint_definition.hpp"

#include <dross/foundation/result.hpp>

#include <godot_cpp/classes/resource.hpp>

namespace dross::godot_adapter {

struct ActorCompileFailure {
  godot::String property_name;
  godot::String message;
  godot::String resource_path;
};

class DrossActorDefinition final : public godot::Resource {
  GDCLASS(DrossActorDefinition, godot::Resource)

public:
  void set_content_id(const godot::String& value);
  [[nodiscard]] godot::String get_content_id() const;
  void set_footprint(const godot::Ref<DrossFootprintDefinition>& value);
  [[nodiscard]] godot::Ref<DrossFootprintDefinition> get_footprint() const;

  [[nodiscard]] godot::Ref<DrossValidationError> validate() const;
  [[nodiscard]] godot::String compile_summary() const;
  [[nodiscard]] Result<CompiledActorDefinition, ActorCompileFailure> compile_core() const;

protected:
  static void _bind_methods();

private:
  [[nodiscard]] godot::String validation_path() const;

  godot::String content_id_;
  godot::Ref<DrossFootprintDefinition> footprint_;
};

} // namespace dross::godot_adapter

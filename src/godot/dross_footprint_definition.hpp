#pragma once

#include "dross_validation_error.hpp"

#include <dross/foundation/result.hpp>
#include <dross/hex/footprint.hpp>

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/string.hpp>

namespace dross::godot_adapter {

struct FootprintCompileFailure {
  godot::String property_name;
  godot::String message;
};

class DrossFootprintDefinition final : public godot::Resource {
  GDCLASS(DrossFootprintDefinition, godot::Resource)

public:
  void set_content_id(const godot::String& value);
  [[nodiscard]] godot::String get_content_id() const;
  void set_offsets(const godot::PackedVector2Array& value);
  [[nodiscard]] godot::PackedVector2Array get_offsets() const;

  [[nodiscard]] godot::Ref<DrossValidationError> validate() const;
  [[nodiscard]] godot::String compile_summary() const;
  [[nodiscard]] Result<FootprintDefinition, FootprintCompileFailure> compile_core() const;

protected:
  static void _bind_methods();

private:
  [[nodiscard]] godot::String validation_path() const;

  godot::String content_id_;
  godot::PackedVector2Array offsets_;
};

} // namespace dross::godot_adapter

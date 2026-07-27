#pragma once

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/string.hpp>

namespace dross::godot_adapter {

class DrossValidationError final : public godot::RefCounted {
  GDCLASS(DrossValidationError, godot::RefCounted)

public:
  void set_resource_path(const godot::String& value);
  [[nodiscard]] godot::String get_resource_path() const;
  void set_property_name(const godot::String& value);
  [[nodiscard]] godot::String get_property_name() const;
  void set_message(const godot::String& value);
  [[nodiscard]] godot::String get_message() const;

  static godot::Ref<DrossValidationError> create(const godot::String& resource_path,
                                                 const godot::String& property_name,
                                                 const godot::String& message);

protected:
  static void _bind_methods();

private:
  godot::String resource_path_;
  godot::String property_name_;
  godot::String message_;
};

} // namespace dross::godot_adapter

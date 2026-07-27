#include "dross_validation_error.hpp"

#include <godot_cpp/core/class_db.hpp>

namespace dross::godot_adapter {

void DrossValidationError::set_resource_path(const godot::String& value) { resource_path_ = value; }

godot::String DrossValidationError::get_resource_path() const { return resource_path_; }

void DrossValidationError::set_property_name(const godot::String& value) { property_name_ = value; }

godot::String DrossValidationError::get_property_name() const { return property_name_; }

void DrossValidationError::set_message(const godot::String& value) { message_ = value; }

godot::String DrossValidationError::get_message() const { return message_; }

godot::Ref<DrossValidationError> DrossValidationError::create(const godot::String& resource_path,
                                                              const godot::String& property_name,
                                                              const godot::String& message) {
  godot::Ref<DrossValidationError> error;
  error.instantiate();
  error->resource_path_ = resource_path;
  error->property_name_ = property_name;
  error->message_ = message;
  return error;
}

void DrossValidationError::_bind_methods() {
  godot::ClassDB::bind_method(godot::D_METHOD("set_resource_path", "value"),
                              &DrossValidationError::set_resource_path);
  godot::ClassDB::bind_method(godot::D_METHOD("get_resource_path"),
                              &DrossValidationError::get_resource_path);
  godot::ClassDB::bind_method(godot::D_METHOD("set_property_name", "value"),
                              &DrossValidationError::set_property_name);
  godot::ClassDB::bind_method(godot::D_METHOD("get_property_name"),
                              &DrossValidationError::get_property_name);
  godot::ClassDB::bind_method(godot::D_METHOD("set_message", "value"),
                              &DrossValidationError::set_message);
  godot::ClassDB::bind_method(godot::D_METHOD("get_message"), &DrossValidationError::get_message);

  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::STRING, "resource_path"), "set_resource_path",
               "get_resource_path");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::STRING, "property_name"), "set_property_name",
               "get_property_name");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::STRING, "message"), "set_message",
               "get_message");
}

} // namespace dross::godot_adapter

#include "dross_actor_definition.hpp"
#include "dross_footprint_definition.hpp"
#include "dross_validation_error.hpp"
#include "dross_world_host.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

namespace {

void initialize_dross_module(godot::ModuleInitializationLevel level) {
  if (level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
    return;
  }
  godot::ClassDB::register_class<dross::godot_adapter::DrossValidationError>();
  godot::ClassDB::register_class<dross::godot_adapter::DrossFootprintDefinition>();
  godot::ClassDB::register_class<dross::godot_adapter::DrossActorDefinition>();
  godot::ClassDB::register_class<dross::godot_adapter::DrossWorldHost>();
}

void uninitialize_dross_module(godot::ModuleInitializationLevel level) {
  if (level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
    return;
  }
}

} // namespace

extern "C" {

GDExtensionBool GDE_EXPORT dross_library_init(GDExtensionInterfaceGetProcAddress get_proc_address,
                                              GDExtensionClassLibraryPtr library,
                                              GDExtensionInitialization* initialization) {
  godot::GDExtensionBinding::InitObject init_object{get_proc_address, library, initialization};
  init_object.register_initializer(initialize_dross_module);
  init_object.register_terminator(uninitialize_dross_module);
  init_object.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
  return init_object.init();
}
}

#include "dross_ability_definition.hpp"
#include "dross_actor_definition.hpp"
#include "dross_entity_view.hpp"
#include "dross_footprint_definition.hpp"
#include "dross_grid_resources.hpp"
#include "dross_hex_grid_overlay3d.hpp"
#include "dross_hex_grid_region3d.hpp"
#include "dross_script_runtime.hpp"
#include "dross_validation_error.hpp"
#include "dross_world_host.hpp"

#include <dross/generated/godot_api.hpp>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

namespace {

void initialize_dross_module(godot::ModuleInitializationLevel level) {
  if (level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
    return;
  }
  godot::ClassDB::register_class<dross::godot_adapter::DrossValidationError>();
  godot::ClassDB::register_class<dross::godot_adapter::DrossAbilityDefinition>();
  godot::ClassDB::register_class<dross::godot_adapter::DrossFootprintDefinition>();
  godot::ClassDB::register_class<dross::godot_adapter::DrossActorDefinition>();
  godot::ClassDB::register_class<dross::godot_adapter::DrossMovementPreview>();
  godot::ClassDB::register_class<dross::godot_adapter::DrossEntityView>();
  godot::ClassDB::register_class<dross::godot_adapter::DrossViewRegistry>();
  godot::ClassDB::register_class<dross::godot_adapter::DrossHexBakeProfile>();
  godot::ClassDB::register_class<dross::godot_adapter::DrossHexGridBake>();
  godot::ClassDB::register_class<dross::godot_adapter::DrossHexGridOverrides>();
  godot::ClassDB::register_class<dross::godot_adapter::DrossCompiledHexMap>();
  godot::ClassDB::register_class<dross::godot_adapter::DrossHexGridRegion3D>();
  godot::ClassDB::register_class<dross::godot_adapter::DrossHexGridOverlay3D>();
  godot::ClassDB::register_class<dross::godot_adapter::DrossScriptModuleDefinition>();
  godot::ClassDB::register_class<dross::godot_adapter::DrossScriptStateApi>();
  godot::ClassDB::register_class<dross::godot_adapter::DrossRandomApi>();
  godot::ClassDB::register_class<dross::godot_adapter::DrossCommandApi>();
  godot::ClassDB::register_class<dross::godot_adapter::DrossQueryApi>();
  godot::ClassDB::register_class<dross::godot_adapter::DrossScriptContext>();
  godot::ClassDB::register_class<dross::godot_adapter::DrossCallbackLogger>();
  godot::ClassDB::register_class<dross::godot_adapter::DrossWorldHost>();
  dross::generated::godot_api::register_generated_godot_types();
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

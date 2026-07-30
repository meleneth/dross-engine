#pragma once

#include "compiled_actor_definition.hpp"
#include "dross_ability_definition.hpp"
#include "dross_actor_definition.hpp"
#include "dross_door_definition.hpp"
#include "dross_entity_view.hpp"
#include "dross_grid_resources.hpp"
#include "dross_script_runtime.hpp"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/typed_array.hpp>

#include <cstdint>
#include <memory>

namespace dross::godot_adapter {

class DrossWorldHost final : public godot::Node {
  GDCLASS(DrossWorldHost, godot::Node)

public:
  DrossWorldHost();
  ~DrossWorldHost() override;

  [[nodiscard]] godot::Ref<DrossValidationError>
  start_synthetic_world(const godot::Ref<DrossActorDefinition>& actor);
  void stop_world();
  [[nodiscard]] bool is_running() const;
  [[nodiscard]] bool advance_test_tick();
  [[nodiscard]] std::int64_t get_tick() const;
  [[nodiscard]] std::int64_t get_entity_count() const;
  [[nodiscard]] godot::String get_actor_id() const;
  [[nodiscard]] std::int64_t get_footprint_cell_count() const;
  [[nodiscard]] bool
  start_script_scenario(const godot::TypedArray<DrossScriptModuleDefinition>& modules,
                        std::int64_t seed);
  [[nodiscard]] bool run_script_scenario();
  [[nodiscard]] godot::String get_script_call_order() const;
  [[nodiscard]] godot::String get_script_mode() const;
  [[nodiscard]] bool is_script_world_faulted() const;
  [[nodiscard]] bool get_script_state_bool(const godot::String& module_id,
                                           std::int64_t entity_sequence,
                                           const godot::String& key) const;
  [[nodiscard]] std::int64_t get_script_state_int(const godot::String& module_id,
                                                  std::int64_t entity_sequence,
                                                  const godot::String& key) const;
  [[nodiscard]] godot::PackedByteArray save_script_state() const;
  [[nodiscard]] bool restore_script_state(const godot::PackedByteArray& bytes);
  [[nodiscard]] bool start_movement_scenario();
  [[nodiscard]] godot::Ref<DrossCompiledHexMap> get_movement_compiled_map() const;
  [[nodiscard]] godot::Ref<DrossMovementPreview> preview_movement(std::int64_t destination_q) const;
  [[nodiscard]] bool move_to(std::int64_t destination_q);
  [[nodiscard]] bool cancel_movement();
  [[nodiscard]] bool request_movement_combat();
  [[nodiscard]] bool advance_movement_tick();
  [[nodiscard]] std::int64_t get_movement_tick() const;
  [[nodiscard]] std::int64_t get_movement_column() const;
  [[nodiscard]] std::int64_t get_movement_presentation_to_column() const;
  [[nodiscard]] double get_movement_presentation_alpha() const;
  [[nodiscard]] godot::String get_movement_state() const;
  [[nodiscard]] godot::String get_movement_mode() const;
  [[nodiscard]] godot::PackedStringArray get_recent_movement_events() const;
  [[nodiscard]] godot::String get_canonical_capability_hash() const;
  [[nodiscard]] godot::PackedByteArray save_integrated_state() const;
  [[nodiscard]] bool restore_integrated_state(const godot::PackedByteArray& bytes);
  [[nodiscard]] godot::String get_last_load_error() const { return last_load_error_; }
  [[nodiscard]] bool
  start_thump_scenario(const godot::Ref<DrossAbilityDefinition>& ability_definition);
  [[nodiscard]] bool perform_thump();
  [[nodiscard]] std::int64_t get_player_action_points() const;
  [[nodiscard]] std::int64_t get_mouse_health() const;
  [[nodiscard]] bool is_mouse_killed() const;
  [[nodiscard]] godot::String get_last_presentation_cue() const;
  [[nodiscard]] bool start_door_scenario(const godot::Ref<DrossDoorDefinition>& definition);
  [[nodiscard]] bool open_door();
  [[nodiscard]] bool close_door();
  [[nodiscard]] bool is_door_open() const;
  [[nodiscard]] bool is_door_edge_traversable() const;
  [[nodiscard]] bool is_door_presentation_pending() const;
  [[nodiscard]] godot::String get_last_door_event() const;
  [[nodiscard]] std::int64_t get_door_presentation_acknowledgement_id() const;
  [[nodiscard]] bool acknowledge_door_presentation(std::int64_t acknowledgement_id);
  [[nodiscard]] bool advance_door_presentation();

protected:
  static void _bind_methods();

private:
  struct RuntimeState;
  struct ScriptScenarioState;
  struct MovementScenarioState;
  struct CombatScenarioState;
  struct DoorScenarioState;
  std::unique_ptr<RuntimeState> state_;
  std::unique_ptr<ScriptScenarioState> script_state_;
  std::unique_ptr<MovementScenarioState> movement_state_;
  std::unique_ptr<CombatScenarioState> combat_state_;
  std::unique_ptr<DoorScenarioState> door_state_;
  godot::String last_load_error_;
};

} // namespace dross::godot_adapter

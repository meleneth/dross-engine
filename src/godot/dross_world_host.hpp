#pragma once

#include "compiled_actor_definition.hpp"
#include "dross_actor_definition.hpp"

#include <godot_cpp/classes/node.hpp>

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

protected:
  static void _bind_methods();

private:
  struct RuntimeState;
  std::unique_ptr<RuntimeState> state_;
};

} // namespace dross::godot_adapter

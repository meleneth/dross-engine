#pragma once

#include <dross/runtime/combat_runtime.hpp>

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/string.hpp>

#include <cstdint>
#include <optional>

namespace dross::godot_adapter {

class DrossAbilityDefinition final : public godot::Resource {
  GDCLASS(DrossAbilityDefinition, godot::Resource)

public:
  void set_ability_id(const godot::String& value);
  [[nodiscard]] godot::String get_ability_id() const;
  void set_range(std::int64_t value);
  [[nodiscard]] std::int64_t get_range() const noexcept;
  void set_action_point_cost(std::int64_t value);
  [[nodiscard]] std::int64_t get_action_point_cost() const noexcept;
  void set_damage(std::int64_t value);
  [[nodiscard]] std::int64_t get_damage() const noexcept;
  void set_presentation_cue(const godot::String& value);
  [[nodiscard]] godot::String get_presentation_cue() const;
  [[nodiscard]] bool is_valid() const;
  [[nodiscard]] std::optional<AbilityDefinition> compile_core() const;

protected:
  static void _bind_methods();

private:
  godot::String ability_id_;
  godot::String presentation_cue_;
  std::int64_t range_{1};
  std::int64_t action_point_cost_{1};
  std::int64_t damage_{1};
};

} // namespace dross::godot_adapter

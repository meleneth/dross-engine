#pragma once

#include <dross/identity/content_id.hpp>
#include <dross/runtime/door_runtime.hpp>

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/string.hpp>

#include <cstdint>
#include <optional>

namespace dross::godot_adapter {

struct CompiledDoorDefinition {
  ContentId id;
  EdgeFootprint footprint;
};

class DrossDoorDefinition final : public godot::Resource {
  GDCLASS(DrossDoorDefinition, godot::Resource)

public:
  void set_door_id(const godot::String& value) { door_id_ = value; }
  [[nodiscard]] godot::String get_door_id() const { return door_id_; }
  void set_region_id(const godot::String& value) { region_id_ = value; }
  [[nodiscard]] godot::String get_region_id() const { return region_id_; }
  void set_from_q(std::int64_t value) { from_q_ = value; }
  [[nodiscard]] std::int64_t get_from_q() const noexcept { return from_q_; }
  void set_from_r(std::int64_t value) { from_r_ = value; }
  [[nodiscard]] std::int64_t get_from_r() const noexcept { return from_r_; }
  void set_to_q(std::int64_t value) { to_q_ = value; }
  [[nodiscard]] std::int64_t get_to_q() const noexcept { return to_q_; }
  void set_to_r(std::int64_t value) { to_r_ = value; }
  [[nodiscard]] std::int64_t get_to_r() const noexcept { return to_r_; }
  [[nodiscard]] bool is_valid() const;
  [[nodiscard]] std::optional<CompiledDoorDefinition> compile_core() const;

protected:
  static void _bind_methods();

private:
  godot::String door_id_;
  godot::String region_id_;
  std::int64_t from_q_{0};
  std::int64_t from_r_{0};
  std::int64_t to_q_{1};
  std::int64_t to_r_{0};
};

} // namespace dross::godot_adapter

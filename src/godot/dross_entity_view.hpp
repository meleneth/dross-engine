#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <cstdint>
#include <map>

namespace dross::godot_adapter {

class DrossEntityView final : public godot::Node3D {
  GDCLASS(DrossEntityView, godot::Node3D)

public:
  void set_entity_sequence(std::int64_t value);
  [[nodiscard]] std::int64_t get_entity_sequence() const noexcept;
  void apply_presentation_snapshot(const godot::Vector3& from, const godot::Vector3& to,
                                   double alpha);

protected:
  static void _bind_methods();

private:
  std::int64_t entity_sequence_{0};
};

class DrossViewRegistry final : public godot::Node {
  GDCLASS(DrossViewRegistry, godot::Node)

public:
  [[nodiscard]] bool register_view(DrossEntityView* view);
  [[nodiscard]] bool unregister_view(std::int64_t entity_sequence);
  [[nodiscard]] DrossEntityView* find_view(std::int64_t entity_sequence) const;
  [[nodiscard]] std::int64_t get_view_count() const noexcept;

protected:
  static void _bind_methods();

private:
  std::map<std::int64_t, std::uint64_t> views_;
};

} // namespace dross::godot_adapter

#pragma once

#include <dross/identity/entity_ref.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace dross {

enum class CombatSessionState : std::uint8_t {
  inactive,
  active,
  completed,
};

struct CombatantDefinition {
  EntityRef entity;
  std::int32_t initiative;
  std::uint32_t maximum_action_points;
};

class CombatSession {
public:
  explicit CombatSession(std::vector<CombatantDefinition> combatants);
  ~CombatSession();
  CombatSession(CombatSession&&) noexcept;
  CombatSession& operator=(CombatSession&&) noexcept;
  CombatSession(const CombatSession&) = delete;
  CombatSession& operator=(const CombatSession&) = delete;

  [[nodiscard]] bool start();
  [[nodiscard]] bool end_turn(EntityId actor);
  [[nodiscard]] bool spend_action_points(EntityId actor, std::uint32_t amount);
  [[nodiscard]] bool set_alive(EntityId actor, bool alive);
  [[nodiscard]] std::uint32_t action_points(EntityId actor) const;
  [[nodiscard]] EntityId active_actor() const;
  [[nodiscard]] std::vector<EntityId> turn_order() const;
  [[nodiscard]] CombatSessionState state() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace dross

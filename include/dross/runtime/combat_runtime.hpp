#pragma once

#include <dross/foundation/quantities.hpp>
#include <dross/hex/hex_topology.hpp>
#include <dross/identity/content_id.hpp>
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

struct AbilityDefinition {
  ContentId id;
  std::uint32_t range;
  std::uint32_t action_point_cost;
  HitPoints damage;
  ContentId presentation_cue;
};

struct AbilityActorState {
  EntityRef entity;
  HexPose pose;
  HitPoints health;
};

enum class AbilityRejection : std::uint8_t {
  none,
  invalid_ability,
  unknown_actor,
  inactive_actor,
  invalid_target,
  target_dead,
  out_of_range,
  insufficient_action_points,
};

struct AbilityResult {
  bool accepted;
  AbilityRejection rejection;
  HitPoints damage;
  HitPoints remaining_health;
  bool killed;

  [[nodiscard]] auto operator<=>(const AbilityResult&) const = default;
};

class AbilityResolver {
public:
  AbilityResolver(CombatSession& session, std::vector<AbilityActorState> actors);

  [[nodiscard]] AbilityResult perform(const AbilityDefinition& ability, EntityId actor,
                                      EntityId target);
  [[nodiscard]] HitPoints health(EntityId actor) const;

private:
  CombatSession* session_;
  std::vector<AbilityActorState> actors_;
};

} // namespace dross

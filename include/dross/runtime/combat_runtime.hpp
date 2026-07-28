#pragma once

#include <dross/foundation/byte_codec.hpp>
#include <dross/foundation/quantities.hpp>
#include <dross/generated/ability_committed.hpp>
#include <dross/generated/action_points_spent.hpp>
#include <dross/generated/actor_killed.hpp>
#include <dross/generated/combat_started.hpp>
#include <dross/generated/damage_applied.hpp>
#include <dross/generated/turn_started.hpp>
#include <dross/hex/hex_topology.hpp>
#include <dross/identity/content_id.hpp>
#include <dross/identity/entity_ref.hpp>
#include <dross/random/random_hub.hpp>
#include <dross/runtime/movement_runtime.hpp>

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

struct CombatantSnapshot {
  EntityId entity;
  std::int32_t initiative;
  std::uint32_t maximum_action_points;
  std::uint32_t action_points;
  bool alive;

  [[nodiscard]] auto operator<=>(const CombatantSnapshot&) const = default;
};

struct CombatSessionSnapshot {
  CombatSessionState state;
  std::size_t active_index;
  std::vector<CombatantSnapshot> combatants;

  [[nodiscard]] auto operator<=>(const CombatSessionSnapshot&) const = default;
};

void encode_combat_session_snapshot(ByteWriter& writer, const CombatSessionSnapshot& snapshot);
[[nodiscard]] Result<CombatSessionSnapshot, DecodeError>
decode_combat_session_snapshot(ByteReader& reader);

enum class CombatRestoreError : std::uint8_t {
  invalid_snapshot,
};

class CombatSession final : public MovementCostAccount {
public:
  class EventSink {
  public:
    virtual ~EventSink() = default;
    virtual void publish(const combat::CombatStarted& event) = 0;
    virtual void publish(const combat::TurnStarted& event) = 0;
    virtual void publish(const combat::ActionPointsSpent& event) = 0;
  };

  explicit CombatSession(std::vector<CombatantDefinition> combatants, EventSink* events = nullptr);
  [[nodiscard]] static Result<std::unique_ptr<CombatSession>, CombatRestoreError>
  from_snapshot(const CombatSessionSnapshot& snapshot, WorldInstanceId world_instance,
                EventSink* events = nullptr);
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
  [[nodiscard]] CombatSessionSnapshot snapshot() const;
  [[nodiscard]] bool restore(const CombatSessionSnapshot& snapshot);
  [[nodiscard]] bool can_spend(EntityId actor, MovementCost cost) const override;
  [[nodiscard]] bool spend(EntityId actor, MovementCost cost) override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

struct AbilityDefinition {
  ContentId id;
  std::uint32_t range;
  std::uint32_t action_point_cost;
  HitPoints damage;
  std::uint32_t bonus_damage_max{0};
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
  rule_rejected,
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
  class RuleSource {
  public:
    virtual ~RuleSource() = default;
    [[nodiscard]] virtual bool allows(const AbilityDefinition& ability, const EntityRef& actor,
                                      const EntityRef& target) = 0;
  };

  class EventSink {
  public:
    virtual ~EventSink() = default;
    virtual void publish(const combat::AbilityCommitted& event) = 0;
    virtual void publish(const combat::DamageApplied& event) = 0;
    virtual void publish(const combat::ActorKilled& event) = 0;
  };

  AbilityResolver(CombatSession& session, std::vector<AbilityActorState> actors,
                  EventSink* events = nullptr, RandomStream* random = nullptr,
                  RuleSource* rules = nullptr);

  [[nodiscard]] AbilityResult perform(const AbilityDefinition& ability, EntityId actor,
                                      EntityId target);
  [[nodiscard]] HitPoints health(EntityId actor) const;

private:
  CombatSession* session_;
  std::vector<AbilityActorState> actors_;
  EventSink* events_;
  RandomStream* random_;
  RuleSource* rules_;
};

} // namespace dross

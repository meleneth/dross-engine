#pragma once

#include <dross/foundation/byte_codec.hpp>
#include <dross/foundation/quantities.hpp>
#include <dross/generated/actor_killed.hpp>
#include <dross/generated/damage_applied.hpp>
#include <dross/hex/hex_topology.hpp>
#include <dross/identity/content_id.hpp>
#include <dross/identity/entity_ref.hpp>
#include <dross/random/random_hub.hpp>

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
  [[nodiscard]] CombatSessionSnapshot snapshot() const;
  [[nodiscard]] bool restore(const CombatSessionSnapshot& snapshot);

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
  class EventSink {
  public:
    virtual ~EventSink() = default;
    virtual void publish(const combat::DamageApplied& event) = 0;
    virtual void publish(const combat::ActorKilled& event) = 0;
  };

  AbilityResolver(CombatSession& session, std::vector<AbilityActorState> actors,
                  EventSink* events = nullptr, RandomStream* random = nullptr);

  [[nodiscard]] AbilityResult perform(const AbilityDefinition& ability, EntityId actor,
                                      EntityId target);
  [[nodiscard]] HitPoints health(EntityId actor) const;

private:
  CombatSession* session_;
  std::vector<AbilityActorState> actors_;
  EventSink* events_;
  RandomStream* random_;
};

} // namespace dross

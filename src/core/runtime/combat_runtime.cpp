#include <dross/runtime/combat_runtime.hpp>

#include <dross/generated/schema_codec.hpp>
#include <dross/hex/hex_coord.hpp>

#include <boost/sml.hpp>

#include <algorithm>
#include <bit>
#include <limits>
#include <set>
#include <utility>

namespace dross {
namespace {

struct Start {};
struct Advance {};
struct Complete {};
struct Inactive {};
struct Active {};
struct Completed {};

struct CombatLogger {
  template <class Machine, class Event>
  void log_process_event([[maybe_unused]] const Event& event) {}
  template <class Machine, class Guard, class Event>
  void log_guard([[maybe_unused]] const Guard& guard, [[maybe_unused]] const Event& event,
                 [[maybe_unused]] bool accepted) {}
  template <class Machine, class Action, class Event>
  void log_action([[maybe_unused]] const Action& action, [[maybe_unused]] const Event& event) {}
  template <class Machine, class Source, class Destination>
  void log_state_change([[maybe_unused]] const Source& source,
                        [[maybe_unused]] const Destination& destination) {}
};

struct CombatTable {
  auto operator()() const {
    using namespace boost::sml;
    return make_transition_table(*state<Inactive> + event<Start> = state<Active>,
                                 state<Active> + event<Advance> = state<Active>,
                                 state<Active> + event<Complete> = state<Completed>);
  }
};

struct CombatantState {
  CombatantDefinition definition;
  std::uint32_t action_points{0};
  bool alive{true};
};

} // namespace

struct CombatSession::Impl {
  explicit Impl(std::vector<CombatantDefinition> definitions, EventSink* event_sink)
      : machine{logger}, events{event_sink} {
    std::ranges::sort(definitions, [](const auto& left, const auto& right) {
      if (left.initiative != right.initiative) {
        return left.initiative > right.initiative;
      }
      return left.entity.id() < right.entity.id();
    });
    combatants.reserve(definitions.size());
    for (auto& definition : definitions) {
      combatants.push_back(CombatantState{.definition = definition});
    }
  }

  [[nodiscard]] auto find(const EntityId actor) {
    return std::ranges::find(combatants, actor, [](const CombatantState& value) {
      return value.definition.entity.id();
    });
  }

  [[nodiscard]] auto find(const EntityId actor) const {
    return std::ranges::find(combatants, actor, [](const CombatantState& value) {
      return value.definition.entity.id();
    });
  }

  [[nodiscard]] std::size_t living_count() const {
    return static_cast<std::size_t>(std::ranges::count_if(combatants, &CombatantState::alive));
  }

  CombatLogger logger;
  boost::sml::sm<CombatTable, boost::sml::logger<CombatLogger>> machine;
  std::vector<CombatantState> combatants;
  std::size_t active_index{0};
  EventSink* events;
};

void encode_combat_session_snapshot(ByteWriter& writer, const CombatSessionSnapshot& snapshot) {
  writer.write_u16(static_cast<std::uint16_t>(snapshot.state));
  writer.write_u64(snapshot.active_index);
  writer.write_u64(snapshot.combatants.size());
  for (const auto& combatant : snapshot.combatants) {
    writer.write(combatant.entity);
    writer.write_u32(std::bit_cast<std::uint32_t>(combatant.initiative));
    writer.write_u32(combatant.maximum_action_points);
    writer.write_u32(combatant.action_points);
    writer.write_u16(combatant.alive ? 1U : 0U);
  }
}

Result<CombatSessionSnapshot, DecodeError> decode_combat_session_snapshot(ByteReader& reader) {
  const auto state = reader.read_u16();
  if (!state) {
    return tl::unexpected{state.error()};
  }
  const auto active_index = reader.read_u64();
  if (!active_index) {
    return tl::unexpected{active_index.error()};
  }
  const auto count = reader.read_u64();
  if (!count) {
    return tl::unexpected{count.error()};
  }
  if (*state > static_cast<std::uint16_t>(CombatSessionState::completed) ||
      *active_index > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) ||
      *count > reader.remaining() ||
      *count > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
    return tl::unexpected{DecodeError{.position = 0, .reason = DecodeErrorReason::invalid_length}};
  }
  std::vector<CombatantSnapshot> combatants;
  combatants.reserve(static_cast<std::size_t>(*count));
  for (std::uint64_t index = 0; index < *count; ++index) {
    const auto entity = reader.read_entity_id();
    if (!entity) {
      return tl::unexpected{entity.error()};
    }
    const auto initiative = reader.read_u32();
    if (!initiative) {
      return tl::unexpected{initiative.error()};
    }
    const auto maximum_action_points = reader.read_u32();
    if (!maximum_action_points) {
      return tl::unexpected{maximum_action_points.error()};
    }
    const auto action_points = reader.read_u32();
    if (!action_points) {
      return tl::unexpected{action_points.error()};
    }
    const auto alive = reader.read_u16();
    if (!alive) {
      return tl::unexpected{alive.error()};
    }
    if (*alive > 1) {
      return tl::unexpected{
          DecodeError{.position = 0, .reason = DecodeErrorReason::invalid_length}};
    }
    combatants.push_back({.entity = *entity,
                          .initiative = std::bit_cast<std::int32_t>(*initiative),
                          .maximum_action_points = *maximum_action_points,
                          .action_points = *action_points,
                          .alive = *alive != 0});
  }
  return CombatSessionSnapshot{
      .state = static_cast<CombatSessionState>(*state),
      .active_index = static_cast<std::size_t>(*active_index),
      .combatants = std::move(combatants),
  };
}

CombatSession::CombatSession(std::vector<CombatantDefinition> combatants, EventSink* events)
    : impl_{std::make_unique<Impl>(std::move(combatants), events)} {}

Result<std::unique_ptr<CombatSession>, CombatRestoreError>
CombatSession::from_snapshot(const CombatSessionSnapshot& snapshot,
                             const WorldInstanceId world_instance, EventSink* events) {
  std::vector<CombatantDefinition> definitions;
  definitions.reserve(snapshot.combatants.size());
  for (const auto& combatant : snapshot.combatants) {
    definitions.push_back({
        .entity = EntityRef{world_instance, combatant.entity},
        .initiative = combatant.initiative,
        .maximum_action_points = combatant.maximum_action_points,
    });
  }
  auto restored = std::make_unique<CombatSession>(std::move(definitions), events);
  if (!restored->restore(snapshot)) {
    return tl::unexpected{CombatRestoreError::invalid_snapshot};
  }
  return restored;
}
CombatSession::~CombatSession() = default;
CombatSession::CombatSession(CombatSession&&) noexcept = default;
CombatSession& CombatSession::operator=(CombatSession&&) noexcept = default;

bool CombatSession::start() {
  if (impl_->combatants.size() < 2 || !impl_->machine.process_event(Start{})) {
    return false;
  }
  impl_->active_index = 0;
  while (!impl_->combatants[impl_->active_index].alive) {
    ++impl_->active_index;
  }
  auto& active = impl_->combatants[impl_->active_index];
  active.action_points = active.definition.maximum_action_points;
  if (impl_->events != nullptr) {
    impl_->events->publish(combat::CombatStarted{.active_actor = active.definition.entity});
    impl_->events->publish(combat::TurnStarted{
        .actor = active.definition.entity,
        .action_points = active.action_points,
    });
  }
  return true;
}

Result<void, CombatCommandRejection> CombatSession::handle(const combat::EndTurn& command) {
  if (state() != CombatSessionState::active ||
      impl_->combatants[impl_->active_index].definition.entity != command.actor) {
    return tl::unexpected{CombatCommandRejection::wrong_entity};
  }
  if (!end_turn(command.actor.id())) {
    return tl::unexpected{CombatCommandRejection::rejected};
  }
  return {};
}

bool CombatSession::end_turn(const EntityId actor) {
  if (state() != CombatSessionState::active || active_actor() != actor ||
      impl_->living_count() <= 1) {
    return false;
  }
  if (!impl_->machine.process_event(Advance{})) {
    return false;
  }
  do {
    impl_->active_index = (impl_->active_index + 1) % impl_->combatants.size();
  } while (!impl_->combatants[impl_->active_index].alive);
  auto& active = impl_->combatants[impl_->active_index];
  active.action_points = active.definition.maximum_action_points;
  if (impl_->events != nullptr) {
    impl_->events->publish(combat::TurnStarted{
        .actor = active.definition.entity,
        .action_points = active.action_points,
    });
  }
  return true;
}

bool CombatSession::spend_action_points(const EntityId actor, const std::uint32_t amount) {
  if (state() != CombatSessionState::active || active_actor() != actor) {
    return false;
  }
  auto& active = impl_->combatants[impl_->active_index];
  if (amount > active.action_points) {
    return false;
  }
  active.action_points -= amount;
  if (impl_->events != nullptr) {
    impl_->events->publish(combat::ActionPointsSpent{
        .actor = active.definition.entity,
        .amount = amount,
        .remaining = active.action_points,
    });
  }
  return true;
}

bool CombatSession::set_alive(const EntityId actor, const bool alive) {
  const auto found = impl_->find(actor);
  if (found == impl_->combatants.end() || found->alive == alive) {
    return false;
  }
  found->alive = alive;
  found->action_points = alive ? found->action_points : 0;
  if (state() == CombatSessionState::active && impl_->living_count() <= 1) {
    static_cast<void>(impl_->machine.process_event(Complete{}));
  }
  return true;
}

std::uint32_t CombatSession::action_points(const EntityId actor) const {
  const auto found = impl_->find(actor);
  return found == impl_->combatants.end() ? 0 : found->action_points;
}

EntityId CombatSession::active_actor() const {
  return impl_->combatants.empty() ? EntityId{0, 0}
                                   : impl_->combatants[impl_->active_index].definition.entity.id();
}

std::vector<EntityId> CombatSession::turn_order() const {
  std::vector<EntityId> result;
  result.reserve(impl_->combatants.size());
  for (const auto& combatant : impl_->combatants) {
    result.push_back(combatant.definition.entity.id());
  }
  return result;
}

CombatSessionState CombatSession::state() const {
  if (impl_->machine.is(boost::sml::state<Inactive>)) {
    return CombatSessionState::inactive;
  }
  if (impl_->machine.is(boost::sml::state<Active>)) {
    return CombatSessionState::active;
  }
  return CombatSessionState::completed;
}

CombatSessionSnapshot CombatSession::snapshot() const {
  std::vector<CombatantSnapshot> combatants;
  combatants.reserve(impl_->combatants.size());
  for (const auto& combatant : impl_->combatants) {
    combatants.push_back({
        .entity = combatant.definition.entity.id(),
        .initiative = combatant.definition.initiative,
        .maximum_action_points = combatant.definition.maximum_action_points,
        .action_points = combatant.action_points,
        .alive = combatant.alive,
    });
  }
  return {
      .state = state(),
      .active_index = impl_->active_index,
      .combatants = std::move(combatants),
  };
}

bool CombatSession::restore(const CombatSessionSnapshot& restored) {
  if (state() != CombatSessionState::inactive ||
      restored.combatants.size() != impl_->combatants.size() ||
      (!restored.combatants.empty() && restored.active_index >= restored.combatants.size())) {
    return false;
  }
  std::size_t living = 0;
  for (std::size_t index = 0; index < restored.combatants.size(); ++index) {
    const auto& saved = restored.combatants[index];
    const auto& current = impl_->combatants[index];
    if (saved.entity != current.definition.entity.id() ||
        saved.initiative != current.definition.initiative ||
        saved.maximum_action_points != current.definition.maximum_action_points ||
        saved.action_points > current.definition.maximum_action_points) {
      return false;
    }
    living += saved.alive ? 1U : 0U;
  }
  const bool valid_active = restored.state == CombatSessionState::active && living > 1 &&
                            restored.combatants[restored.active_index].alive;
  const bool valid_completed = restored.state == CombatSessionState::completed && living <= 1;
  const bool valid_inactive = restored.state == CombatSessionState::inactive;
  if (!valid_active && !valid_completed && !valid_inactive) {
    return false;
  }
  if (restored.state != CombatSessionState::inactive && !impl_->machine.process_event(Start{})) {
    return false;
  }
  if (restored.state == CombatSessionState::completed &&
      !impl_->machine.process_event(Complete{})) {
    return false;
  }
  impl_->active_index = restored.active_index;
  for (std::size_t index = 0; index < restored.combatants.size(); ++index) {
    impl_->combatants[index].action_points = restored.combatants[index].action_points;
    impl_->combatants[index].alive = restored.combatants[index].alive;
  }
  return true;
}

bool CombatSession::can_spend(const EntityId actor, const MovementCost cost) const {
  return state() == CombatSessionState::active && active_actor() == actor &&
         action_points(actor) >= cost.value();
}

bool CombatSession::spend(const EntityId actor, const MovementCost cost) {
  return spend_action_points(actor, cost.value());
}

void encode_ability_resolver_snapshot(ByteWriter& writer, const AbilityResolverSnapshot& snapshot) {
  writer.write_u64(snapshot.actors.size());
  for (const auto& actor : snapshot.actors) {
    writer.write(actor.entity);
    generated::encode_hex_pose(writer, actor.pose);
    writer.write_u32(std::bit_cast<std::uint32_t>(actor.health.value()));
  }
}

Result<AbilityResolverSnapshot, DecodeError> decode_ability_resolver_snapshot(ByteReader& reader) {
  const auto count = reader.read_u64();
  if (!count || *count > reader.remaining() ||
      *count > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
    return tl::unexpected{DecodeError{.position = 0, .reason = DecodeErrorReason::invalid_length}};
  }
  std::vector<AbilityActorSnapshot> actors;
  actors.reserve(static_cast<std::size_t>(*count));
  for (std::uint64_t index = 0; index < *count; ++index) {
    const auto entity = reader.read_entity_id();
    if (!entity) {
      return tl::unexpected{entity.error()};
    }
    auto pose = generated::decode_hex_pose(reader);
    if (!pose) {
      return tl::unexpected{pose.error()};
    }
    const auto health = reader.read_u32();
    if (!health) {
      return tl::unexpected{health.error()};
    }
    actors.push_back({
        .entity = *entity,
        .pose = *std::move(pose),
        .health = HitPoints{std::bit_cast<std::int32_t>(*health)},
    });
  }
  return AbilityResolverSnapshot{.actors = std::move(actors)};
}

AbilityResolver::AbilityResolver(CombatSession& session, std::vector<AbilityActorState> actors,
                                 EventSink* events, RandomStream* random, RuleSource* rules)
    : session_{&session}, actors_{std::move(actors)}, events_{events}, random_{random},
      rules_{rules} {
  std::ranges::sort(actors_, {}, [](const AbilityActorState& value) { return value.entity.id(); });
}

Result<std::unique_ptr<AbilityResolver>, AbilityRestoreError>
AbilityResolver::from_snapshot(CombatSession& session, const AbilityResolverSnapshot& snapshot,
                               const WorldInstanceId world_instance, EventSink* events,
                               RandomStream* random, RuleSource* rules) {
  std::set<EntityId> identities;
  std::vector<AbilityActorState> actors;
  actors.reserve(snapshot.actors.size());
  for (const auto& actor : snapshot.actors) {
    if (actor.health.value() < 0 || !identities.insert(actor.entity).second) {
      return tl::unexpected{AbilityRestoreError::invalid_snapshot};
    }
    actors.push_back({
        .entity = EntityRef{world_instance, actor.entity},
        .pose = actor.pose,
        .health = actor.health,
    });
  }
  return std::make_unique<AbilityResolver>(session, std::move(actors), events, random, rules);
}

AbilityResult AbilityResolver::perform(const AbilityDefinition& ability, const EntityId actor,
                                       const EntityId target) {
  const auto source = std::ranges::find(
      actors_, actor, [](const AbilityActorState& value) { return value.entity.id(); });
  const auto destination = std::ranges::find(
      actors_, target, [](const AbilityActorState& value) { return value.entity.id(); });
  const auto rejected = [&](const AbilityRejection reason) {
    return AbilityResult{
        .accepted = false,
        .rejection = reason,
        .damage = HitPoints{0},
        .remaining_health = destination == actors_.end() ? HitPoints{0} : destination->health,
        .killed = false,
    };
  };
  if (ability.damage.value() <= 0 || (ability.bonus_damage_max > 0 && random_ == nullptr) ||
      ability.bonus_damage_max >
          static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max() -
                                     ability.damage.value())) {
    return rejected(AbilityRejection::invalid_ability);
  }
  if (source == actors_.end()) {
    return rejected(AbilityRejection::unknown_actor);
  }
  if (session_->state() != CombatSessionState::active || session_->active_actor() != actor) {
    return rejected(AbilityRejection::inactive_actor);
  }
  if (destination == actors_.end() || actor == target ||
      source->entity.world_instance() != destination->entity.world_instance()) {
    return rejected(AbilityRejection::invalid_target);
  }
  if (destination->health.value() <= 0) {
    return rejected(AbilityRejection::target_dead);
  }
  if (source->pose.anchor.region != destination->pose.anchor.region ||
      source->pose.anchor.layer != destination->pose.anchor.layer ||
      hex_distance(source->pose.anchor.coord, destination->pose.anchor.coord) > ability.range) {
    return rejected(AbilityRejection::out_of_range);
  }
  if (rules_ != nullptr && !rules_->allows(ability, source->entity, destination->entity)) {
    return rejected(AbilityRejection::rule_rejected);
  }
  if (session_->action_points(actor) < ability.action_point_cost) {
    return rejected(AbilityRejection::insufficient_action_points);
  }
  if (!session_->spend_action_points(actor, ability.action_point_cost)) {
    return rejected(AbilityRejection::insufficient_action_points);
  }
  if (events_ != nullptr) {
    events_->publish(combat::AbilityCommitted{
        .actor = source->entity,
        .target = destination->entity,
        .ability = ability.id,
    });
  }

  const auto bonus =
      ability.bonus_damage_max == 0
          ? 0
          : static_cast<std::int32_t>(
                random_->bounded_u64(static_cast<std::uint64_t>(ability.bonus_damage_max) + 1)
                    .value());
  const auto rolled_damage = HitPoints{static_cast<std::int32_t>(ability.damage.value() + bonus)};
  const auto applied =
      rolled_damage.value() >= destination->health.value() ? destination->health : rolled_damage;
  destination->health =
      HitPoints{static_cast<std::int32_t>(destination->health.value() - applied.value())};
  const bool killed = destination->health.value() == 0;
  if (events_ != nullptr) {
    events_->publish(combat::DamageApplied{
        .source = source->entity,
        .target = destination->entity,
        .amount = applied,
        .damage_type = ability.id,
    });
  }
  if (killed) {
    static_cast<void>(session_->set_alive(target, false));
    if (events_ != nullptr) {
      events_->publish(combat::ActorKilled{
          .killer = source->entity,
          .target = destination->entity,
          .ability = ability.id,
      });
    }
  }
  return AbilityResult{
      .accepted = true,
      .rejection = AbilityRejection::none,
      .damage = applied,
      .remaining_health = destination->health,
      .killed = killed,
  };
}

AbilityResult AbilityResolver::perform(const AbilityDefinition& ability,
                                       const combat::PerformAbility& command) {
  const auto source = std::ranges::find(actors_, command.actor, &AbilityActorState::entity);
  const auto destination = std::ranges::find(actors_, command.target, &AbilityActorState::entity);
  if (command.ability != ability.id || source == actors_.end()) {
    return {.accepted = false,
            .rejection = AbilityRejection::invalid_ability,
            .damage = HitPoints{0},
            .remaining_health = HitPoints{0},
            .killed = false};
  }
  if (destination == actors_.end()) {
    return {.accepted = false,
            .rejection = AbilityRejection::invalid_target,
            .damage = HitPoints{0},
            .remaining_health = HitPoints{0},
            .killed = false};
  }
  return perform(ability, command.actor.id(), command.target.id());
}

HitPoints AbilityResolver::health(const EntityId actor) const {
  const auto found = std::ranges::find(
      actors_, actor, [](const AbilityActorState& value) { return value.entity.id(); });
  return found == actors_.end() ? HitPoints{0} : found->health;
}

AbilityResolverSnapshot AbilityResolver::snapshot() const {
  std::vector<AbilityActorSnapshot> actors;
  actors.reserve(actors_.size());
  for (const auto& actor : actors_) {
    actors.push_back({
        .entity = actor.entity.id(),
        .pose = actor.pose,
        .health = actor.health,
    });
  }
  return {.actors = std::move(actors)};
}

} // namespace dross

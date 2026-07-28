#include <dross/runtime/combat_runtime.hpp>

#include <dross/hex/hex_coord.hpp>

#include <boost/sml.hpp>

#include <algorithm>
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
  template <class Machine, class Event> void log_process_event(const Event&) {}
  template <class Machine, class Guard, class Event>
  void log_guard(const Guard&, const Event&, bool) {}
  template <class Machine, class Action, class Event>
  void log_action(const Action&, const Event&) {}
  template <class Machine, class Source, class Destination>
  void log_state_change(const Source&, const Destination&) {}
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
  explicit Impl(std::vector<CombatantDefinition> definitions) : machine{logger} {
    std::ranges::sort(definitions, [](const auto& left, const auto& right) {
      if (left.initiative != right.initiative) {
        return left.initiative > right.initiative;
      }
      return left.entity.id() < right.entity.id();
    });
    combatants.reserve(definitions.size());
    for (auto& definition : definitions) {
      combatants.push_back(CombatantState{.definition = std::move(definition)});
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
};

CombatSession::CombatSession(std::vector<CombatantDefinition> combatants)
    : impl_{std::make_unique<Impl>(std::move(combatants))} {}
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
  return true;
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

AbilityResolver::AbilityResolver(CombatSession& session, std::vector<AbilityActorState> actors)
    : session_{&session}, actors_{std::move(actors)} {
  std::ranges::sort(actors_, {}, [](const AbilityActorState& value) { return value.entity.id(); });
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
  if (ability.damage.value() <= 0) {
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
  if (session_->action_points(actor) < ability.action_point_cost) {
    return rejected(AbilityRejection::insufficient_action_points);
  }
  if (!session_->spend_action_points(actor, ability.action_point_cost)) {
    return rejected(AbilityRejection::insufficient_action_points);
  }

  const auto applied =
      ability.damage.value() >= destination->health.value() ? destination->health : ability.damage;
  destination->health =
      HitPoints{static_cast<std::int32_t>(destination->health.value() - applied.value())};
  const bool killed = destination->health.value() == 0;
  if (killed) {
    static_cast<void>(session_->set_alive(target, false));
  }
  return AbilityResult{
      .accepted = true,
      .rejection = AbilityRejection::none,
      .damage = applied,
      .remaining_health = destination->health,
      .killed = killed,
  };
}

HitPoints AbilityResolver::health(const EntityId actor) const {
  const auto found = std::ranges::find(
      actors_, actor, [](const AbilityActorState& value) { return value.entity.id(); });
  return found == actors_.end() ? HitPoints{0} : found->health;
}

} // namespace dross

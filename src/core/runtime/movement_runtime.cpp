#include <dross/runtime/movement_runtime.hpp>

#include <boost/sml.hpp>

#include <limits>
#include <utility>

namespace dross {
namespace {

struct Accept {};
struct Finish {};
struct Block {};
template <MovementLifecycleState State> struct Restore {};
struct Idle {};
struct Traversing {};
struct Blocked {};

struct MovementLogger {
  template <class Machine, class Event> void log_process_event(const Event&) {}
  template <class Machine, class Guard, class Event>
  void log_guard(const Guard&, const Event&, bool) {}
  template <class Machine, class Action, class Event>
  void log_action(const Action&, const Event&) {}
  template <class Machine, class Source, class Destination>
  void log_state_change(const Source&, const Destination&) {}
};

struct MovementTable {
  auto operator()() const {
    using namespace boost::sml;
    return make_transition_table(
        *state<Idle> + event<Accept> = state<Traversing>,
        state<Traversing> + event<Finish> = state<Idle>,
        state<Traversing> + event<Block> = state<Blocked>,
        state<Idle> + event<Restore<MovementLifecycleState::traversing>> = state<Traversing>,
        state<Idle> + event<Restore<MovementLifecycleState::blocked>> = state<Blocked>);
  }
};

} // namespace

struct MovementLifecycle::Impl {
  explicit Impl(MovementLogger& logger) : machine{logger} {}
  boost::sml::sm<MovementTable, boost::sml::logger<MovementLogger>> machine;
};

MovementLifecycle::MovementLifecycle() {
  static MovementLogger logger;
  impl_ = std::make_unique<Impl>(logger);
}
MovementLifecycle::~MovementLifecycle() = default;
MovementLifecycle::MovementLifecycle(MovementLifecycle&&) noexcept = default;
MovementLifecycle& MovementLifecycle::operator=(MovementLifecycle&&) noexcept = default;

bool MovementLifecycle::accept() { return impl_->machine.process_event(Accept{}); }
bool MovementLifecycle::finish() { return impl_->machine.process_event(Finish{}); }
bool MovementLifecycle::block() { return impl_->machine.process_event(Block{}); }

bool MovementLifecycle::restore(const MovementLifecycleState restored) {
  static MovementLogger logger;
  impl_ = std::make_unique<Impl>(logger);
  switch (restored) {
  case MovementLifecycleState::idle:
    return true;
  case MovementLifecycleState::traversing:
    return impl_->machine.process_event(Restore<MovementLifecycleState::traversing>{});
  case MovementLifecycleState::blocked:
    return impl_->machine.process_event(Restore<MovementLifecycleState::blocked>{});
  }
  return false;
}

MovementLifecycleState MovementLifecycle::state() const {
  if (impl_->machine.is(boost::sml::state<Idle>)) {
    return MovementLifecycleState::idle;
  }
  if (impl_->machine.is(boost::sml::state<Traversing>)) {
    return MovementLifecycleState::traversing;
  }
  return MovementLifecycleState::blocked;
}

MovementRuntime::MovementRuntime(const CompiledHexMap& map, OccupancyIndex& occupancy,
                                 const PathPlanner& planner, const FootprintDefinition& footprint,
                                 const EntityId entity, HexPose initial_pose,
                                 const MovementConfig config)
    : map_{&map}, occupancy_{&occupancy}, planner_{&planner}, footprint_{&footprint},
      entity_{entity}, pose_{std::move(initial_pose)}, config_{config} {}

MovementPreview MovementRuntime::preview(const HexPose& goal) const {
  const auto planned = planner_->plan(*map_, *occupancy_, *footprint_, pose_, goal,
                                      TraversalPolicy{.rotation_cost = MovementCost{0}}, entity_);
  if (!planned) {
    return MovementPreview{.accepted = false,
                           .rejection = planned.error(),
                           .path = {},
                           .cost = MovementCost{0},
                           .duration_ticks = 0,
                           .occupancy_revision = occupancy_->revision()};
  }
  const auto transitions = planned->poses.empty() ? 0U : planned->poses.size() - 1U;
  if (transitions > std::numeric_limits<std::uint64_t>::max() / config_.ticks_per_transition) {
    return MovementPreview{.accepted = false,
                           .rejection = PathError::cost_overflow,
                           .path = {},
                           .cost = MovementCost{0},
                           .duration_ticks = 0,
                           .occupancy_revision = planned->occupancy_revision};
  }
  return MovementPreview{
      .accepted = true,
      .rejection = PathError::no_path,
      .path = planned->poses,
      .cost = planned->total_cost,
      .duration_ticks = static_cast<std::uint64_t>(transitions) * config_.ticks_per_transition,
      .occupancy_revision = planned->occupancy_revision,
  };
}

bool MovementRuntime::move_to(const HexPose& goal) {
  if (combat_stop_requested_ || lifecycle_.state() != MovementLifecycleState::idle ||
      config_.ticks_per_transition == 0) {
    return false;
  }
  const auto planned = preview(goal);
  if (!planned.accepted || planned.path.size() < 2 || !lifecycle_.accept()) {
    return false;
  }
  path_ = planned.path;
  next_pose_ = 1;
  transition_ticks_ = 0;
  expected_occupancy_revision_ = planned.occupancy_revision;
  cancel_requested_ = false;
  return true;
}

bool MovementRuntime::cancel() {
  if (lifecycle_.state() != MovementLifecycleState::traversing) {
    return false;
  }
  cancel_requested_ = true;
  return true;
}

void MovementRuntime::request_combat_stop() { combat_stop_requested_ = true; }

MovementAdvance MovementRuntime::advance(const Tick tick) {
  static_cast<void>(tick);
  if (lifecycle_.state() != MovementLifecycleState::traversing) {
    return MovementAdvance::idle;
  }
  if (transition_ticks_ == 0 && occupancy_->revision() != expected_occupancy_revision_) {
    static_cast<void>(lifecycle_.block());
    return MovementAdvance::blocked;
  }
  ++transition_ticks_;
  if (transition_ticks_ < config_.ticks_per_transition) {
    return MovementAdvance::in_progress;
  }
  const auto& destination = path_[next_pose_];
  if (!occupancy_->move(entity_, footprint_->expand(destination))) {
    static_cast<void>(lifecycle_.block());
    return MovementAdvance::blocked;
  }
  pose_ = destination;
  expected_occupancy_revision_ = occupancy_->revision();
  transition_ticks_ = 0;
  ++next_pose_;
  if (cancel_requested_) {
    static_cast<void>(lifecycle_.finish());
    return MovementAdvance::cancelled;
  }
  if (combat_stop_requested_) {
    static_cast<void>(lifecycle_.finish());
    return MovementAdvance::combat_boundary;
  }
  if (next_pose_ == path_.size()) {
    static_cast<void>(lifecycle_.finish());
    return MovementAdvance::completed;
  }
  return MovementAdvance::entered_cell;
}

MovementSnapshot MovementRuntime::snapshot() const {
  return MovementSnapshot{
      .state = lifecycle_.state(),
      .pose = pose_,
      .path = path_,
      .next_pose = next_pose_,
      .transition_ticks = transition_ticks_,
      .expected_occupancy_revision = expected_occupancy_revision_,
      .cancel_requested = cancel_requested_,
      .combat_stop_requested = combat_stop_requested_,
  };
}

bool MovementRuntime::restore(const MovementSnapshot& restored) {
  const bool valid_idle =
      restored.state == MovementLifecycleState::idle && restored.transition_ticks == 0;
  const bool valid_active = restored.state != MovementLifecycleState::idle &&
                            restored.path.size() >= 2 && restored.next_pose > 0 &&
                            restored.next_pose < restored.path.size() &&
                            restored.transition_ticks < config_.ticks_per_transition &&
                            restored.path[restored.next_pose - 1] == restored.pose;
  if ((!valid_idle && !valid_active) || restored.pose != pose_ ||
      restored.expected_occupancy_revision != occupancy_->revision() ||
      !lifecycle_.restore(restored.state)) {
    return false;
  }
  pose_ = restored.pose;
  path_ = restored.path;
  next_pose_ = restored.next_pose;
  transition_ticks_ = restored.transition_ticks;
  expected_occupancy_revision_ = restored.expected_occupancy_revision;
  cancel_requested_ = restored.cancel_requested;
  combat_stop_requested_ = restored.combat_stop_requested;
  return true;
}

} // namespace dross

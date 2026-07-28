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

void encode_movement_snapshot(ByteWriter& writer, const MovementSnapshot& snapshot) {
  writer.write_u16(static_cast<std::uint16_t>(snapshot.state));
  generated::encode_hex_pose(writer, snapshot.pose);
  writer.write_u64(snapshot.path.size());
  for (const auto& path_pose : snapshot.path) {
    generated::encode_hex_pose(writer, path_pose);
  }
  writer.write_u64(snapshot.next_pose);
  writer.write_u32(snapshot.transition_ticks);
  writer.write_u64(snapshot.expected_occupancy_revision);
  writer.write_u16(snapshot.cancel_requested ? 1U : 0U);
  writer.write_u16(snapshot.combat_stop_requested ? 1U : 0U);
}

Result<MovementSnapshot, DecodeError> decode_movement_snapshot(ByteReader& reader) {
  auto state = reader.read_u16();
  auto restored_pose = generated::decode_hex_pose(reader);
  auto path_size = reader.read_u64();
  if (!state || !restored_pose || !path_size) {
    const auto error =
        !state ? state.error() : (!restored_pose ? restored_pose.error() : path_size.error());
    return tl::unexpected{error};
  }
  if (*state > static_cast<std::uint16_t>(MovementLifecycleState::blocked) ||
      *path_size > reader.remaining() ||
      *path_size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
    return tl::unexpected{DecodeError{.position = 0, .reason = DecodeErrorReason::invalid_length}};
  }
  std::vector<HexPose> path;
  path.reserve(static_cast<std::size_t>(*path_size));
  for (std::uint64_t index = 0; index < *path_size; ++index) {
    auto path_pose = generated::decode_hex_pose(reader);
    if (!path_pose) {
      return tl::unexpected{path_pose.error()};
    }
    path.push_back(*std::move(path_pose));
  }
  auto next_pose = reader.read_u64();
  auto transition_ticks = reader.read_u32();
  auto occupancy_revision = reader.read_u64();
  auto cancel_requested = reader.read_u16();
  auto combat_stop_requested = reader.read_u16();
  if (!next_pose || !transition_ticks || !occupancy_revision || !cancel_requested ||
      !combat_stop_requested) {
    const auto error =
        !next_pose
            ? next_pose.error()
            : (!transition_ticks
                   ? transition_ticks.error()
                   : (!occupancy_revision ? occupancy_revision.error()
                                          : (!cancel_requested ? cancel_requested.error()
                                                               : combat_stop_requested.error())));
    return tl::unexpected{error};
  }
  if (*next_pose > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) ||
      *cancel_requested > 1 || *combat_stop_requested > 1) {
    return tl::unexpected{DecodeError{.position = 0, .reason = DecodeErrorReason::invalid_length}};
  }
  return MovementSnapshot{
      .state = static_cast<MovementLifecycleState>(*state),
      .pose = *std::move(restored_pose),
      .path = std::move(path),
      .next_pose = static_cast<std::size_t>(*next_pose),
      .transition_ticks = *transition_ticks,
      .expected_occupancy_revision = *occupancy_revision,
      .cancel_requested = *cancel_requested != 0,
      .combat_stop_requested = *combat_stop_requested != 0,
  };
}

MovementRuntime::MovementRuntime(const CompiledHexMap& map, OccupancyIndex& occupancy,
                                 const PathPlanner& planner, const FootprintDefinition& footprint,
                                 const EntityRef entity, HexPose initial_pose,
                                 const MovementConfig config, MovementEventSink* events)
    : map_{&map}, occupancy_{&occupancy}, planner_{&planner}, footprint_{&footprint},
      entity_{entity}, pose_{std::move(initial_pose)}, config_{config}, events_{events} {}

MovementPreview MovementRuntime::preview(const HexPose& goal) const {
  const auto planned =
      planner_->plan(*map_, *occupancy_, *footprint_, pose_, goal,
                     TraversalPolicy{.rotation_cost = MovementCost{0}}, entity_.id());
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
  if (events_ != nullptr) {
    events_->publish(movement::MovementStarted{
        .entity = entity_,
        .origin = pose_,
        .destination = goal,
    });
  }
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
  if (!occupancy_->move(entity_.id(), footprint_->expand(destination))) {
    static_cast<void>(lifecycle_.block());
    return MovementAdvance::blocked;
  }
  pose_ = destination;
  expected_occupancy_revision_ = occupancy_->revision();
  transition_ticks_ = 0;
  ++next_pose_;
  if (events_ != nullptr) {
    events_->publish(movement::ActorEnteredCell{
        .entity = entity_,
        .pose = pose_,
    });
  }
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
    if (events_ != nullptr) {
      events_->publish(movement::MovementCompleted{
          .entity = entity_,
          .pose = pose_,
      });
    }
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

#include <dross/runtime/command_event_kernel.hpp>

#include <eventpp/eventqueue.h>

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace dross {
namespace {

struct QueuedPlacementEvent {
  placement::EntityPlaced payload;
  CommandMetadata source;
};

using EventppQueue = eventpp::EventQueue<int, void(const QueuedPlacementEvent&)>;
constexpr int entity_placed_event = 1;

} // namespace

struct PlacementEventQueue::Impl {
  EventppQueue invariant;
  EventppQueue capability;
  EventReactionContext* context{nullptr};
  bool draining{false};
};

struct CommandEventKernel::PendingEvent {
  placement::EntityPlaced payload;
  CommandMetadata source;
};

CommandResult CommandResult::accepted_result() noexcept {
  return CommandResult{.accepted = true, .rejection = CommandRejection::none};
}

CommandResult CommandResult::rejected(const CommandRejection reason) noexcept {
  return CommandResult{.accepted = false, .rejection = reason};
}

Result<void, RegistrationError> CommandRouter::register_place_entity(PlaceEntityHandler handler) {
  if (place_entity_handler_) {
    return tl::unexpected{RegistrationError::duplicate_command_handler};
  }
  place_entity_handler_ = std::move(handler);
  return {};
}

CommandResult CommandRouter::dispatch(const PlaceEntityEnvelope& command) const {
  if (!place_entity_handler_) {
    throw std::logic_error{"PlaceEntity handler is not registered"};
  }
  return place_entity_handler_(command);
}

void HeadlessPlacementScriptPort::reject_cell(HexCellId cell, ContentId reason) {
  rejections_.emplace_back(std::move(cell), std::move(reason));
  std::ranges::sort(rejections_, {}, &std::pair<HexCellId, ContentId>::first);
}

void HeadlessPlacementScriptPort::random_reject_cell(HexCellId cell, const RationalChance chance,
                                                     RandomStream& stream, ContentId reason) {
  random_rejections_.push_back(RandomRejection{
      .cell = std::move(cell), .chance = chance, .stream = &stream, .reason = std::move(reason)});
  std::ranges::sort(random_rejections_, {}, &RandomRejection::cell);
}

PlacementRuleContribution
HeadlessPlacementScriptPort::contribute(const placement::PlaceEntity& command) const {
  const auto found = std::ranges::find(rejections_, command.target.anchor,
                                       &std::pair<HexCellId, ContentId>::first);
  if (found != rejections_.end()) {
    return PlacementRuleContribution{
        .phase = PlacementRulePhase::script,
        .accepted = false,
        .reason = found->second,
    };
  }
  const auto random =
      std::ranges::find(random_rejections_, command.target.anchor, &RandomRejection::cell);
  if (random != random_rejections_.end() && random->stream->chance(random->chance).value()) {
    return PlacementRuleContribution{
        .phase = PlacementRulePhase::script,
        .accepted = false,
        .reason = random->reason,
    };
  }
  return PlacementRuleContribution{
      .phase = PlacementRulePhase::script,
      .accepted = true,
      .reason = std::nullopt,
  };
}

void InMemoryTraceSink::record(CommandTrace trace) { commands_.push_back(std::move(trace)); }

void InMemoryTraceSink::record(EventTrace trace) { events_.push_back(trace); }

const std::vector<CommandTrace>& InMemoryTraceSink::commands() const noexcept { return commands_; }

const std::vector<EventTrace>& InMemoryTraceSink::events() const noexcept { return events_; }

PlacementEventQueue::Subscription::Subscription(std::function<void()> remove)
    : remove_{std::move(remove)} {}

PlacementEventQueue::Subscription::~Subscription() {
  if (remove_) {
    remove_();
  }
}

PlacementEventQueue::Subscription::Subscription(Subscription&& other) noexcept
    : remove_{std::exchange(other.remove_, {})} {}

PlacementEventQueue::Subscription&
PlacementEventQueue::Subscription::operator=(Subscription&& other) noexcept {
  if (this != &other) {
    if (remove_) {
      remove_();
    }
    remove_ = std::exchange(other.remove_, {});
  }
  return *this;
}

PlacementEventQueue::PlacementEventQueue() : impl_{std::make_shared<Impl>()} {}
PlacementEventQueue::~PlacementEventQueue() = default;

Result<PlacementEventQueue::Subscription, EventRegistrationError>
PlacementEventQueue::subscribe_invariant(Listener listener) {
  const auto state = impl_;
  if (state->draining) {
    return tl::unexpected{EventRegistrationError::subscription_during_dispatch};
  }
  const auto handle = state->invariant.appendListener(
      entity_placed_event, [state, listener = std::move(listener)](const auto& event) {
        listener(event.payload, *state->context);
      });
  return Subscription{[weak = std::weak_ptr<Impl>{state}, handle] {
    if (const auto locked = weak.lock()) {
      locked->invariant.removeListener(entity_placed_event, handle);
    }
  }};
}

Result<PlacementEventQueue::Subscription, EventRegistrationError>
PlacementEventQueue::subscribe_capability(Listener listener) {
  const auto state = impl_;
  if (state->draining) {
    return tl::unexpected{EventRegistrationError::subscription_during_dispatch};
  }
  const auto handle = state->capability.appendListener(
      entity_placed_event, [state, listener = std::move(listener)](const auto& event) {
        listener(event.payload, *state->context);
      });
  return Subscription{[weak = std::weak_ptr<Impl>{state}, handle] {
    if (const auto locked = weak.lock()) {
      locked->capability.removeListener(entity_placed_event, handle);
    }
  }};
}

Result<PlacementEventQueue::Subscription, EventRegistrationError>
PlacementEventQueue::subscribe(const EventListenerPhase phase, Listener listener) {
  switch (phase) {
  case EventListenerPhase::native_invariant:
    return subscribe_invariant(std::move(listener));
  case EventListenerPhase::native_capability:
    return subscribe_capability(std::move(listener));
  }
  return tl::unexpected{EventRegistrationError::invalid_listener_phase};
}

void EventReactionContext::enqueue_follow_up(PlaceEntityEnvelope command) {
  kernel_->enqueue_follow_up(std::move(command));
}

CommandEventKernel::CommandEventKernel(WorldStorage& world, CompiledHexMap map,
                                       PlacementScriptPort& scripts, TraceSink& trace)
    : world_{&world}, map_{std::move(map)}, scripts_{&scripts}, trace_{&trace} {
  const auto registered = router_.register_place_entity(
      [this](const PlaceEntityEnvelope& command) { return handle(command); });
  if (!registered) {
    throw std::logic_error{"duplicate PlaceEntity handler"};
  }
  inspection_subscription_ =
      std::move(events_
                    .subscribe_capability(
                        [this](const placement::EntityPlaced& event, EventReactionContext&) {
                          last_placement_ =
                              LastPlacementInspection{.entity = event.entity, .pose = event.pose};
                        })
                    .value());
}

CommandEventKernel::~CommandEventKernel() = default;

void CommandEventKernel::enqueue(PlaceEntityEnvelope command) {
  pending_commands_.push_back(std::move(command));
}

bool CommandEventKernel::event_queue_draining() const noexcept { return events_.impl_->draining; }

std::vector<CommandResult> CommandEventKernel::run_cycle() {
  auto active = std::move(pending_commands_);
  pending_commands_.clear();
  std::vector<CommandResult> results;
  results.reserve(active.size());
  command_active_ = true;
  for (const auto& command : active) {
    const auto duplicate = std::ranges::find(completed_, command.metadata.id,
                                             &std::pair<CommandId, CommandResult>::first);
    if (duplicate != completed_.end()) {
      results.push_back(duplicate->second);
      trace_->record(CommandTrace{
          .metadata = command.metadata,
          .duplicate = true,
          .result = duplicate->second,
          .contributions = {},
      });
      continue;
    }
    const auto result = router_.dispatch(command);
    completed_.emplace_back(command.metadata.id, result);
    results.push_back(result);
  }
  command_active_ = false;
  drain_events();
  pending_commands_ = std::move(follow_up_commands_);
  follow_up_commands_.clear();
  return results;
}

CommandResult CommandEventKernel::handle(const PlaceEntityEnvelope& command) {
  std::vector<PlacementRulePhase> phases;
  phases.push_back(PlacementRulePhase::engine_invariant);
  if (!world_->read().valid(command.payload.entity)) {
    const auto result = CommandResult::rejected(CommandRejection::invalid_entity);
    trace_->record(CommandTrace{.metadata = command.metadata,
                                .duplicate = false,
                                .result = result,
                                .contributions = std::move(phases)});
    return result;
  }

  phases.push_back(PlacementRulePhase::hex_capability);
  const auto facts = map_.cell(command.payload.target.anchor);
  if (!facts || !facts->traversable) {
    const auto result = CommandResult::rejected(CommandRejection::invalid_target);
    trace_->record(CommandTrace{.metadata = command.metadata,
                                .duplicate = false,
                                .result = result,
                                .contributions = std::move(phases)});
    return result;
  }
  const std::vector cells{command.payload.target.anchor};
  if (!occupancy_.can_occupy(cells)) {
    const auto result = CommandResult::rejected(CommandRejection::occupied);
    trace_->record(CommandTrace{.metadata = command.metadata,
                                .duplicate = false,
                                .result = result,
                                .contributions = std::move(phases)});
    return result;
  }

  phases.push_back(PlacementRulePhase::script);
  const auto contribution = scripts_->contribute(command.payload);
  if (!contribution.accepted) {
    const auto result = CommandResult::rejected(CommandRejection::script_rejected);
    trace_->record(CommandTrace{.metadata = command.metadata,
                                .duplicate = false,
                                .result = result,
                                .contributions = std::move(phases)});
    return result;
  }

  auto staged_occupancy = occupancy_;
  const auto staged = staged_occupancy.place(command.payload.entity.id(), cells);
  if (!staged) {
    const auto result = CommandResult::rejected(CommandRejection::occupied);
    trace_->record(CommandTrace{.metadata = command.metadata,
                                .duplicate = false,
                                .result = result,
                                .contributions = std::move(phases)});
    return result;
  }

  auto staged_pose = command.payload.target;
  pending_events_.push_back(PendingEvent{
      .payload =
          placement::EntityPlaced{
              .entity = command.payload.entity,
              .pose = command.payload.target,
          },
      .source = command.metadata,
  });
  world_->write().commit_pose(command.payload.entity, std::move(staged_pose));
  occupancy_ = std::move(staged_occupancy);
  const auto result = CommandResult::accepted_result();
  trace_->record(CommandTrace{.metadata = command.metadata,
                              .duplicate = false,
                              .result = result,
                              .contributions = std::move(phases)});
  return result;
}

void CommandEventKernel::enqueue_follow_up(PlaceEntityEnvelope command) {
  follow_up_commands_.push_back(std::move(command));
}

void CommandEventKernel::drain_events() {
  EventReactionContext context{*this};
  events_.impl_->context = &context;
  events_.impl_->draining = true;
  for (const auto& event : pending_events_) {
    const auto queued = QueuedPlacementEvent{.payload = event.payload, .source = event.source};
    events_.impl_->invariant.enqueue(entity_placed_event, queued);
    events_.impl_->capability.enqueue(entity_placed_event, queued);
  }
  events_.impl_->invariant.process();
  events_.impl_->capability.process();
  for (const auto& event : pending_events_) {
    trace_->record(EventTrace{
        .source_command = event.source.id,
        .causation = event.source.causation,
        .correlation = event.source.correlation,
    });
  }
  pending_events_.clear();
  events_.impl_->draining = false;
  events_.impl_->context = nullptr;
}

std::optional<LastPlacementInspection> CommandEventKernel::last_placement() const {
  return last_placement_;
}

std::string CommandEventKernel::canonical_summary() const {
  std::ostringstream output;
  output << "occupancy=" << occupancy_.revision() << ':';
  for (const auto& entry : occupancy_.entries()) {
    output << entry.entity.lineage() << ',' << entry.entity.sequence() << '@'
           << entry.cell.region.content_id().canonical() << ',' << entry.cell.coord.q << ','
           << entry.cell.coord.r << ',' << entry.cell.layer << ';';
  }
  return output.str();
}

} // namespace dross

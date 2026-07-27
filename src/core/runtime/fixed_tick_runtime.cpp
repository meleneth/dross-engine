#include <dross/runtime/fixed_tick_runtime.hpp>

#include <algorithm>
#include <utility>

namespace dross {

Result<void, ClockError> SimulationClock::advance() {
  const auto next = current_.checked_add(1);
  if (!next) {
    return tl::unexpected{ClockError::exhausted};
  }
  current_ = *next;
  return {};
}

EngineRuntime::EngineRuntime(CommandEventKernel& kernel, WorldLifecycle& lifecycle,
                             SimulationMode& mode, const RuntimeConfig config)
    : kernel_{&kernel}, lifecycle_{&lifecycle}, mode_{&mode}, config_{config},
      clock_{Tick{0}, config.ticks_per_second} {}

void EngineRuntime::set_checkpoint_callback(std::function<void(Tick)> callback) {
  checkpoint_callback_ = std::move(callback);
}

std::vector<PlaceEntityEnvelope> EngineRuntime::pending_external_commands() const {
  std::vector<PlaceEntityEnvelope> result;
  result.reserve(external_commands_.size());
  for (const auto& scheduled : external_commands_) {
    result.push_back(scheduled.command);
  }
  return result;
}

Result<void, ScheduleError> EngineRuntime::schedule_external(PlaceEntityEnvelope command) {
  if (state_ == RuntimeState::faulted) {
    return tl::unexpected{ScheduleError::runtime_faulted};
  }
  if (lifecycle_->state() != WorldLifecycleState::running) {
    return tl::unexpected{ScheduleError::world_not_running};
  }
  if (command.metadata.tick < clock_.current()) {
    return tl::unexpected{ScheduleError::elapsed_tick};
  }
  external_commands_.push_back(ScheduledCommand{
      .sequence = next_submission_sequence_,
      .command = std::move(command),
  });
  ++next_submission_sequence_;
  return {};
}

bool EngineRuntime::request_combat() {
  return lifecycle_->state() == WorldLifecycleState::running && mode_->request_combat();
}

TickReport EngineRuntime::advance_tick() {
  TickReport report{
      .tick = clock_.current(),
      .command_cycles = 0,
      .command_ids = {},
      .command_results = {},
      .phases = {},
      .fault = std::nullopt,
  };
  if (state_ == RuntimeState::faulted) {
    return report;
  }
  if (lifecycle_->state() != WorldLifecycleState::running) {
    if (lifecycle_->state() == WorldLifecycleState::faulted) {
      state_ = RuntimeState::faulted;
    }
    return report;
  }

  report.phases.push_back(TickPhase::ingest_external);
  auto due = std::vector<ScheduledCommand>{};
  auto retained = std::vector<ScheduledCommand>{};
  due.reserve(external_commands_.size());
  retained.reserve(external_commands_.size());
  for (auto& scheduled : external_commands_) {
    if (scheduled.command.metadata.tick == clock_.current()) {
      due.push_back(std::move(scheduled));
    } else {
      retained.push_back(std::move(scheduled));
    }
  }
  external_commands_ = std::move(retained);
  std::ranges::sort(due, {}, &ScheduledCommand::sequence);
  for (auto& scheduled : due) {
    report.command_ids.push_back(scheduled.command.metadata.id);
    kernel_->enqueue(std::move(scheduled.command));
  }

  report.phases.push_back(TickPhase::process_commands);
  while (kernel_->pending_command_count() > 0 &&
         report.command_cycles < config_.max_command_cycles_per_tick) {
    auto results = kernel_->run_cycle();
    report.command_results.insert(report.command_results.end(),
                                  std::make_move_iterator(results.begin()),
                                  std::make_move_iterator(results.end()));
    ++report.command_cycles;
  }
  if (kernel_->pending_command_count() > 0) {
    state_ = RuntimeState::faulted;
    report.fault = RuntimeFault::command_cycle_budget_exhausted;
    return report;
  }

  report.phases.push_back(TickPhase::advance_time_systems);
  if (mode_->state() == SimulationModeState::combat_pending) {
    static_cast<void>(mode_->reach_safe_boundary());
  }
  report.phases.push_back(TickPhase::produce_inspection);
  report.phases.push_back(TickPhase::checkpoint);
  if (checkpoint_callback_) {
    checkpoint_callback_(clock_.current());
  }
  report.phases.push_back(TickPhase::increment_clock);
  if (!clock_.advance()) {
    state_ = RuntimeState::faulted;
    report.fault = RuntimeFault::tick_exhausted;
  }
  return report;
}

} // namespace dross

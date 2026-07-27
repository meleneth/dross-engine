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

EngineRuntime::EngineRuntime(CommandEventKernel& kernel, const RuntimeConfig config)
    : kernel_{&kernel}, config_{config}, clock_{Tick{0}, config.ticks_per_second} {}

Result<void, ScheduleError> EngineRuntime::schedule_external(PlaceEntityEnvelope command) {
  if (state_ == RuntimeState::faulted) {
    return tl::unexpected{ScheduleError::runtime_faulted};
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
  report.phases.push_back(TickPhase::produce_inspection);
  report.phases.push_back(TickPhase::checkpoint);
  report.phases.push_back(TickPhase::increment_clock);
  if (!clock_.advance()) {
    state_ = RuntimeState::faulted;
    report.fault = RuntimeFault::tick_exhausted;
  }
  return report;
}

} // namespace dross

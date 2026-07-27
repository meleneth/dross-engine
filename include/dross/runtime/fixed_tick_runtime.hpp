#pragma once

#include <dross/foundation/quantities.hpp>
#include <dross/foundation/result.hpp>
#include <dross/runtime/command_event_kernel.hpp>
#include <dross/runtime/simulation_mode.hpp>
#include <dross/runtime/world_lifecycle.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace dross {

enum class ClockError : std::uint8_t {
  exhausted,
};

class SimulationClock {
public:
  explicit SimulationClock(Tick current = Tick{0}, std::uint32_t ticks_per_second = 30) noexcept
      : current_{current}, ticks_per_second_{ticks_per_second} {}

  [[nodiscard]] Tick current() const noexcept { return current_; }
  [[nodiscard]] std::uint32_t ticks_per_second() const noexcept { return ticks_per_second_; }
  [[nodiscard]] Result<void, ClockError> advance();

private:
  Tick current_;
  std::uint32_t ticks_per_second_;
};

struct RuntimeConfig {
  std::uint32_t ticks_per_second{30};
  std::size_t max_command_cycles_per_tick{64};
};

enum class RuntimeState : std::uint8_t {
  running,
  faulted,
};

enum class RuntimeFault : std::uint8_t {
  command_cycle_budget_exhausted,
  tick_exhausted,
};

enum class ScheduleError : std::uint8_t {
  elapsed_tick,
  runtime_faulted,
  world_not_running,
};

enum class TickPhase : std::uint8_t {
  ingest_external,
  process_commands,
  advance_time_systems,
  produce_inspection,
  checkpoint,
  increment_clock,
};

struct TickReport {
  Tick tick;
  std::size_t command_cycles;
  std::vector<CommandId> command_ids;
  std::vector<CommandResult> command_results;
  std::vector<TickPhase> phases;
  std::optional<RuntimeFault> fault;
};

class EngineRuntime {
public:
  EngineRuntime(CommandEventKernel& kernel, WorldLifecycle& lifecycle, SimulationMode& mode,
                RuntimeConfig config);

  [[nodiscard]] Result<void, ScheduleError> schedule_external(PlaceEntityEnvelope command);
  [[nodiscard]] bool request_combat();
  [[nodiscard]] TickReport advance_tick();
  void set_checkpoint_callback(std::function<void(Tick)> callback);
  [[nodiscard]] std::vector<PlaceEntityEnvelope> pending_external_commands() const;

  [[nodiscard]] RuntimeState state() const noexcept { return state_; }
  [[nodiscard]] const SimulationClock& clock() const noexcept { return clock_; }
  [[nodiscard]] std::size_t pending_external_count() const noexcept {
    return external_commands_.size();
  }

private:
  struct ScheduledCommand {
    std::uint64_t sequence;
    PlaceEntityEnvelope command;
  };

  CommandEventKernel* kernel_;
  WorldLifecycle* lifecycle_;
  SimulationMode* mode_;
  RuntimeConfig config_;
  SimulationClock clock_;
  RuntimeState state_{RuntimeState::running};
  std::vector<ScheduledCommand> external_commands_;
  std::uint64_t next_submission_sequence_{0};
  std::function<void(Tick)> checkpoint_callback_;
};

} // namespace dross

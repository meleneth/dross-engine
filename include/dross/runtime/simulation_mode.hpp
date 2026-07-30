#pragma once

#include <dross/foundation/result.hpp>
#include <dross/generated/request_combat_start.hpp>
#include <dross/runtime/machine_trace.hpp>

#include <cstdint>
#include <memory>

namespace dross {

enum class SimulationModeState : std::uint8_t {
  exploration,
  combat_pending,
  combat,
};

struct SimulationModeSnapshot {
  SimulationModeState state;

  [[nodiscard]] bool operator==(const SimulationModeSnapshot&) const = default;
};

enum class SimulationModeCommandRejection : std::uint8_t {
  invalid_participants,
  rejected,
};

class SimulationMode {
public:
  explicit SimulationMode(MachineTraceSink& trace);
  ~SimulationMode();
  SimulationMode(SimulationMode&&) noexcept;
  SimulationMode& operator=(SimulationMode&&) noexcept;
  SimulationMode(const SimulationMode&) = delete;
  SimulationMode& operator=(const SimulationMode&) = delete;

  [[nodiscard]] bool request_combat();
  [[nodiscard]] Result<void, SimulationModeCommandRejection>
  handle(const combat::RequestCombatStart& command);
  [[nodiscard]] bool reach_safe_boundary();
  [[nodiscard]] bool end_combat();

  [[nodiscard]] SimulationModeState state() const;
  [[nodiscard]] SimulationModeSnapshot snapshot() const;
  [[nodiscard]] bool restore(SimulationModeSnapshot snapshot);

private:
  struct Impl;
  template <class Event> [[nodiscard]] bool process(Event event, MachineEventId event_id);

  MachineTraceSink* trace_;
  std::unique_ptr<Impl> impl_;
};

} // namespace dross

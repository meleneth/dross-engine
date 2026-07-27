#include <dross/runtime/simulation_mode.hpp>

#include <boost/sml.hpp>

#include <memory>

namespace dross {
namespace {

namespace sml = boost::sml;

struct Exploration {};
struct CombatPending {};
struct Combat {};
struct CombatRequested {};
struct SafeBoundaryReached {};
struct CombatEnded {};
template <SimulationModeState State> struct Restore {};

struct SimulationModeLogger {
  template <class Machine, class Event> void log_process_event(const Event& event) {
    static_cast<void>(event);
  }
  template <class Machine, class Guard, class Event>
  void log_guard(const Guard& guard, const Event& event, const bool result) {
    static_cast<void>(guard);
    static_cast<void>(event);
    static_cast<void>(result);
  }
  template <class Machine, class Action, class Event>
  void log_action(const Action& action, const Event& event) {
    static_cast<void>(action);
    static_cast<void>(event);
  }
  template <class Machine, class Source, class Destination>
  void log_state_change(const Source& source, const Destination& destination) {
    static_cast<void>(source);
    static_cast<void>(destination);
  }
};

struct SimulationModeDefinition {
  [[nodiscard]] auto operator()() const {
    using namespace sml;
    return make_transition_table(
        *state<Exploration> + event<CombatRequested> = state<CombatPending>,
        state<CombatPending> + event<SafeBoundaryReached> = state<Combat>,
        state<Combat> + event<CombatEnded> = state<Exploration>,
        state<Exploration> + event<Restore<SimulationModeState::combat_pending>> =
            state<CombatPending>,
        state<Exploration> + event<Restore<SimulationModeState::combat>> = state<Combat>);
  }
};

MachineStateId state_id(const SimulationModeState state) {
  switch (state) {
  case SimulationModeState::exploration:
    return MachineStateId::exploration;
  case SimulationModeState::combat_pending:
    return MachineStateId::combat_pending;
  case SimulationModeState::combat:
    return MachineStateId::combat;
  }
  return MachineStateId::exploration;
}

} // namespace

struct SimulationMode::Impl {
  explicit Impl(SimulationModeLogger& logger) : machine{logger} {}

  sml::sm<SimulationModeDefinition, sml::logger<SimulationModeLogger>> machine;
};

SimulationMode::SimulationMode(MachineTraceSink& trace) : trace_{&trace} {
  static SimulationModeLogger logger;
  impl_ = std::make_unique<Impl>(logger);
}

SimulationMode::~SimulationMode() = default;
SimulationMode::SimulationMode(SimulationMode&&) noexcept = default;
SimulationMode& SimulationMode::operator=(SimulationMode&&) noexcept = default;

SimulationModeState SimulationMode::state() const {
  if (impl_->machine.is(sml::state<Exploration>)) {
    return SimulationModeState::exploration;
  }
  if (impl_->machine.is(sml::state<CombatPending>)) {
    return SimulationModeState::combat_pending;
  }
  return SimulationModeState::combat;
}

template <class Event>
bool SimulationMode::process(Event event, const MachineEventId event_id_value) {
  const auto source = state();
  const bool accepted = impl_->machine.process_event(event);
  const auto destination = state();
  trace_->record(MachineTraceEntry{
      .machine = MachineFamily::simulation_mode,
      .source = state_id(source),
      .destination = state_id(destination),
      .event = event_id_value,
      .outcome = accepted ? MachineEventOutcome::transitioned : MachineEventOutcome::rejected,
  });
  return accepted;
}

bool SimulationMode::request_combat() {
  return process(CombatRequested{}, MachineEventId::combat_requested);
}

bool SimulationMode::reach_safe_boundary() {
  return process(SafeBoundaryReached{}, MachineEventId::safe_boundary_reached);
}

bool SimulationMode::end_combat() { return process(CombatEnded{}, MachineEventId::combat_ended); }

SimulationModeSnapshot SimulationMode::snapshot() const { return {.state = state()}; }

bool SimulationMode::restore(const SimulationModeSnapshot snapshot_value) {
  static SimulationModeLogger logger;
  impl_ = std::make_unique<Impl>(logger);
  switch (snapshot_value.state) {
  case SimulationModeState::exploration:
    return true;
  case SimulationModeState::combat_pending:
    return process(Restore<SimulationModeState::combat_pending>{}, MachineEventId::restore);
  case SimulationModeState::combat:
    return process(Restore<SimulationModeState::combat>{}, MachineEventId::restore);
  }
  return false;
}

} // namespace dross

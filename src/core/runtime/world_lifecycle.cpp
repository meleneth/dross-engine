#include <dross/runtime/world_lifecycle.hpp>

#include <boost/sml.hpp>

#include <utility>

namespace dross {
namespace {

namespace sml = boost::sml;

struct Empty {};
struct Loading {};
struct Ready {};
struct Running {};
struct Saving {};
struct Unloading {};
struct Faulted {};

struct BeginLoad {};
struct LoadSucceeded {};
struct LoadFailed {};
struct BeginRun {};
struct BeginSave {};
struct SaveSucceeded {};
struct SaveIoFailed {};
struct BeginUnload {};
struct UnloadSucceeded {};
struct FatalFault {};
template <WorldLifecycleState State> struct Restore {};

struct DrossSmlLogger {
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

struct WorldLifecycleDefinition {
  [[nodiscard]] auto operator()() const {
    using namespace sml;
    return make_transition_table(
        *state<Empty> + event<BeginLoad> = state<Loading>,
        state<Loading> + event<LoadSucceeded> = state<Ready>,
        state<Loading> + event<LoadFailed> = state<Faulted>,
        state<Ready> + event<BeginRun> = state<Running>,
        state<Running> + event<BeginSave> = state<Saving>,
        state<Saving> + event<SaveSucceeded> = state<Running>,
        state<Saving> + event<SaveIoFailed> = state<Running>,
        state<Running> + event<BeginUnload> = state<Unloading>,
        state<Unloading> + event<UnloadSucceeded> = state<Empty>,
        state<Empty> + event<FatalFault> = state<Faulted>,
        state<Loading> + event<FatalFault> = state<Faulted>,
        state<Ready> + event<FatalFault> = state<Faulted>,
        state<Running> + event<FatalFault> = state<Faulted>,
        state<Saving> + event<FatalFault> = state<Faulted>,
        state<Unloading> + event<FatalFault> = state<Faulted>,
        state<Empty> + event<Restore<WorldLifecycleState::loading>> = state<Loading>,
        state<Empty> + event<Restore<WorldLifecycleState::ready>> = state<Ready>,
        state<Empty> + event<Restore<WorldLifecycleState::running>> = state<Running>,
        state<Empty> + event<Restore<WorldLifecycleState::saving>> = state<Saving>,
        state<Empty> + event<Restore<WorldLifecycleState::unloading>> = state<Unloading>,
        state<Empty> + event<Restore<WorldLifecycleState::faulted>> = state<Faulted>);
  }
};

MachineStateId state_id(const WorldLifecycleState state) {
  switch (state) {
  case WorldLifecycleState::empty:
    return MachineStateId::world_empty;
  case WorldLifecycleState::loading:
    return MachineStateId::world_loading;
  case WorldLifecycleState::ready:
    return MachineStateId::world_ready;
  case WorldLifecycleState::running:
    return MachineStateId::world_running;
  case WorldLifecycleState::saving:
    return MachineStateId::world_saving;
  case WorldLifecycleState::unloading:
    return MachineStateId::world_unloading;
  case WorldLifecycleState::faulted:
    return MachineStateId::world_faulted;
  }
  return MachineStateId::world_faulted;
}

} // namespace

struct WorldLifecycle::Impl {
  explicit Impl(DrossSmlLogger& logger) : machine{logger} {}
  sml::sm<WorldLifecycleDefinition, sml::logger<DrossSmlLogger>> machine;
};

WorldLifecycle::WorldLifecycle(MachineTraceSink& trace) : trace_{&trace} {
  static DrossSmlLogger logger;
  impl_ = std::make_unique<Impl>(logger);
}

WorldLifecycle::~WorldLifecycle() = default;
WorldLifecycle::WorldLifecycle(WorldLifecycle&&) noexcept = default;
WorldLifecycle& WorldLifecycle::operator=(WorldLifecycle&&) noexcept = default;

WorldLifecycleState WorldLifecycle::state() const {
  if (impl_->machine.is(sml::state<Empty>)) {
    return WorldLifecycleState::empty;
  }
  if (impl_->machine.is(sml::state<Loading>)) {
    return WorldLifecycleState::loading;
  }
  if (impl_->machine.is(sml::state<Ready>)) {
    return WorldLifecycleState::ready;
  }
  if (impl_->machine.is(sml::state<Running>)) {
    return WorldLifecycleState::running;
  }
  if (impl_->machine.is(sml::state<Saving>)) {
    return WorldLifecycleState::saving;
  }
  if (impl_->machine.is(sml::state<Unloading>)) {
    return WorldLifecycleState::unloading;
  }
  return WorldLifecycleState::faulted;
}

template <class Event>
bool WorldLifecycle::process(Event event, const MachineEventId event_id_value) {
  const auto source = state();
  const bool accepted = impl_->machine.process_event(event);
  const auto destination = state();
  trace_->record(MachineTraceEntry{
      .machine = MachineFamily::world_lifecycle,
      .source = state_id(source),
      .destination = state_id(destination),
      .event = event_id_value,
      .outcome = accepted ? MachineEventOutcome::transitioned : MachineEventOutcome::rejected,
  });
  return accepted;
}

bool WorldLifecycle::begin_load() { return process(BeginLoad{}, MachineEventId::begin_load); }
bool WorldLifecycle::load_succeeded() {
  return process(LoadSucceeded{}, MachineEventId::load_succeeded);
}
bool WorldLifecycle::load_failed() { return process(LoadFailed{}, MachineEventId::load_failed); }
bool WorldLifecycle::begin_run() { return process(BeginRun{}, MachineEventId::begin_run); }
bool WorldLifecycle::begin_save() { return process(BeginSave{}, MachineEventId::begin_save); }
bool WorldLifecycle::save_succeeded() {
  return process(SaveSucceeded{}, MachineEventId::save_succeeded);
}
bool WorldLifecycle::save_io_failed() {
  return process(SaveIoFailed{}, MachineEventId::save_io_failed);
}
bool WorldLifecycle::begin_unload() { return process(BeginUnload{}, MachineEventId::begin_unload); }
bool WorldLifecycle::unload_succeeded() {
  return process(UnloadSucceeded{}, MachineEventId::unload_succeeded);
}
bool WorldLifecycle::fatal_fault() { return process(FatalFault{}, MachineEventId::fatal_fault); }

WorldLifecycleSnapshot WorldLifecycle::snapshot() const { return {.state = state()}; }

bool WorldLifecycle::restore(const WorldLifecycleSnapshot snapshot_value) {
  static DrossSmlLogger logger;
  impl_ = std::make_unique<Impl>(logger);
  switch (snapshot_value.state) {
  case WorldLifecycleState::empty:
    return true;
  case WorldLifecycleState::loading:
    return process(Restore<WorldLifecycleState::loading>{}, MachineEventId::restore);
  case WorldLifecycleState::ready:
    return process(Restore<WorldLifecycleState::ready>{}, MachineEventId::restore);
  case WorldLifecycleState::running:
    return process(Restore<WorldLifecycleState::running>{}, MachineEventId::restore);
  case WorldLifecycleState::saving:
    return process(Restore<WorldLifecycleState::saving>{}, MachineEventId::restore);
  case WorldLifecycleState::unloading:
    return process(Restore<WorldLifecycleState::unloading>{}, MachineEventId::restore);
  case WorldLifecycleState::faulted:
    return process(Restore<WorldLifecycleState::faulted>{}, MachineEventId::restore);
  }
  return false;
}

} // namespace dross

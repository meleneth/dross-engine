#include <thump_demo/fsm/caretaker_machine.hpp>

#include <boost/sml.hpp>

#include <memory>

namespace thump_demo {
namespace {

namespace sml = boost::sml;

struct WaitingForMouseContact {};
struct HuntAssigned {};
struct WaitingForTail {};
struct Settled {};
struct MouseContacted {};
struct MouseTailObserved {};
struct MouseTailReturned {};
template <CaretakerState State> struct Restore {};

struct CaretakerLogger {
  template <class Machine, class Event> void log_process_event(const Event&) {}
  template <class Machine, class Guard, class Event>
  void log_guard(const Guard&, const Event&, bool) {}
  template <class Machine, class Action, class Event>
  void log_action(const Action&, const Event&) {}
  template <class Machine, class Source, class Destination>
  void log_state_change(const Source&, const Destination&) {}
};

struct CaretakerDefinition {
  [[nodiscard]] auto operator()() const {
    using namespace sml;
    return make_transition_table(
        *state<WaitingForMouseContact> + event<MouseContacted> = state<HuntAssigned>,
        state<HuntAssigned> + event<MouseTailObserved> = state<WaitingForTail>,
        state<WaitingForTail> + event<MouseTailReturned> = state<Settled>,
        state<WaitingForMouseContact> + event<Restore<CaretakerState::hunt_assigned>> =
            state<HuntAssigned>,
        state<WaitingForMouseContact> + event<Restore<CaretakerState::waiting_for_tail>> =
            state<WaitingForTail>,
        state<WaitingForMouseContact> + event<Restore<CaretakerState::settled>> = state<Settled>);
  }
};

} // namespace

struct CaretakerMachine::Impl {
  explicit Impl(CaretakerLogger& logger) : machine{logger} {}
  sml::sm<CaretakerDefinition, sml::logger<CaretakerLogger>> machine;
};

CaretakerMachine::CaretakerMachine() {
  static CaretakerLogger logger;
  impl_ = std::make_unique<Impl>(logger);
}
CaretakerMachine::~CaretakerMachine() = default;
CaretakerMachine::CaretakerMachine(CaretakerMachine&&) noexcept = default;
CaretakerMachine& CaretakerMachine::operator=(CaretakerMachine&&) noexcept = default;

CaretakerState CaretakerMachine::state() const {
  if (impl_->machine.is(sml::state<WaitingForMouseContact>)) {
    return CaretakerState::waiting_for_mouse_contact;
  }
  if (impl_->machine.is(sml::state<HuntAssigned>)) {
    return CaretakerState::hunt_assigned;
  }
  if (impl_->machine.is(sml::state<WaitingForTail>)) {
    return CaretakerState::waiting_for_tail;
  }
  return CaretakerState::settled;
}

std::string_view CaretakerMachine::state_name() const {
  switch (state()) {
  case CaretakerState::waiting_for_mouse_contact:
    return "waiting_for_mouse_contact";
  case CaretakerState::hunt_assigned:
    return "hunt_assigned";
  case CaretakerState::waiting_for_tail:
    return "waiting_for_tail";
  case CaretakerState::settled:
    return "settled";
  }
  return "waiting_for_mouse_contact";
}

bool CaretakerMachine::contact_mouse() { return impl_->machine.process_event(MouseContacted{}); }

bool CaretakerMachine::observe_mouse_tail() {
  return impl_->machine.process_event(MouseTailObserved{});
}

bool CaretakerMachine::hand_in_tail() { return impl_->machine.process_event(MouseTailReturned{}); }

CaretakerSnapshot CaretakerMachine::snapshot() const { return {.state = state()}; }

bool CaretakerMachine::restore(const CaretakerSnapshot snapshot_value) {
  static CaretakerLogger logger;
  impl_ = std::make_unique<Impl>(logger);
  switch (snapshot_value.state) {
  case CaretakerState::waiting_for_mouse_contact:
    return true;
  case CaretakerState::hunt_assigned:
    return impl_->machine.process_event(Restore<CaretakerState::hunt_assigned>{});
  case CaretakerState::waiting_for_tail:
    return impl_->machine.process_event(Restore<CaretakerState::waiting_for_tail>{});
  case CaretakerState::settled:
    return impl_->machine.process_event(Restore<CaretakerState::settled>{});
  }
  return false;
}

} // namespace thump_demo

#include <dross/runtime/door_runtime.hpp>

#include <dross/hex/hex_coord.hpp>

#include <boost/sml.hpp>

#include <algorithm>
#include <set>
#include <utility>

namespace dross {
namespace {

struct Open {};
struct Close {};
struct Closed {};
struct Opened {};

struct DoorLogger {
  template <class Machine, class Event> void log_process_event(const Event&) {}
  template <class Machine, class Guard, class Event>
  void log_guard(const Guard&, const Event&, bool) {}
  template <class Machine, class Action, class Event>
  void log_action(const Action&, const Event&) {}
  template <class Machine, class Source, class Destination>
  void log_state_change(const Source&, const Destination&) {}
};

struct DoorTable {
  auto operator()() const {
    using namespace boost::sml;
    return make_transition_table(*state<Closed> + event<Open> = state<Opened>,
                                 state<Opened> + event<Close> = state<Closed>);
  }
};

} // namespace

Result<EdgeFootprint, EdgeFootprintError> EdgeFootprint::create(std::vector<EdgeKey> edges) {
  if (edges.empty()) {
    return tl::unexpected{EdgeFootprintError::empty};
  }
  std::ranges::sort(edges);
  if (std::ranges::adjacent_find(edges) != edges.end()) {
    return tl::unexpected{EdgeFootprintError::duplicate_edge};
  }
  for (const auto& edge : edges) {
    if (edge.first().region != edge.second().region || edge.first().layer != edge.second().layer ||
        hex_distance(edge.first().coord, edge.second().coord) != 1) {
      return tl::unexpected{EdgeFootprintError::non_adjacent_edge};
    }
  }
  return EdgeFootprint{std::move(edges)};
}

bool EdgeFootprint::contains(const EdgeKey& edge) const {
  return std::ranges::binary_search(edges_, edge);
}

struct DoorRuntime::Impl {
  Impl(const EntityRef door_entity, EdgeFootprint owned_edges, const DoorState initial_state,
       EventSink* event_sink)
      : entity{door_entity}, footprint{std::move(owned_edges)}, machine{logger},
        events{event_sink} {
    if (initial_state == DoorState::open) {
      static_cast<void>(machine.process_event(Open{}));
    }
  }

  EntityRef entity;
  EdgeFootprint footprint;
  DoorLogger logger;
  boost::sml::sm<DoorTable, boost::sml::logger<DoorLogger>> machine;
  std::set<std::uint64_t> acknowledgements;
  EventSink* events;
};

DoorRuntime::DoorRuntime(const EntityRef entity, EdgeFootprint footprint,
                         const DoorState initial_state, EventSink* events)
    : impl_{std::make_unique<Impl>(entity, std::move(footprint), initial_state, events)} {}
DoorRuntime::~DoorRuntime() = default;
DoorRuntime::DoorRuntime(DoorRuntime&&) noexcept = default;
DoorRuntime& DoorRuntime::operator=(DoorRuntime&&) noexcept = default;

bool DoorRuntime::open() {
  if (!impl_->machine.process_event(Open{})) {
    return false;
  }
  if (impl_->events != nullptr) {
    impl_->events->publish(door::DoorOpened{.door = impl_->entity});
  }
  return true;
}

bool DoorRuntime::close() {
  if (!impl_->machine.process_event(Close{})) {
    return false;
  }
  if (impl_->events != nullptr) {
    impl_->events->publish(door::DoorClosed{.door = impl_->entity});
  }
  return true;
}

DoorState DoorRuntime::state() const {
  return impl_->machine.is(boost::sml::state<Opened>) ? DoorState::open : DoorState::closed;
}

bool DoorRuntime::allows(const EdgeKey& edge) const {
  return !impl_->footprint.contains(edge) || state() == DoorState::open;
}

void DoorRuntime::acknowledge_presentation(const std::uint64_t acknowledgement_id) {
  impl_->acknowledgements.insert(acknowledgement_id);
}

} // namespace dross

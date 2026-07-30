#include <dross/runtime/dialogue_runtime.hpp>

#include <boost/sml.hpp>

#include <algorithm>
#include <set>
#include <utility>

namespace dross {
namespace {

namespace sml = boost::sml;

struct Inactive {};
struct Active {};
struct Begin {};
struct Choose {};
struct End {};
struct Restore {};

struct DialogueLogger {
  template <class Machine, class Event> void log_process_event(const Event&) {}
  template <class Machine, class Guard, class Event>
  void log_guard(const Guard&, const Event&, bool) {}
  template <class Machine, class Action, class Event>
  void log_action(const Action&, const Event&) {}
  template <class Machine, class Source, class Destination>
  void log_state_change(const Source&, const Destination&) {}
};

struct DialogueDefinition {
  [[nodiscard]] auto operator()() const {
    using namespace sml;
    return make_transition_table(*state<Inactive> + event<Begin> = state<Active>,
                                 state<Active> + event<Choose> = state<Active>,
                                 state<Active> + event<End> = state<Inactive>,
                                 state<Inactive> + event<Restore> = state<Active>);
  }
};

DecodeError invalid_snapshot(const ByteReader& reader) {
  return {.position = reader.remaining(), .reason = DecodeErrorReason::invalid_length};
}

} // namespace

void encode_dialogue_snapshot(ByteWriter& writer, const DialogueSnapshot& snapshot) {
  writer.write_u16(snapshot.session.has_value() ? 1U : 0U);
  if (!snapshot.session) {
    return;
  }
  writer.write(snapshot.session->initiator);
  writer.write(snapshot.session->partner);
  writer.write(snapshot.session->dialogue);
  writer.write_u32(static_cast<std::uint32_t>(snapshot.session->offered_options.size()));
  for (const auto& option : snapshot.session->offered_options) {
    writer.write(option);
  }
}

Result<DialogueSnapshot, DecodeError> decode_dialogue_snapshot(ByteReader& reader) {
  const auto present = reader.read_u16();
  if (!present || *present > 1U) {
    return tl::unexpected{invalid_snapshot(reader)};
  }
  if (*present == 0U) {
    return DialogueSnapshot{};
  }
  auto initiator = reader.read_entity_id();
  auto partner = reader.read_entity_id();
  auto dialogue = reader.read_content_id();
  const auto option_count = reader.read_u32();
  if (!initiator || !partner || !dialogue || !option_count) {
    return tl::unexpected{invalid_snapshot(reader)};
  }
  std::vector<ContentId> options;
  options.reserve(*option_count);
  for (std::uint32_t index = 0; index < *option_count; ++index) {
    auto option = reader.read_content_id();
    if (!option) {
      return tl::unexpected{option.error()};
    }
    options.push_back(*std::move(option));
  }
  return DialogueSnapshot{
      .session =
          DialogueSessionSnapshot{
              .initiator = *initiator,
              .partner = *partner,
              .dialogue = *std::move(dialogue),
              .offered_options = std::move(options),
          },
  };
}

struct DialogueRuntime::Impl {
  Impl(const WorldInstanceId world_instance_value, std::vector<EntityId> participant_values,
       MachineTraceSink& trace_value, EventSink* event_sink)
      : world_instance{world_instance_value},
        participants{participant_values.begin(), participant_values.end()}, trace{&trace_value},
        events{event_sink}, machine{logger} {}

  [[nodiscard]] bool active() const { return machine.is(sml::state<Active>); }

  template <class Event>
  [[nodiscard]] bool process(const Event& event, const MachineEventId event_id) {
    const auto source =
        active() ? MachineStateId::dialogue_active : MachineStateId::dialogue_inactive;
    const auto accepted = machine.process_event(event);
    trace->record(MachineTraceEntry{
        .machine = MachineFamily::dialogue,
        .source = source,
        .destination =
            active() ? MachineStateId::dialogue_active : MachineStateId::dialogue_inactive,
        .event = event_id,
        .outcome = accepted ? MachineEventOutcome::transitioned : MachineEventOutcome::rejected,
    });
    return accepted;
  }

  [[nodiscard]] bool matches(const EntityRef& initiator, const EntityRef& partner,
                             const ContentId& dialogue) const {
    return session && initiator.world_instance() == world_instance &&
           partner.world_instance() == world_instance && session->initiator == initiator.id() &&
           session->partner == partner.id() && session->dialogue == dialogue;
  }

  WorldInstanceId world_instance;
  std::set<EntityId> participants;
  MachineTraceSink* trace;
  EventSink* events;
  DialogueLogger logger;
  sml::sm<DialogueDefinition, sml::logger<DialogueLogger>> machine;
  std::optional<DialogueSessionSnapshot> session;
};

DialogueRuntime::DialogueRuntime(const WorldInstanceId world_instance,
                                 std::vector<EntityId> participants, MachineTraceSink& trace,
                                 EventSink* events)
    : impl_{std::make_unique<Impl>(world_instance, std::move(participants), trace, events)} {}

DialogueRuntime::~DialogueRuntime() = default;
DialogueRuntime::DialogueRuntime(DialogueRuntime&&) noexcept = default;
DialogueRuntime& DialogueRuntime::operator=(DialogueRuntime&&) noexcept = default;

Result<void, DialogueRejection> DialogueRuntime::handle(const dialogue::BeginDialogue& command) {
  if (command.initiator == command.partner ||
      command.initiator.world_instance() != impl_->world_instance ||
      command.partner.world_instance() != impl_->world_instance) {
    return tl::unexpected{DialogueRejection::invalid_participants};
  }
  if (!impl_->participants.contains(command.initiator.id()) ||
      !impl_->participants.contains(command.partner.id())) {
    return tl::unexpected{DialogueRejection::unknown_participant};
  }
  if (!impl_->process(Begin{}, MachineEventId::dialogue_started)) {
    return tl::unexpected{DialogueRejection::unexpected_transition};
  }
  impl_->session = DialogueSessionSnapshot{
      .initiator = command.initiator.id(),
      .partner = command.partner.id(),
      .dialogue = command.dialogue,
      .offered_options = {},
  };
  if (impl_->events != nullptr) {
    impl_->events->publish(dialogue::DialogueStarted{
        .initiator = command.initiator,
        .partner = command.partner,
        .dialogue = command.dialogue,
    });
  }
  return {};
}

Result<void, DialogueRejection>
DialogueRuntime::handle(const dialogue::ChooseDialogueOption& command) {
  if (!impl_->matches(command.initiator, command.partner, command.dialogue)) {
    return tl::unexpected{DialogueRejection::wrong_session};
  }
  if (!std::ranges::binary_search(impl_->session->offered_options, command.option)) {
    return tl::unexpected{DialogueRejection::option_not_offered};
  }
  if (!impl_->process(Choose{}, MachineEventId::dialogue_option_chosen)) {
    return tl::unexpected{DialogueRejection::unexpected_transition};
  }
  impl_->session->offered_options.clear();
  if (impl_->events != nullptr) {
    impl_->events->publish(dialogue::DialogueOptionChosen{
        .initiator = command.initiator,
        .partner = command.partner,
        .dialogue = command.dialogue,
        .option = command.option,
    });
  }
  return {};
}

Result<void, DialogueRejection> DialogueRuntime::handle(const dialogue::EndDialogue& command) {
  if (!impl_->matches(command.initiator, command.partner, command.dialogue)) {
    return tl::unexpected{DialogueRejection::wrong_session};
  }
  if (!impl_->process(End{}, MachineEventId::dialogue_ended)) {
    return tl::unexpected{DialogueRejection::unexpected_transition};
  }
  impl_->session.reset();
  if (impl_->events != nullptr) {
    impl_->events->publish(dialogue::DialogueEnded{
        .initiator = command.initiator,
        .partner = command.partner,
        .dialogue = command.dialogue,
    });
  }
  return {};
}

Result<void, DialogueRejection> DialogueRuntime::offer_options(std::vector<ContentId> options) {
  if (!impl_->active() || !impl_->session) {
    return tl::unexpected{DialogueRejection::wrong_session};
  }
  std::ranges::sort(options);
  options.erase(std::unique(options.begin(), options.end()), options.end());
  impl_->session->offered_options = std::move(options);
  return {};
}

bool DialogueRuntime::active() const { return impl_->active(); }

const std::vector<ContentId>& DialogueRuntime::offered_options() const {
  static const std::vector<ContentId> empty;
  return impl_->session ? impl_->session->offered_options : empty;
}

DialogueSnapshot DialogueRuntime::snapshot() const { return {.session = impl_->session}; }

bool DialogueRuntime::restore(const DialogueSnapshot& snapshot) {
  if (!snapshot.session) {
    impl_ = std::make_unique<Impl>(
        impl_->world_instance,
        std::vector<EntityId>{impl_->participants.begin(), impl_->participants.end()},
        *impl_->trace, impl_->events);
    return true;
  }
  const auto& session = *snapshot.session;
  if (session.initiator == session.partner || !impl_->participants.contains(session.initiator) ||
      !impl_->participants.contains(session.partner) ||
      !std::ranges::is_sorted(session.offered_options) ||
      std::ranges::adjacent_find(session.offered_options) != session.offered_options.end()) {
    return false;
  }
  auto replacement = std::make_unique<Impl>(
      impl_->world_instance,
      std::vector<EntityId>{impl_->participants.begin(), impl_->participants.end()}, *impl_->trace,
      impl_->events);
  if (!replacement->machine.process_event(Restore{})) {
    return false;
  }
  replacement->session = session;
  impl_ = std::move(replacement);
  return true;
}

} // namespace dross

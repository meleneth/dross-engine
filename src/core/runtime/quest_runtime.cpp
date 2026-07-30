#include <dross/runtime/quest_runtime.hpp>

#include <boost/sml.hpp>

#include <map>
#include <set>
#include <utility>

namespace dross {
namespace {

namespace sml = boost::sml;

struct Inactive {};
struct Active {};
struct Completed {};
struct Failed {};
struct Start {};
struct Advance {};
struct Complete {};
struct Fail {};
template <QuestStatus Status> struct Restore {};

struct QuestLogger {
  template <class Machine, class Event> void log_process_event(const Event&) {}
  template <class Machine, class Guard, class Event>
  void log_guard(const Guard&, const Event&, bool) {}
  template <class Machine, class Action, class Event>
  void log_action(const Action&, const Event&) {}
  template <class Machine, class Source, class Destination>
  void log_state_change(const Source&, const Destination&) {}
};

struct QuestDefinition {
  [[nodiscard]] auto operator()() const {
    using namespace sml;
    return make_transition_table(
        *state<Inactive> + event<Start> = state<Active>,
        state<Active> + event<Advance> = state<Active>,
        state<Active> + event<Complete> = state<Completed>,
        state<Active> + event<Fail> = state<Failed>,
        state<Inactive> + event<Restore<QuestStatus::active>> = state<Active>,
        state<Inactive> + event<Restore<QuestStatus::completed>> = state<Completed>,
        state<Inactive> + event<Restore<QuestStatus::failed>> = state<Failed>);
  }
};

MachineStateId state_id(const QuestStatus status) {
  switch (status) {
  case QuestStatus::inactive:
    return MachineStateId::quest_inactive;
  case QuestStatus::active:
    return MachineStateId::quest_active;
  case QuestStatus::completed:
    return MachineStateId::quest_completed;
  case QuestStatus::failed:
    return MachineStateId::quest_failed;
  }
  return MachineStateId::quest_inactive;
}

class QuestMachine {
public:
  QuestMachine(ContentId quest, MachineTraceSink& trace)
      : quest_{std::move(quest)}, trace_{&trace}, machine_{logger_} {}

  [[nodiscard]] QuestStatus status() const {
    if (machine_.is(sml::state<Inactive>)) {
      return QuestStatus::inactive;
    }
    if (machine_.is(sml::state<Active>)) {
      return QuestStatus::active;
    }
    if (machine_.is(sml::state<Completed>)) {
      return QuestStatus::completed;
    }
    return QuestStatus::failed;
  }

  [[nodiscard]] const ContentId& quest() const { return quest_; }
  [[nodiscard]] const std::optional<ContentId>& stage() const { return stage_; }

  [[nodiscard]] bool start(const ContentId& stage) {
    if (!process(Start{}, MachineEventId::quest_started)) {
      return false;
    }
    stage_ = stage;
    return true;
  }

  [[nodiscard]] bool advance(const ContentId& stage) {
    if (!process(Advance{}, MachineEventId::quest_advanced)) {
      return false;
    }
    stage_ = stage;
    return true;
  }

  [[nodiscard]] bool complete() {
    if (!process(Complete{}, MachineEventId::quest_completed)) {
      return false;
    }
    stage_.reset();
    return true;
  }

  [[nodiscard]] bool fail() {
    if (!process(Fail{}, MachineEventId::quest_failed)) {
      return false;
    }
    stage_.reset();
    return true;
  }

  [[nodiscard]] bool restore(const QuestStatus status, std::optional<ContentId> stage) {
    if ((status == QuestStatus::active) != stage.has_value() || status == QuestStatus::inactive) {
      return false;
    }
    bool restored = false;
    switch (status) {
    case QuestStatus::active:
      restored = machine_.process_event(Restore<QuestStatus::active>{});
      break;
    case QuestStatus::completed:
      restored = machine_.process_event(Restore<QuestStatus::completed>{});
      break;
    case QuestStatus::failed:
      restored = machine_.process_event(Restore<QuestStatus::failed>{});
      break;
    case QuestStatus::inactive:
      return false;
    }
    if (restored) {
      stage_ = std::move(stage);
    }
    return restored;
  }

private:
  template <class Event>
  [[nodiscard]] bool process(const Event& event, const MachineEventId event_id) {
    const auto source = status();
    const auto accepted = machine_.process_event(event);
    trace_->record(MachineTraceEntry{
        .machine = MachineFamily::quest,
        .source = state_id(source),
        .destination = state_id(status()),
        .event = event_id,
        .outcome = accepted ? MachineEventOutcome::transitioned : MachineEventOutcome::rejected,
    });
    return accepted;
  }

  ContentId quest_;
  MachineTraceSink* trace_;
  QuestLogger logger_;
  sml::sm<QuestDefinition, sml::logger<QuestLogger>> machine_;
  std::optional<ContentId> stage_;
};

DecodeError invalid_snapshot(const ByteReader& reader) {
  return {.position = reader.remaining(), .reason = DecodeErrorReason::invalid_length};
}

} // namespace

void encode_quest_snapshot(ByteWriter& writer, const QuestSnapshot& snapshot) {
  writer.write_u32(static_cast<std::uint32_t>(snapshot.entries.size()));
  for (const auto& entry : snapshot.entries) {
    writer.write(entry.quest);
    writer.write_u16(static_cast<std::uint16_t>(entry.status));
    writer.write_u16(entry.stage.has_value() ? 1U : 0U);
    if (entry.stage) {
      writer.write(*entry.stage);
    }
  }
}

Result<QuestSnapshot, DecodeError> decode_quest_snapshot(ByteReader& reader) {
  const auto count = reader.read_u32();
  if (!count) {
    return tl::unexpected{count.error()};
  }
  QuestSnapshot snapshot;
  snapshot.entries.reserve(*count);
  for (std::uint32_t index = 0; index < *count; ++index) {
    auto quest = reader.read_content_id();
    const auto status = reader.read_u16();
    const auto has_stage = reader.read_u16();
    if (!quest || !status || !has_stage ||
        *status > static_cast<std::uint16_t>(QuestStatus::failed) || *has_stage > 1U) {
      return tl::unexpected{invalid_snapshot(reader)};
    }
    std::optional<ContentId> stage;
    if (*has_stage == 1U) {
      auto decoded = reader.read_content_id();
      if (!decoded) {
        return tl::unexpected{decoded.error()};
      }
      stage = *std::move(decoded);
    }
    const auto decoded_status = static_cast<QuestStatus>(*status);
    if (decoded_status == QuestStatus::inactive ||
        ((decoded_status == QuestStatus::active) != stage.has_value())) {
      return tl::unexpected{invalid_snapshot(reader)};
    }
    snapshot.entries.push_back(QuestProgressSnapshot{
        .quest = *std::move(quest),
        .status = decoded_status,
        .stage = std::move(stage),
    });
  }
  return snapshot;
}

struct QuestRuntime::Impl {
  MachineTraceSink* trace;
  EventSink* events;
  std::map<ContentId, std::unique_ptr<QuestMachine>> quests;
};

QuestRuntime::QuestRuntime(MachineTraceSink& trace, EventSink* events)
    : impl_{std::make_unique<Impl>(Impl{.trace = &trace, .events = events, .quests = {}})} {}

QuestRuntime::~QuestRuntime() = default;
QuestRuntime::QuestRuntime(QuestRuntime&&) noexcept = default;
QuestRuntime& QuestRuntime::operator=(QuestRuntime&&) noexcept = default;

Result<void, QuestRejection> QuestRuntime::handle(const quest::StartQuest& command) {
  if (impl_->quests.contains(command.quest)) {
    return tl::unexpected{QuestRejection::unexpected_transition};
  }
  auto machine = std::make_unique<QuestMachine>(command.quest, *impl_->trace);
  if (!machine->start(command.stage)) {
    return tl::unexpected{QuestRejection::unexpected_transition};
  }
  impl_->quests.emplace(command.quest, std::move(machine));
  if (impl_->events != nullptr) {
    impl_->events->publish(quest::QuestStarted{.quest = command.quest, .stage = command.stage});
  }
  return {};
}

Result<void, QuestRejection> QuestRuntime::handle(const quest::AdvanceQuest& command) {
  const auto found = impl_->quests.find(command.quest);
  if (found == impl_->quests.end() || found->second->status() != QuestStatus::active) {
    return tl::unexpected{QuestRejection::not_active};
  }
  if (found->second->stage() != command.expected_stage) {
    return tl::unexpected{QuestRejection::stale_stage};
  }
  if (!found->second->advance(command.next_stage)) {
    return tl::unexpected{QuestRejection::unexpected_transition};
  }
  if (impl_->events != nullptr) {
    impl_->events->publish(quest::QuestAdvanced{
        .quest = command.quest,
        .previous_stage = command.expected_stage,
        .current_stage = command.next_stage,
    });
  }
  return {};
}

Result<void, QuestRejection> QuestRuntime::handle(const quest::CompleteQuest& command) {
  const auto found = impl_->quests.find(command.quest);
  if (found == impl_->quests.end() || found->second->status() != QuestStatus::active) {
    return tl::unexpected{QuestRejection::not_active};
  }
  if (found->second->stage() != command.expected_stage) {
    return tl::unexpected{QuestRejection::stale_stage};
  }
  if (!found->second->complete()) {
    return tl::unexpected{QuestRejection::unexpected_transition};
  }
  if (impl_->events != nullptr) {
    impl_->events->publish(quest::QuestCompleted{.quest = command.quest});
  }
  return {};
}

Result<void, QuestRejection> QuestRuntime::handle(const quest::FailQuest& command) {
  const auto found = impl_->quests.find(command.quest);
  if (found == impl_->quests.end() || found->second->status() != QuestStatus::active) {
    return tl::unexpected{QuestRejection::not_active};
  }
  if (found->second->stage() != command.expected_stage) {
    return tl::unexpected{QuestRejection::stale_stage};
  }
  if (!found->second->fail()) {
    return tl::unexpected{QuestRejection::unexpected_transition};
  }
  if (impl_->events != nullptr) {
    impl_->events->publish(quest::QuestFailed{.quest = command.quest});
  }
  return {};
}

QuestStatus QuestRuntime::status(const ContentId& quest) const {
  const auto found = impl_->quests.find(quest);
  return found == impl_->quests.end() ? QuestStatus::inactive : found->second->status();
}

std::optional<ContentId> QuestRuntime::stage(const ContentId& quest) const {
  const auto found = impl_->quests.find(quest);
  return found == impl_->quests.end() ? std::nullopt : found->second->stage();
}

QuestSnapshot QuestRuntime::snapshot() const {
  QuestSnapshot snapshot;
  snapshot.entries.reserve(impl_->quests.size());
  for (const auto& [quest, machine] : impl_->quests) {
    snapshot.entries.push_back(QuestProgressSnapshot{
        .quest = quest, .status = machine->status(), .stage = machine->stage()});
  }
  return snapshot;
}

bool QuestRuntime::restore(const QuestSnapshot& snapshot) {
  std::map<ContentId, std::unique_ptr<QuestMachine>> restored;
  for (const auto& entry : snapshot.entries) {
    auto machine = std::make_unique<QuestMachine>(entry.quest, *impl_->trace);
    if (restored.contains(entry.quest) || !machine->restore(entry.status, entry.stage)) {
      return false;
    }
    restored.emplace(entry.quest, std::move(machine));
  }
  impl_->quests = std::move(restored);
  return true;
}

} // namespace dross

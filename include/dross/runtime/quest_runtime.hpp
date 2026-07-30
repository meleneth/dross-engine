#pragma once

#include <dross/foundation/byte_codec.hpp>
#include <dross/foundation/result.hpp>
#include <dross/generated/advance_quest.hpp>
#include <dross/generated/complete_quest.hpp>
#include <dross/generated/fail_quest.hpp>
#include <dross/generated/quest_advanced.hpp>
#include <dross/generated/quest_completed.hpp>
#include <dross/generated/quest_failed.hpp>
#include <dross/generated/quest_started.hpp>
#include <dross/generated/start_quest.hpp>
#include <dross/runtime/machine_trace.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace dross {

enum class QuestStatus : std::uint8_t {
  inactive,
  active,
  completed,
  failed,
};

struct QuestProgressSnapshot {
  ContentId quest;
  QuestStatus status;
  std::optional<ContentId> stage;

  [[nodiscard]] auto operator<=>(const QuestProgressSnapshot&) const = default;
};

struct QuestSnapshot {
  std::vector<QuestProgressSnapshot> entries;

  [[nodiscard]] bool operator==(const QuestSnapshot&) const = default;
};

void encode_quest_snapshot(ByteWriter& writer, const QuestSnapshot& snapshot);
[[nodiscard]] Result<QuestSnapshot, DecodeError> decode_quest_snapshot(ByteReader& reader);

enum class QuestRejection : std::uint8_t {
  not_active,
  stale_stage,
  unexpected_transition,
};

class QuestRuntime {
public:
  class EventSink {
  public:
    virtual ~EventSink() = default;
    virtual void publish(const quest::QuestStarted& event) = 0;
    virtual void publish(const quest::QuestAdvanced& event) = 0;
    virtual void publish(const quest::QuestCompleted& event) = 0;
    virtual void publish(const quest::QuestFailed& event) = 0;
  };

  explicit QuestRuntime(MachineTraceSink& trace, EventSink* events = nullptr);
  ~QuestRuntime();
  QuestRuntime(QuestRuntime&&) noexcept;
  QuestRuntime& operator=(QuestRuntime&&) noexcept;
  QuestRuntime(const QuestRuntime&) = delete;
  QuestRuntime& operator=(const QuestRuntime&) = delete;

  [[nodiscard]] Result<void, QuestRejection> handle(const quest::StartQuest& command);
  [[nodiscard]] Result<void, QuestRejection> handle(const quest::AdvanceQuest& command);
  [[nodiscard]] Result<void, QuestRejection> handle(const quest::CompleteQuest& command);
  [[nodiscard]] Result<void, QuestRejection> handle(const quest::FailQuest& command);
  [[nodiscard]] QuestStatus status(const ContentId& quest) const;
  [[nodiscard]] std::optional<ContentId> stage(const ContentId& quest) const;
  [[nodiscard]] QuestSnapshot snapshot() const;
  [[nodiscard]] bool restore(const QuestSnapshot& snapshot);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace dross

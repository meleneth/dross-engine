#pragma once

#include <dross/foundation/byte_codec.hpp>
#include <dross/foundation/result.hpp>
#include <dross/generated/begin_dialogue.hpp>
#include <dross/generated/choose_dialogue_option.hpp>
#include <dross/generated/dialogue_ended.hpp>
#include <dross/generated/dialogue_option_chosen.hpp>
#include <dross/generated/dialogue_started.hpp>
#include <dross/generated/end_dialogue.hpp>
#include <dross/runtime/machine_trace.hpp>

#include <memory>
#include <optional>
#include <vector>

namespace dross {

struct DialogueSessionSnapshot {
  EntityId initiator;
  EntityId partner;
  ContentId dialogue;
  std::vector<ContentId> offered_options;

  [[nodiscard]] bool operator==(const DialogueSessionSnapshot&) const = default;
};

struct DialogueSnapshot {
  std::optional<DialogueSessionSnapshot> session;

  [[nodiscard]] bool operator==(const DialogueSnapshot&) const = default;
};

void encode_dialogue_snapshot(ByteWriter& writer, const DialogueSnapshot& snapshot);
[[nodiscard]] Result<DialogueSnapshot, DecodeError> decode_dialogue_snapshot(ByteReader& reader);

enum class DialogueRejection : std::uint8_t {
  invalid_participants,
  unknown_participant,
  wrong_session,
  option_not_offered,
  unexpected_transition,
};

class DialogueRuntime {
public:
  class EventSink {
  public:
    virtual ~EventSink() = default;
    virtual void publish(const dialogue::DialogueStarted& event) = 0;
    virtual void publish(const dialogue::DialogueOptionChosen& event) = 0;
    virtual void publish(const dialogue::DialogueEnded& event) = 0;
  };

  DialogueRuntime(WorldInstanceId world_instance, std::vector<EntityId> participants,
                  MachineTraceSink& trace, EventSink* events = nullptr);
  ~DialogueRuntime();
  DialogueRuntime(DialogueRuntime&&) noexcept;
  DialogueRuntime& operator=(DialogueRuntime&&) noexcept;
  DialogueRuntime(const DialogueRuntime&) = delete;
  DialogueRuntime& operator=(const DialogueRuntime&) = delete;

  [[nodiscard]] Result<void, DialogueRejection> handle(const dialogue::BeginDialogue& command);
  [[nodiscard]] Result<void, DialogueRejection>
  handle(const dialogue::ChooseDialogueOption& command);
  [[nodiscard]] Result<void, DialogueRejection> handle(const dialogue::EndDialogue& command);
  [[nodiscard]] Result<void, DialogueRejection> offer_options(std::vector<ContentId> options);
  [[nodiscard]] bool active() const;
  [[nodiscard]] const std::vector<ContentId>& offered_options() const;
  [[nodiscard]] DialogueSnapshot snapshot() const;
  [[nodiscard]] bool restore(const DialogueSnapshot& snapshot);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace dross

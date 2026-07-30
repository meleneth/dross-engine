#include <dross/runtime/dialogue_runtime.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace {

dross::ContentId id(const char* value) { return dross::ContentId::parse(value).value(); }

constexpr dross::WorldInstanceId world{12};
constexpr dross::EntityRef player{world, dross::EntityId{52, 1}};
constexpr dross::EntityRef caretaker{world, dross::EntityId{52, 3}};

class RecordingDialogueEvents final : public dross::DialogueRuntime::EventSink {
public:
  void publish(const dross::dialogue::DialogueStarted& event) override {
    calls.push_back("started/" + std::string{event.dialogue.canonical()});
  }
  void publish(const dross::dialogue::DialogueOptionChosen& event) override {
    calls.push_back("chosen/" + std::string{event.option.canonical()});
  }
  void publish(const dross::dialogue::DialogueEnded& event) override {
    calls.push_back("ended/" + std::string{event.dialogue.canonical()});
  }
  std::vector<std::string> calls;
};

dross::dialogue::BeginDialogue begin_command() {
  return {
      .initiator = player, .partner = caretaker, .dialogue = id("thump_demo:caretaker_dialogue")};
}

} // namespace

TEST_CASE("dialogue accepts only canonically offered options for the active session") {
  RecordingDialogueEvents events;
  dross::InMemoryMachineTrace trace;
  dross::DialogueRuntime dialogue{world, {player.id(), caretaker.id()}, trace, &events};
  const auto accept = id("thump_demo:accept_mouse_quest");
  const auto leave = id("thump_demo:leave");

  REQUIRE(dialogue.handle(begin_command()));
  REQUIRE(dialogue.offer_options({leave, accept, leave}));
  CHECK(dialogue.offered_options() == std::vector{accept, leave});
  REQUIRE(dialogue.handle(dross::dialogue::ChooseDialogueOption{
      .initiator = player,
      .partner = caretaker,
      .dialogue = id("thump_demo:caretaker_dialogue"),
      .option = accept,
  }));
  CHECK(dialogue.offered_options().empty());
  REQUIRE(dialogue.handle(dross::dialogue::EndDialogue{
      .initiator = player,
      .partner = caretaker,
      .dialogue = id("thump_demo:caretaker_dialogue"),
  }));
  CHECK_FALSE(dialogue.active());
  CHECK(events.calls == std::vector<std::string>{"started/thump_demo:caretaker_dialogue",
                                                 "chosen/thump_demo:accept_mouse_quest",
                                                 "ended/thump_demo:caretaker_dialogue"});
  REQUIRE(trace.entries().size() == 3);
  CHECK(trace.entries().front().machine == dross::MachineFamily::dialogue);
}

TEST_CASE("dialogue rejects unoffered stale and foreign choices without mutation") {
  RecordingDialogueEvents events;
  dross::NullMachineTrace trace;
  dross::DialogueRuntime dialogue{world, {player.id(), caretaker.id()}, trace, &events};
  REQUIRE(dialogue.handle(begin_command()));
  const auto offered = id("thump_demo:leave");
  REQUIRE(dialogue.offer_options({offered}));
  const auto before = dialogue.snapshot();
  const auto unoffered = dialogue.handle(dross::dialogue::ChooseDialogueOption{
      .initiator = player,
      .partner = caretaker,
      .dialogue = id("thump_demo:caretaker_dialogue"),
      .option = id("thump_demo:hand_over_mouse_tail"),
  });
  const auto foreign = dialogue.handle(dross::dialogue::ChooseDialogueOption{
      .initiator = dross::EntityRef{dross::WorldInstanceId{99}, player.id()},
      .partner = caretaker,
      .dialogue = id("thump_demo:caretaker_dialogue"),
      .option = offered,
  });

  REQUIRE_FALSE(unoffered);
  CHECK(unoffered.error() == dross::DialogueRejection::option_not_offered);
  REQUIRE_FALSE(foreign);
  CHECK(foreign.error() == dross::DialogueRejection::wrong_session);
  CHECK(dialogue.snapshot() == before);
  CHECK(events.calls.size() == 1);
}

TEST_CASE("dialogue rejects invalid participants and unexpected transitions") {
  dross::NullMachineTrace trace;
  dross::DialogueRuntime dialogue{world, {player.id(), caretaker.id()}, trace};
  const auto same = dialogue.handle(dross::dialogue::BeginDialogue{
      .initiator = player,
      .partner = player,
      .dialogue = id("test:dialogue"),
  });
  const auto unknown = dialogue.handle(dross::dialogue::BeginDialogue{
      .initiator = player,
      .partner = dross::EntityRef{world, dross::EntityId{52, 99}},
      .dialogue = id("test:dialogue"),
  });
  REQUIRE_FALSE(same);
  CHECK(same.error() == dross::DialogueRejection::invalid_participants);
  REQUIRE_FALSE(unknown);
  CHECK(unknown.error() == dross::DialogueRejection::unknown_participant);
  REQUIRE(dialogue.handle(begin_command()));
  CHECK_FALSE(dialogue.handle(begin_command()));
}

TEST_CASE("dialogue snapshot restores session and current offers") {
  dross::NullMachineTrace trace;
  dross::DialogueRuntime original{world, {player.id(), caretaker.id()}, trace};
  REQUIRE(original.handle(begin_command()));
  REQUIRE(original.offer_options({id("thump_demo:leave"), id("thump_demo:accept_mouse_quest")}));
  const auto snapshot = original.snapshot();

  dross::ByteWriter writer;
  dross::encode_dialogue_snapshot(writer, snapshot);
  dross::ByteReader reader{writer.bytes()};
  const auto decoded = dross::decode_dialogue_snapshot(reader);
  REQUIRE(decoded);
  CHECK(reader.remaining() == 0);

  dross::DialogueRuntime restored{world, {player.id(), caretaker.id()}, trace};
  REQUIRE(restored.restore(*decoded));
  CHECK(restored.snapshot() == snapshot);
  CHECK(restored.offered_options() == original.offered_options());

  REQUIRE(snapshot.session);
  auto duplicate_options = snapshot.session->offered_options;
  REQUIRE_FALSE(duplicate_options.empty());
  duplicate_options.push_back(duplicate_options.front());
  const auto malformed = dross::DialogueSnapshot{
      .session =
          dross::DialogueSessionSnapshot{
              .initiator = snapshot.session->initiator,
              .partner = snapshot.session->partner,
              .dialogue = snapshot.session->dialogue,
              .offered_options = std::move(duplicate_options),
          },
  };
  CHECK_FALSE(restored.restore(malformed));
  CHECK(restored.snapshot() == snapshot);
}

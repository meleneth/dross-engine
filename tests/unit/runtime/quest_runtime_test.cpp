#include <dross/runtime/quest_runtime.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace {

dross::ContentId id(const char* value) { return dross::ContentId::parse(value).value(); }

class RecordingQuestEvents final : public dross::QuestRuntime::EventSink {
public:
  void publish(const dross::quest::QuestStarted& event) override {
    calls.push_back("started/" + std::string{event.quest.canonical()} + "/" +
                    std::string{event.stage.canonical()});
  }
  void publish(const dross::quest::QuestAdvanced& event) override {
    calls.push_back("advanced/" + std::string{event.previous_stage.canonical()} + "/" +
                    std::string{event.current_stage.canonical()});
  }
  void publish(const dross::quest::QuestCompleted& event) override {
    calls.push_back("completed/" + std::string{event.quest.canonical()});
  }
  void publish(const dross::quest::QuestFailed& event) override {
    calls.push_back("failed/" + std::string{event.quest.canonical()});
  }
  std::vector<std::string> calls;
};

} // namespace

TEST_CASE("quest lifecycle starts advances and completes through typed facts") {
  RecordingQuestEvents events;
  dross::InMemoryMachineTrace trace;
  dross::QuestRuntime quests{trace, &events};
  const auto quest = id("thump_demo:mouse_quest");
  const auto hunt = id("thump_demo:hunt_mouse");
  const auto return_stage = id("thump_demo:return_tail");

  CHECK(quests.status(quest) == dross::QuestStatus::inactive);
  CHECK_FALSE(quests.stage(quest));
  REQUIRE(quests.handle(dross::quest::StartQuest{.quest = quest, .stage = hunt}));
  CHECK(quests.status(quest) == dross::QuestStatus::active);
  CHECK(quests.stage(quest) == hunt);
  REQUIRE(quests.handle(dross::quest::AdvanceQuest{
      .quest = quest, .expected_stage = hunt, .next_stage = return_stage}));
  CHECK(quests.stage(quest) == return_stage);
  REQUIRE(
      quests.handle(dross::quest::CompleteQuest{.quest = quest, .expected_stage = return_stage}));
  CHECK(quests.status(quest) == dross::QuestStatus::completed);
  CHECK_FALSE(quests.stage(quest));
  CHECK(events.calls == std::vector<std::string>{
                            "started/thump_demo:mouse_quest/thump_demo:hunt_mouse",
                            "advanced/thump_demo:hunt_mouse/thump_demo:return_tail",
                            "completed/thump_demo:mouse_quest",
                        });
  REQUIRE(trace.entries().size() == 3);
  CHECK(trace.entries().back().machine == dross::MachineFamily::quest);
}

TEST_CASE("quest unexpected and stale commands leave progress unchanged") {
  RecordingQuestEvents events;
  dross::NullMachineTrace trace;
  dross::QuestRuntime quests{trace, &events};
  const auto quest = id("thump_demo:mouse_quest");
  const auto hunt = id("thump_demo:hunt_mouse");
  const auto stale = id("thump_demo:stale");
  const auto next = id("thump_demo:return_tail");

  const auto inactive_advance = quests.handle(
      dross::quest::AdvanceQuest{.quest = quest, .expected_stage = hunt, .next_stage = next});
  REQUIRE_FALSE(inactive_advance);
  CHECK(inactive_advance.error() == dross::QuestRejection::not_active);
  REQUIRE(quests.handle(dross::quest::StartQuest{.quest = quest, .stage = hunt}));
  const auto before = quests.snapshot();
  const auto duplicate = quests.handle(dross::quest::StartQuest{.quest = quest, .stage = hunt});
  const auto stale_advance = quests.handle(
      dross::quest::AdvanceQuest{.quest = quest, .expected_stage = stale, .next_stage = next});
  const auto stale_complete =
      quests.handle(dross::quest::CompleteQuest{.quest = quest, .expected_stage = stale});

  REQUIRE_FALSE(duplicate);
  CHECK(duplicate.error() == dross::QuestRejection::unexpected_transition);
  REQUIRE_FALSE(stale_advance);
  CHECK(stale_advance.error() == dross::QuestRejection::stale_stage);
  REQUIRE_FALSE(stale_complete);
  CHECK(stale_complete.error() == dross::QuestRejection::stale_stage);
  CHECK(quests.snapshot() == before);
  CHECK(events.calls.size() == 1);
}

TEST_CASE("quest failure is terminal and every status restores") {
  dross::NullMachineTrace trace;
  const auto quest = id("test:quest");
  const auto stage = id("test:stage");
  dross::QuestRuntime failed{trace};
  REQUIRE(failed.handle(dross::quest::StartQuest{.quest = quest, .stage = stage}));
  REQUIRE(failed.handle(dross::quest::FailQuest{.quest = quest, .expected_stage = stage}));
  CHECK(failed.status(quest) == dross::QuestStatus::failed);
  CHECK_FALSE(failed.handle(dross::quest::CompleteQuest{.quest = quest, .expected_stage = stage}));

  for (const auto status :
       {dross::QuestStatus::active, dross::QuestStatus::completed, dross::QuestStatus::failed}) {
    const auto snapshot = dross::QuestSnapshot{
        .entries =
            {
                dross::QuestProgressSnapshot{
                    .quest = quest,
                    .status = status,
                    .stage =
                        status == dross::QuestStatus::active ? std::optional{stage} : std::nullopt,
                },
            },
    };
    dross::QuestRuntime restored{trace};
    REQUIRE(restored.restore(snapshot));
    CHECK(restored.snapshot() == snapshot);
  }
}

TEST_CASE("quest snapshots are canonical and reject malformed restore") {
  dross::NullMachineTrace trace;
  dross::QuestRuntime quests{trace};
  const auto first = id("test:first_quest");
  const auto second = id("test:second_quest");
  const auto stage = id("test:stage");
  REQUIRE(quests.handle(dross::quest::StartQuest{.quest = second, .stage = stage}));
  REQUIRE(quests.handle(dross::quest::StartQuest{.quest = first, .stage = stage}));
  const auto snapshot = quests.snapshot();
  CHECK(snapshot.entries.front().quest == first);

  dross::ByteWriter writer;
  dross::encode_quest_snapshot(writer, snapshot);
  dross::ByteReader reader{writer.bytes()};
  const auto decoded = dross::decode_quest_snapshot(reader);
  REQUIRE(decoded);
  CHECK(reader.remaining() == 0);
  dross::QuestRuntime restored{trace};
  REQUIRE(restored.restore(*decoded));
  CHECK(restored.snapshot() == snapshot);

  auto malformed = snapshot;
  malformed.entries.push_back(malformed.entries.front());
  CHECK_FALSE(restored.restore(malformed));
  CHECK(restored.snapshot() == snapshot);
}

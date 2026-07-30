#include <dross/runtime/script_runtime.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

dross::ContentId id(const char* value) { return dross::ContentId::parse(value).value(); }

dross::ScriptModule region_module(const char* module) {
  return dross::ScriptModule{
      .module_id = id(module),
      .scope = dross::ScriptScope::for_region(id("demo:region")),
      .state_schema_version = 1,
  };
}

dross::ScriptModule entity_module(const char* module, const std::uint64_t sequence) {
  return dross::ScriptModule{
      .module_id = id(module),
      .scope = dross::ScriptScope::for_entity(id("demo:region"), dross::EntityId{7, sequence}),
      .state_schema_version = 1,
  };
}

class RecordingPort final : public dross::ScriptRuntimePort {
public:
  bool fault_rule{false};
  bool fault_event{false};
  bool request_combat{false};
  bool grant_reward{false};
  bool start_mouse_quest{false};
  std::vector<std::string> calls;
  std::vector<std::uint64_t> rolls;

  bool discover_callbacks(const dross::ScriptModule& module) override {
    calls.push_back("discover:" + std::string{module.module_id.canonical()});
    return true;
  }

  dross::Result<void, std::string>
  contribute_placement(const dross::ScriptModule& module, const dross::placement::PlaceEntity&,
                       dross::ScriptCallbackTransaction& transaction,
                       dross::RandomStream& random) override {
    calls.push_back("rule:" + std::string{module.module_id.canonical()});
    rolls.push_back(random.next_u64());
    transaction.set_state(dross::ScriptStateKey::parse("visited").value(), true);
    transaction.add_rule(dross::ScriptRuleContribution{.accepted = true, .reason = {}});
    if (request_combat) {
      transaction.request_combat();
    }
    if (fault_rule) {
      return tl::unexpected{std::string{"rule failed"}};
    }
    return {};
  }

  dross::Result<void, std::string> contribute_ability(const dross::ScriptModule& module,
                                                      const dross::combat::PerformAbility&,
                                                      dross::ScriptCallbackTransaction& transaction,
                                                      dross::RandomStream&) override {
    calls.push_back("ability:" + std::string{module.module_id.canonical()});
    transaction.set_state(dross::ScriptStateKey::parse("ability_checked").value(), true);
    transaction.add_rule(dross::ScriptRuleContribution{.accepted = true, .reason = {}});
    return {};
  }

  dross::Result<void, std::string> on_entity_placed(const dross::ScriptModule& module,
                                                    const dross::placement::EntityPlaced&,
                                                    dross::ScriptCallbackTransaction& transaction,
                                                    dross::RandomStream&) override {
    calls.push_back("event:" + std::string{module.module_id.canonical()});
    transaction.set_state(dross::ScriptStateKey::parse("observed").value(), true);
    if (fault_event) {
      return tl::unexpected{std::string{"event failed"}};
    }
    return {};
  }

  dross::Result<void, std::string> on_actor_killed(const dross::ScriptModule&,
                                                   const dross::combat::ActorKilled& event,
                                                   dross::ScriptCallbackTransaction& transaction,
                                                   dross::RandomStream&) override {
    if (grant_reward) {
      transaction.submit(dross::inventory::GrantItem{
          .owner = event.killer,
          .item = id("thump_demo:mouse_tail"),
          .count = 1,
      });
    }
    if (start_mouse_quest) {
      transaction.submit(dross::quest::StartQuest{
          .quest = id("thump_demo:mouse_quest"),
          .stage = id("thump_demo:hunt_mouse"),
      });
    }
    if (fault_event) {
      return tl::unexpected{std::string{"event failed"}};
    }
    return {};
  }
};

dross::placement::PlaceEntity query() {
  return dross::placement::PlaceEntity{
      .entity = dross::EntityRef{dross::WorldInstanceId{1}, dross::EntityId{7, 1}},
      .target =
          dross::HexPose{
              .anchor =
                  dross::HexCellId{
                      .region = dross::RegionId{id("demo:region")},
                      .coord = dross::HexCoord{.q = 0, .r = 0},
                      .layer = 0,
                  },
              .facing = dross::HexFacing::east,
          },
  };
}

dross::combat::PerformAbility ability_query() {
  return {
      .actor = dross::EntityRef{dross::WorldInstanceId{1}, dross::EntityId{7, 1}},
      .target = dross::EntityRef{dross::WorldInstanceId{1}, dross::EntityId{7, 2}},
      .ability = id("dross_demo:thump"),
  };
}

} // namespace

TEST_CASE("script modules validate scope schema and duplicate identity") {
  RecordingPort port;
  dross::RandomHub random{dross::MasterSeed{1}};
  dross::TypedScriptRuntime runtime{port, random};

  auto invalid = region_module("demo:invalid");
  invalid.state_schema_version = 0;
  CHECK(runtime.install(std::move(invalid)).error() == dross::ScriptModuleError::state_schema_zero);

  REQUIRE(runtime.install(region_module("demo:module")));
  CHECK(runtime.install(region_module("demo:module")).error() ==
        dross::ScriptModuleError::duplicate_module_in_scope);
}

TEST_CASE("script callbacks follow region entity and module identity order") {
  RecordingPort port;
  dross::RandomHub random{dross::MasterSeed{1}};
  dross::TypedScriptRuntime runtime{port, random};
  REQUIRE(runtime.install(entity_module("demo:zeta", 2)));
  REQUIRE(runtime.install(entity_module("demo:zeta", 1)));
  REQUIRE(runtime.install(region_module("demo:zeta")));
  REQUIRE(runtime.install(entity_module("demo:alpha", 1)));
  port.calls.clear();

  const auto result = runtime.contribute_placement(query(), dross::Tick{0});
  REQUIRE(result.accepted);
  CHECK(port.calls == std::vector<std::string>{"rule:demo:zeta", "rule:demo:alpha",
                                               "rule:demo:zeta", "rule:demo:zeta"});
}

TEST_CASE("ability rule callbacks use deterministic module ordering and commit state") {
  RecordingPort port;
  dross::RandomHub random{dross::MasterSeed{1}};
  dross::TypedScriptRuntime runtime{port, random};
  REQUIRE(runtime.install(entity_module("demo:zeta", 2)));
  REQUIRE(runtime.install(region_module("demo:alpha")));
  port.calls.clear();

  const auto result = runtime.contribute_ability(ability_query(), dross::Tick{3});

  REQUIRE(result.accepted);
  CHECK(port.calls == std::vector<std::string>{"ability:demo:alpha", "ability:demo:zeta"});
  const auto value = runtime.state().find(dross::ScriptStateAddress{
      .module_id = id("demo:zeta"),
      .scope = dross::ScriptScope::for_entity(id("demo:region"), dross::EntityId{7, 2}),
      .key = dross::ScriptStateKey::parse("ability_checked").value(),
  });
  REQUIRE(value != nullptr);
  CHECK(std::get<bool>(*value));
}

TEST_CASE("rule callback fault discards buffered state") {
  RecordingPort port;
  port.fault_rule = true;
  dross::RandomHub random{dross::MasterSeed{9}};
  dross::TypedScriptRuntime runtime{port, random};
  REQUIRE(runtime.install(region_module("demo:fault")));

  const auto result = runtime.contribute_placement(query(), dross::Tick{4});
  CHECK_FALSE(result.accepted);
  REQUIRE(result.fault);
  CHECK(result.fault->callback == "contribute_placement");
  CHECK(runtime.state().values().empty());
  CHECK_FALSE(runtime.world_faulted());
}

TEST_CASE("script commands are deferred until a successful callback pass") {
  RecordingPort port;
  port.request_combat = true;
  dross::RandomHub random{dross::MasterSeed{9}};
  dross::TypedScriptRuntime runtime{port, random};
  REQUIRE(runtime.install(region_module("demo:command")));

  const auto accepted = runtime.contribute_placement(query(), dross::Tick{4});
  REQUIRE(accepted.accepted);
  CHECK(accepted.deferred_mode_commands ==
        std::vector<dross::ScriptModeCommand>{dross::ScriptModeCommand::request_combat});

  port.fault_rule = true;
  const auto faulted = runtime.contribute_placement(query(), dross::Tick{5});
  CHECK_FALSE(faulted.accepted);
  CHECK(faulted.deferred_mode_commands.empty());
}

TEST_CASE("event callback fault discards output and faults world after commit") {
  RecordingPort port;
  port.fault_event = true;
  dross::RandomHub random{dross::MasterSeed{9}};
  dross::TypedScriptRuntime runtime{port, random};
  REQUIRE(runtime.install(region_module("demo:fault")));
  const auto placement = query();

  const auto result = runtime.on_entity_placed(
      dross::placement::EntityPlaced{.entity = placement.entity, .pose = placement.target},
      dross::Tick{5});
  REQUIRE(result.fault);
  CHECK(runtime.world_faulted());
  CHECK(runtime.state().values().empty());
}

TEST_CASE("script state keys and random streams are deterministic") {
  CHECK_FALSE(dross::ScriptStateKey::parse(""));
  CHECK_FALSE(dross::ScriptStateKey::parse("Upper"));
  REQUIRE(dross::ScriptStateKey::parse("observed_1"));

  RecordingPort first_port;
  RecordingPort second_port;
  dross::RandomHub first_random{dross::MasterSeed{12345}};
  dross::RandomHub second_random{dross::MasterSeed{12345}};
  dross::TypedScriptRuntime first{first_port, first_random};
  dross::TypedScriptRuntime second{second_port, second_random};
  REQUIRE(first.install(entity_module("demo:mouse", 3)));
  REQUIRE(second.install(entity_module("demo:mouse", 3)));
  REQUIRE(first.contribute_placement(query(), dross::Tick{0}).accepted);
  REQUIRE(second.contribute_placement(query(), dross::Tick{0}).accepted);
  CHECK(first_port.rolls == second_port.rolls);
  CHECK(first.state().values() == second.state().values());
}

TEST_CASE("script state has a canonical durable codec") {
  dross::ScriptStateBag state;
  const auto module = entity_module("demo:persistent", 3);
  state.apply({
      dross::ScriptStateWrite{.address = {.module_id = module.module_id,
                                          .scope = module.scope,
                                          .key = dross::ScriptStateKey::parse("alert").value()},
                              .value = true},
      dross::ScriptStateWrite{.address = {.module_id = module.module_id,
                                          .scope = module.scope,
                                          .key = dross::ScriptStateKey::parse("count").value()},
                              .value = std::int64_t{-42}},
      dross::ScriptStateWrite{.address = {.module_id = module.module_id,
                                          .scope = module.scope,
                                          .key = dross::ScriptStateKey::parse("target").value()},
                              .value = dross::EntityId{9, 17}},
  });

  const auto encoded = dross::encode_script_state(state);
  const auto restored = dross::decode_script_state(encoded);
  REQUIRE(restored);
  CHECK(restored->values() == state.values());
  CHECK(dross::encode_script_state(*restored) == encoded);

  const std::vector<std::byte> truncated{encoded.begin(), encoded.end() - 1};
  CHECK_FALSE(dross::decode_script_state(truncated));
}

TEST_CASE("script inventory commands are deferred and discarded on callback fault") {
  RecordingPort port;
  port.grant_reward = true;
  dross::RandomHub random{dross::MasterSeed{9}};
  dross::TypedScriptRuntime runtime{port, random};
  REQUIRE(runtime.install(entity_module("thump_demo:field_mouse", 2)));
  const auto killed = dross::combat::ActorKilled{
      .killer = dross::EntityRef{dross::WorldInstanceId{1}, dross::EntityId{7, 1}},
      .target = dross::EntityRef{dross::WorldInstanceId{1}, dross::EntityId{7, 2}},
      .ability = id("thump_demo:thump"),
  };

  const auto accepted = runtime.on_actor_killed(killed, dross::Tick{1});
  REQUIRE_FALSE(accepted.fault);
  REQUIRE(accepted.deferred_inventory_commands.size() == 1);
  CHECK(accepted.deferred_inventory_commands.front().owner == killed.killer);
  CHECK(accepted.deferred_inventory_commands.front().item == id("thump_demo:mouse_tail"));

  port.fault_event = true;
  const auto rejected = runtime.on_actor_killed(killed, dross::Tick{2});
  REQUIRE(rejected.fault);
  CHECK(rejected.deferred_inventory_commands.empty());
}

TEST_CASE("script quest commands are deferred and discarded on callback fault") {
  RecordingPort port;
  port.start_mouse_quest = true;
  dross::RandomHub random{dross::MasterSeed{10}};
  dross::TypedScriptRuntime runtime{port, random};
  REQUIRE(runtime.install(entity_module("thump_demo:caretaker_dialogue", 3)));
  const auto killed = dross::combat::ActorKilled{
      .killer = dross::EntityRef{dross::WorldInstanceId{1}, dross::EntityId{7, 1}},
      .target = dross::EntityRef{dross::WorldInstanceId{1}, dross::EntityId{7, 2}},
      .ability = id("thump_demo:thump"),
  };

  const auto accepted = runtime.on_actor_killed(killed, dross::Tick{1});
  REQUIRE_FALSE(accepted.fault);
  REQUIRE(accepted.deferred_quest_commands.size() == 1);
  const auto* start =
      std::get_if<dross::quest::StartQuest>(&accepted.deferred_quest_commands.front());
  REQUIRE(start != nullptr);
  CHECK(start->quest == id("thump_demo:mouse_quest"));
  CHECK(start->stage == id("thump_demo:hunt_mouse"));

  port.fault_event = true;
  const auto rejected = runtime.on_actor_killed(killed, dross::Tick{2});
  REQUIRE(rejected.fault);
  CHECK(rejected.deferred_quest_commands.empty());
}

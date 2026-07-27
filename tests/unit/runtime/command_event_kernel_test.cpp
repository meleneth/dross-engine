#include <dross/generated/place_entity.hpp>
#include <dross/hex/compiled_hex_map.hpp>
#include <dross/runtime/command_event_kernel.hpp>
#include <dross/world/world_storage.hpp>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <utility>
#include <vector>

namespace {

using dross::HexCellId;
using dross::HexCoord;
using dross::HexFacing;
using dross::HexPose;

dross::ContentId id(const char* value) { return dross::ContentId::parse(value).value(); }

HexCellId cell(const int q, const int r) {
  return HexCellId{
      .region = dross::RegionId{id("test:arena")},
      .coord = HexCoord{.q = q, .r = r},
      .layer = 0,
  };
}

dross::CompiledHexMap map_with(std::initializer_list<HexCellId> cells) {
  dross::CompiledHexMapBuilder builder;
  for (const auto& value : cells) {
    REQUIRE(builder
                .add_cell(dross::CellFacts{
                    .id = value,
                    .surface_height = dross::Millimeters{0},
                    .terrain = id("test:floor"),
                    .base_cost = dross::MovementCost{1},
                    .clearance = dross::Clearance::open,
                    .traversable = true,
                    .semantic_tags = {},
                })
                .has_value());
  }
  return std::move(builder).build().value();
}

dross::PlaceEntityEnvelope envelope(const std::uint64_t command, const dross::EntityRef entity,
                                    const HexCellId& target) {
  return dross::PlaceEntityEnvelope{
      .metadata =
          dross::CommandMetadata{
              .id = dross::CommandId{command},
              .tick = dross::Tick{1},
              .source = dross::CommandSource::headless_test,
              .causation = dross::CausationId{command + 100},
              .correlation = dross::CorrelationId{9},
          },
      .payload =
          dross::placement::PlaceEntity{
              .entity = entity,
              .target = HexPose{.anchor = target, .facing = HexFacing::east},
          },
  };
}

struct Fixture {
  Fixture()
      : world{dross::WorldConfig{
            .lineage = 7,
            .instance_id = dross::WorldInstanceId{3},
        }},
        first{world.write().spawn(dross::SpawnPlan::runtime()).value()},
        second{world.write().spawn(dross::SpawnPlan::runtime()).value()} {}

  dross::WorldStorage world;
  dross::EntityRef first;
  dross::EntityRef second;
};

} // namespace

TEST_CASE("command router rejects duplicate authoritative handlers") {
  dross::CommandRouter router;
  REQUIRE(router.register_place_entity(
      [](const auto&) { return dross::CommandResult::accepted_result(); }));
  const auto duplicate = router.register_place_entity(
      [](const auto&) { return dross::CommandResult::accepted_result(); });
  REQUIRE_FALSE(duplicate);
  CHECK(duplicate.error() == dross::RegistrationError::duplicate_command_handler);
}

TEST_CASE("placement rules are stable and rejection leaves world unchanged") {
  Fixture fixture;
  dross::HeadlessPlacementScriptPort scripts;
  scripts.reject_cell(cell(1, 0), id("test:script_block"));
  dross::InMemoryTraceSink trace;
  dross::CommandEventKernel kernel{fixture.world, map_with({cell(0, 0), cell(1, 0)}), scripts,
                                   trace};

  kernel.enqueue(envelope(1, fixture.first, cell(1, 0)));
  const auto before = kernel.canonical_summary();
  const auto results = kernel.run_cycle();

  REQUIRE(results.size() == 1);
  CHECK_FALSE(results.front().accepted);
  CHECK(results.front().rejection == dross::CommandRejection::script_rejected);
  CHECK(kernel.canonical_summary() == before);
  REQUIRE(trace.commands().size() == 1);
  CHECK(trace.commands().front().contributions == std::vector<dross::PlacementRulePhase>{
                                                      dross::PlacementRulePhase::engine_invariant,
                                                      dross::PlacementRulePhase::hex_capability,
                                                      dross::PlacementRulePhase::script,
                                                  });
  CHECK(trace.events().empty());
}

TEST_CASE("accepted placement commits before a queued typed event is observed") {
  Fixture fixture;
  dross::HeadlessPlacementScriptPort scripts;
  dross::InMemoryTraceSink trace;
  dross::CommandEventKernel kernel{fixture.world, map_with({cell(0, 0), cell(1, 0)}), scripts,
                                   trace};
  bool observed_committed_pose = false;
  auto subscription = kernel.events().subscribe_capability(
      [&fixture, &observed_committed_pose](const dross::placement::EntityPlaced& event,
                                           dross::EventReactionContext&) {
        observed_committed_pose = fixture.world.read().pose(event.entity).value() == event.pose;
      });

  kernel.enqueue(envelope(2, fixture.first, cell(0, 0)));
  const auto results = kernel.run_cycle();

  REQUIRE(results.size() == 1);
  CHECK(results.front().accepted);
  CHECK(observed_committed_pose);
  CHECK(kernel.occupancy().occupant(cell(0, 0)) == fixture.first.id());
  REQUIRE(trace.events().size() == 1);
  CHECK(trace.events().front().source_command == dross::CommandId{2});
  CHECK(trace.events().front().causation == dross::CausationId{102});
}

TEST_CASE("blocked placement rejection preserves the canonical world summary") {
  Fixture fixture;
  dross::HeadlessPlacementScriptPort scripts;
  dross::InMemoryTraceSink trace;
  dross::CommandEventKernel kernel{fixture.world, map_with({cell(0, 0)}), scripts, trace};
  kernel.enqueue(envelope(20, fixture.first, cell(0, 0)));
  REQUIRE(kernel.run_cycle().front().accepted);
  const auto before = kernel.canonical_summary();

  kernel.enqueue(envelope(21, fixture.second, cell(0, 0)));
  const auto rejected = kernel.run_cycle();

  REQUIRE(rejected.size() == 1);
  CHECK_FALSE(rejected.front().accepted);
  CHECK(rejected.front().rejection == dross::CommandRejection::occupied);
  CHECK(kernel.canonical_summary() == before);
  CHECK_FALSE(fixture.world.read().pose(fixture.second));
}

TEST_CASE("listeners cannot reenter and follow-up commands run in the next cycle") {
  Fixture fixture;
  dross::HeadlessPlacementScriptPort scripts;
  dross::InMemoryTraceSink trace;
  dross::CommandEventKernel kernel{fixture.world, map_with({cell(0, 0), cell(1, 0)}), scripts,
                                   trace};
  bool active_during_listener = false;
  auto subscription = kernel.events().subscribe_capability(
      [&kernel, &fixture, &active_during_listener](const dross::placement::EntityPlaced&,
                                                   dross::EventReactionContext& context) {
        active_during_listener = kernel.command_active();
        context.enqueue_follow_up(envelope(4, fixture.second, cell(1, 0)));
      });

  kernel.enqueue(envelope(3, fixture.first, cell(0, 0)));
  const auto first_cycle = kernel.run_cycle();
  CHECK_FALSE(active_during_listener);
  REQUIRE(first_cycle.size() == 1);
  CHECK_FALSE(fixture.world.read().pose(fixture.second));

  const auto second_cycle = kernel.run_cycle();
  REQUIRE(second_cycle.size() == 1);
  CHECK(second_cycle.front().accepted);
  CHECK(fixture.world.read().pose(fixture.second).has_value());
}

TEST_CASE("duplicate command IDs return the previous result without a second commit") {
  Fixture fixture;
  dross::HeadlessPlacementScriptPort scripts;
  dross::InMemoryTraceSink trace;
  dross::CommandEventKernel kernel{fixture.world, map_with({cell(0, 0), cell(1, 0)}), scripts,
                                   trace};
  const auto command = envelope(5, fixture.first, cell(0, 0));

  kernel.enqueue(command);
  const auto first = kernel.run_cycle().front();
  const auto revision = kernel.occupancy().revision();
  kernel.enqueue(command);
  const auto duplicate = kernel.run_cycle().front();

  CHECK(first.accepted);
  CHECK(duplicate == first);
  CHECK(kernel.occupancy().revision() == revision);
  CHECK(trace.commands().back().duplicate);
}

TEST_CASE("event subscriptions are lifetime safe and listener phases are fixed") {
  Fixture fixture;
  dross::HeadlessPlacementScriptPort scripts;
  dross::InMemoryTraceSink trace;
  dross::CommandEventKernel kernel{fixture.world, map_with({cell(0, 0)}), scripts, trace};
  std::vector<dross::EventListenerPhase> phases;
  auto invariant = kernel.events().subscribe_invariant([&phases](const auto&, auto&) {
    phases.push_back(dross::EventListenerPhase::native_invariant);
  });
  {
    auto capability = kernel.events().subscribe_capability([&phases](const auto&, auto&) {
      phases.push_back(dross::EventListenerPhase::native_capability);
    });
  }

  kernel.enqueue(envelope(6, fixture.first, cell(0, 0)));
  const auto results = kernel.run_cycle();

  REQUIRE(results.size() == 1);
  CHECK(phases == std::vector<dross::EventListenerPhase>{
                      dross::EventListenerPhase::native_invariant,
                  });
}

TEST_CASE("event queue rejects invalid listener phases") {
  dross::PlacementEventQueue events;
  const auto invalid =
      events.subscribe(static_cast<dross::EventListenerPhase>(255), [](const auto&, auto&) {});

  REQUIRE_FALSE(invalid);
  CHECK(invalid.error() == dross::EventRegistrationError::invalid_listener_phase);
}

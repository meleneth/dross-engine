#include <dross/runtime/movement_runtime.hpp>

#include <dross/runtime/combat_runtime.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

dross::ContentId id(const char* value) { return dross::ContentId::parse(value).value(); }

dross::HexCellId cell(const std::int32_t q, const std::int32_t r) {
  return {.region = dross::RegionId{id("demo:room")}, .coord = {.q = q, .r = r}, .layer = 0};
}

dross::HexPose pose(const std::int32_t q, const std::int32_t r) {
  return {.anchor = cell(q, r), .facing = dross::HexFacing::east};
}

dross::CompiledHexMap line_map() {
  dross::CompiledHexMapBuilder builder;
  for (std::int32_t q = 0; q < 4; ++q) {
    REQUIRE(builder.add_cell(dross::CellFacts{
        .id = cell(q, 0),
        .surface_height = dross::Millimeters{0},
        .terrain = id("dross:floor"),
        .base_cost = dross::MovementCost{1},
        .clearance = dross::Clearance::open,
        .traversable = true,
        .semantic_tags = {},
    }));
  }
  for (std::int32_t q = 0; q < 3; ++q) {
    REQUIRE(builder.add_edge(
        cell(q, 0), cell(q + 1, 0),
        dross::DirectionalEdgeFacts{.traversable = true, .cost = dross::MovementCost{1}},
        dross::DirectionalEdgeFacts{.traversable = true, .cost = dross::MovementCost{1}}));
  }
  return std::move(builder).build().value();
}

dross::FootprintDefinition single_footprint() {
  return dross::FootprintDefinition::create(dross::FootprintId{id("demo:single")},
                                            {{.q = 0, .r = 0}})
      .value();
}

constexpr dross::EntityRef actor() {
  return dross::EntityRef{dross::WorldInstanceId{1}, dross::EntityId{7, 1}};
}

struct Fixture {
  dross::CompiledHexMap map{line_map()};
  dross::OccupancyIndex occupancy;
  dross::WeightedAStarPathPlanner planner;
  dross::FootprintDefinition footprint{single_footprint()};
  dross::MovementRuntime movement{map,
                                  occupancy,
                                  planner,
                                  footprint,
                                  actor(),
                                  pose(0, 0),
                                  dross::MovementConfig{.ticks_per_transition = 2}};

  Fixture() { REQUIRE(occupancy.place(actor().id(), {cell(0, 0)})); }
};

class RecordingMovementEvents final : public dross::MovementEventSink {
public:
  void publish(const dross::movement::MovementStarted&) override { calls.emplace_back("started"); }
  void publish(const dross::movement::ActorEnteredCell&) override { calls.emplace_back("entered"); }
  void publish(const dross::movement::MovementCompleted&) override {
    calls.emplace_back("completed");
  }

  std::vector<std::string> calls;
};

} // namespace

TEST_CASE("accepted movement commits occupancy only at fixed transition boundaries") {
  Fixture fixture;
  const auto preview = fixture.movement.preview(pose(2, 0));
  REQUIRE(preview.accepted);
  CHECK(preview.duration_ticks == 4);
  REQUIRE(fixture.movement.move_to(pose(2, 0)));
  CHECK(fixture.movement.preview(pose(2, 0)).path == preview.path);

  CHECK(fixture.movement.advance(dross::Tick{0}) == dross::MovementAdvance::in_progress);
  CHECK(fixture.occupancy.occupant(cell(0, 0)) == dross::EntityId{7, 1});
  CHECK_FALSE(fixture.occupancy.occupant(cell(1, 0)));
  CHECK(fixture.movement.advance(dross::Tick{1}) == dross::MovementAdvance::entered_cell);
  CHECK_FALSE(fixture.occupancy.occupant(cell(0, 0)));
  CHECK(fixture.occupancy.occupant(cell(1, 0)) == dross::EntityId{7, 1});
  CHECK(fixture.movement.pose() == pose(1, 0));

  CHECK(fixture.movement.advance(dross::Tick{2}) == dross::MovementAdvance::in_progress);
  CHECK(fixture.movement.advance(dross::Tick{3}) == dross::MovementAdvance::completed);
  CHECK(fixture.movement.pose() == pose(2, 0));
  CHECK(fixture.occupancy.occupant(cell(2, 0)) == dross::EntityId{7, 1});
}

TEST_CASE("cancel and combat pending stop movement at a committed cell boundary") {
  Fixture cancelled;
  REQUIRE(cancelled.movement.move_to(pose(3, 0)));
  REQUIRE(cancelled.movement.cancel());
  CHECK(cancelled.movement.advance(dross::Tick{0}) == dross::MovementAdvance::in_progress);
  CHECK(cancelled.movement.advance(dross::Tick{1}) == dross::MovementAdvance::cancelled);
  CHECK(cancelled.movement.pose() == pose(1, 0));
  CHECK(cancelled.movement.state() == dross::MovementLifecycleState::idle);

  Fixture pending;
  REQUIRE(pending.movement.move_to(pose(3, 0)));
  pending.movement.request_combat_stop();
  CHECK_FALSE(pending.movement.move_to(pose(2, 0)));
  static_cast<void>(pending.movement.advance(dross::Tick{0}));
  CHECK(pending.movement.advance(dross::Tick{1}) == dross::MovementAdvance::combat_boundary);
  CHECK(pending.movement.pose() == pose(1, 0));
}

TEST_CASE("dynamic occupancy revision invalidates the unstarted path tail") {
  Fixture fixture;
  REQUIRE(fixture.movement.move_to(pose(3, 0)));
  static_cast<void>(fixture.movement.advance(dross::Tick{0}));
  CHECK(fixture.movement.advance(dross::Tick{1}) == dross::MovementAdvance::entered_cell);
  REQUIRE(fixture.occupancy.place(dross::EntityId{7, 2}, {cell(2, 0)}));

  CHECK(fixture.movement.advance(dross::Tick{2}) == dross::MovementAdvance::blocked);
  CHECK(fixture.movement.pose() == pose(1, 0));
  CHECK(fixture.movement.state() == dross::MovementLifecycleState::blocked);
  CHECK(fixture.occupancy.occupant(cell(1, 0)) == dross::EntityId{7, 1});
}

TEST_CASE("movement snapshot restores a partially elapsed edge") {
  Fixture original;
  REQUIRE(original.movement.move_to(pose(2, 0)));
  CHECK(original.movement.advance(dross::Tick{0}) == dross::MovementAdvance::in_progress);
  const auto saved = original.movement.snapshot();

  Fixture restored;
  REQUIRE(restored.movement.restore(saved));
  CHECK(restored.movement.snapshot() == saved);
  CHECK(original.movement.advance(dross::Tick{1}) == dross::MovementAdvance::entered_cell);
  CHECK(restored.movement.advance(dross::Tick{1}) == dross::MovementAdvance::entered_cell);
  CHECK(original.movement.advance(dross::Tick{2}) == dross::MovementAdvance::in_progress);
  CHECK(restored.movement.advance(dross::Tick{2}) == dross::MovementAdvance::in_progress);
  CHECK(original.movement.advance(dross::Tick{3}) == dross::MovementAdvance::completed);
  CHECK(restored.movement.advance(dross::Tick{3}) == dross::MovementAdvance::completed);
  CHECK(original.movement.snapshot() == restored.movement.snapshot());
  CHECK(original.occupancy.entries() == restored.occupancy.entries());
}

TEST_CASE("idle movement snapshot restores independently of inactive occupancy revision") {
  Fixture original;
  const auto saved = original.movement.snapshot();
  REQUIRE(saved.state == dross::MovementLifecycleState::idle);
  REQUIRE(saved.expected_occupancy_revision != original.occupancy.revision());

  Fixture restored;
  REQUIRE(restored.movement.restore(saved));
  CHECK(restored.movement.snapshot() == saved);
}

TEST_CASE("movement snapshot codec resumes to the same final state") {
  Fixture original;
  REQUIRE(original.movement.move_to(pose(2, 0)));
  CHECK(original.movement.advance(dross::Tick{0}) == dross::MovementAdvance::in_progress);

  dross::ByteWriter writer;
  dross::encode_movement_snapshot(writer, original.movement.snapshot());
  dross::ByteReader reader{writer.bytes()};
  const auto decoded = dross::decode_movement_snapshot(reader);
  REQUIRE(decoded);
  CHECK(reader.remaining() == 0);

  Fixture resumed;
  REQUIRE(resumed.movement.restore(*decoded));
  for (std::uint64_t tick = 1; tick < 4; ++tick) {
    static_cast<void>(original.movement.advance(dross::Tick{tick}));
    static_cast<void>(resumed.movement.advance(dross::Tick{tick}));
  }
  CHECK(resumed.movement.snapshot() == original.movement.snapshot());
  CHECK(resumed.occupancy.entries() == original.occupancy.entries());
}

TEST_CASE("multi-cell footprint occupancy moves atomically at the edge boundary") {
  dross::CompiledHexMapBuilder builder;
  for (std::int32_t q = 0; q < 3; ++q) {
    for (std::int32_t r = 0; r < 2; ++r) {
      REQUIRE(builder.add_cell(dross::CellFacts{
          .id = cell(q, r),
          .surface_height = dross::Millimeters{0},
          .terrain = id("dross:floor"),
          .base_cost = dross::MovementCost{1},
          .clearance = dross::Clearance::open,
          .traversable = true,
          .semantic_tags = {},
      }));
    }
    if (q > 0) {
      for (std::int32_t r = 0; r < 2; ++r) {
        REQUIRE(builder.add_edge(
            cell(q - 1, r), cell(q, r),
            dross::DirectionalEdgeFacts{.traversable = true, .cost = dross::MovementCost{1}},
            dross::DirectionalEdgeFacts{.traversable = true, .cost = dross::MovementCost{1}}));
      }
    }
  }
  auto map = std::move(builder).build().value();
  dross::OccupancyIndex occupancy;
  dross::WeightedAStarPathPlanner planner;
  auto footprint = dross::FootprintDefinition::create(dross::FootprintId{id("demo:double")},
                                                      {{.q = 0, .r = 0}, {.q = 0, .r = 1}})
                       .value();
  const dross::EntityId entity{7, 1};
  REQUIRE(occupancy.place(entity, footprint.expand(pose(0, 0))));
  dross::MovementRuntime movement{map,
                                  occupancy,
                                  planner,
                                  footprint,
                                  dross::EntityRef{dross::WorldInstanceId{1}, entity},
                                  pose(0, 0),
                                  {.ticks_per_transition = 2}};
  REQUIRE(movement.move_to(pose(1, 0)));

  CHECK(movement.advance(dross::Tick{0}) == dross::MovementAdvance::in_progress);
  CHECK(occupancy.occupant(cell(0, 0)) == entity);
  CHECK(occupancy.occupant(cell(0, 1)) == entity);
  CHECK_FALSE(occupancy.occupant(cell(1, 0)));
  CHECK_FALSE(occupancy.occupant(cell(1, 1)));
  CHECK(movement.advance(dross::Tick{1}) == dross::MovementAdvance::completed);
  CHECK_FALSE(occupancy.occupant(cell(0, 0)));
  CHECK_FALSE(occupancy.occupant(cell(0, 1)));
  CHECK(occupancy.occupant(cell(1, 0)) == entity);
  CHECK(occupancy.occupant(cell(1, 1)) == entity);
}

TEST_CASE("movement facts follow authoritative cell commits") {
  auto map = line_map();
  dross::OccupancyIndex occupancy;
  dross::WeightedAStarPathPlanner planner;
  auto footprint = single_footprint();
  RecordingMovementEvents events;
  REQUIRE(occupancy.place(actor().id(), {cell(0, 0)}));
  dross::MovementRuntime movement{
      map,    occupancy, planner, footprint, actor(), pose(0, 0), {.ticks_per_transition = 2},
      &events};

  REQUIRE(movement.move_to(pose(2, 0)));
  CHECK(events.calls == std::vector<std::string>{"started"});
  CHECK(movement.advance(dross::Tick{0}) == dross::MovementAdvance::in_progress);
  CHECK(events.calls == std::vector<std::string>{"started"});
  CHECK(movement.advance(dross::Tick{1}) == dross::MovementAdvance::entered_cell);
  CHECK(events.calls == std::vector<std::string>{"started", "entered"});
  CHECK(movement.advance(dross::Tick{2}) == dross::MovementAdvance::in_progress);
  CHECK(events.calls == std::vector<std::string>{"started", "entered"});
  CHECK(movement.advance(dross::Tick{3}) == dross::MovementAdvance::completed);
  CHECK(events.calls == std::vector<std::string>{"started", "entered", "entered", "completed"});
}

TEST_CASE("typed movement commands validate actor identity before changing state") {
  auto map = line_map();
  dross::OccupancyIndex occupancy;
  dross::WeightedAStarPathPlanner planner;
  auto footprint = single_footprint();
  REQUIRE(occupancy.place(actor().id(), {cell(0, 0)}));
  dross::MovementRuntime movement{
      map, occupancy, planner, footprint, actor(), pose(0, 0), {.ticks_per_transition = 2}};
  const dross::EntityRef stranger{dross::WorldInstanceId{1}, dross::EntityId{7, 2}};

  const auto rejected =
      movement.handle(dross::movement::MoveTo{.entity = stranger, .destination = pose(2, 0)});
  REQUIRE_FALSE(rejected);
  CHECK(rejected.error() == dross::MovementCommandRejection::wrong_entity);
  CHECK(movement.state() == dross::MovementLifecycleState::idle);
  CHECK(movement.pose() == pose(0, 0));

  REQUIRE(movement.handle(dross::movement::MoveTo{.entity = actor(), .destination = pose(2, 0)}));
  const auto wrong_cancel = movement.handle(dross::movement::CancelMovement{.entity = stranger});
  REQUIRE_FALSE(wrong_cancel);
  CHECK(wrong_cancel.error() == dross::MovementCommandRejection::wrong_entity);
  CHECK(movement.state() == dross::MovementLifecycleState::traversing);
  REQUIRE(movement.handle(dross::movement::CancelMovement{.entity = actor()}));
}

TEST_CASE("combat movement previews against AP and spends only at committed boundaries") {
  auto map = line_map();
  dross::OccupancyIndex occupancy;
  dross::WeightedAStarPathPlanner planner;
  auto footprint = single_footprint();
  dross::CombatSession combat{{
      {.entity = actor(), .initiative = 10, .maximum_action_points = 3},
      {.entity = dross::EntityRef{dross::WorldInstanceId{1}, dross::EntityId{7, 2}},
       .initiative = 5,
       .maximum_action_points = 3},
  }};
  REQUIRE(combat.start());
  REQUIRE(occupancy.place(actor().id(), {cell(0, 0)}));
  dross::MovementRuntime movement{
      map,     occupancy, planner, footprint, actor(), pose(0, 0), {.ticks_per_transition = 2},
      nullptr, &combat};

  const auto too_far = movement.preview(pose(2, 0));
  CHECK_FALSE(too_far.accepted);
  CHECK(too_far.rejection == dross::PathError::insufficient_budget);
  CHECK(combat.action_points(actor().id()) == 3);

  REQUIRE(movement.move_to(pose(1, 0)));
  CHECK(movement.advance(dross::Tick{0}) == dross::MovementAdvance::in_progress);
  CHECK(combat.action_points(actor().id()) == 3);
  CHECK(movement.advance(dross::Tick{1}) == dross::MovementAdvance::completed);
  CHECK(combat.action_points(actor().id()) == 1);
  CHECK(movement.pose() == pose(1, 0));
}

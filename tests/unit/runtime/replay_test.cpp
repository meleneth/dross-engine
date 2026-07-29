#include <dross/random/random_hub.hpp>
#include <dross/runtime/replay.hpp>
#include <dross/world/world_storage.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>

namespace {

dross::ContentId content_id(const char* value) { return dross::ContentId::parse(value).value(); }

dross::CanonicalCheckpoint checkpoint_for(const std::vector<std::uint64_t>& spawn_order,
                                          const std::uint64_t seed) {
  dross::WorldStorage world{
      dross::WorldConfig{.lineage = 7, .instance_id = dross::WorldInstanceId{9}}};
  for (const auto sequence : spawn_order) {
    REQUIRE(world.write().spawn(dross::SpawnPlan::authored(sequence)));
  }
  dross::OccupancyIndex occupancy;
  dross::RandomHub random{dross::MasterSeed{seed}};
  static_cast<void>(random.stream(dross::RandomStreamId{content_id("dross:combat")}).next_u64());
  dross::NullMachineTrace trace;
  dross::WorldLifecycle lifecycle{trace};
  dross::SimulationMode mode{trace};
  REQUIRE(lifecycle.restore(
      dross::WorldLifecycleSnapshot{.state = dross::WorldLifecycleState::running}));
  return dross::canonical_checkpoint(dross::Tick{3}, world, occupancy, random.snapshot(),
                                     lifecycle.snapshot(), mode.snapshot(), {});
}

} // namespace

TEST_CASE("canonical checkpoint is independent from storage insertion order") {
  CHECK(checkpoint_for({2, 1}, 12345) == checkpoint_for({1, 2}, 12345));
}

TEST_CASE("changed master seed changes the random checkpoint section") {
  const auto first = checkpoint_for({1, 2}, 12345);
  const auto second = checkpoint_for({1, 2}, 54321);

  CHECK(first.overall != second.overall);
  CHECK(first.sections.at(dross::CheckpointSection::random) !=
        second.sections.at(dross::CheckpointSection::random));
}

TEST_CASE("machine state changes the canonical machine section") {
  dross::WorldStorage world{
      dross::WorldConfig{.lineage = 7, .instance_id = dross::WorldInstanceId{9}}};
  dross::OccupancyIndex occupancy;
  dross::RandomHub random{dross::MasterSeed{12345}};
  dross::NullMachineTrace trace;
  dross::WorldLifecycle lifecycle{trace};
  dross::SimulationMode mode{trace};
  REQUIRE(lifecycle.restore(
      dross::WorldLifecycleSnapshot{.state = dross::WorldLifecycleState::running}));
  const auto exploration =
      dross::canonical_checkpoint(dross::Tick{0}, world, occupancy, random.snapshot(),
                                  lifecycle.snapshot(), mode.snapshot(), {});
  REQUIRE(mode.request_combat());
  const auto pending =
      dross::canonical_checkpoint(dross::Tick{0}, world, occupancy, random.snapshot(),
                                  lifecycle.snapshot(), mode.snapshot(), {});

  CHECK(exploration.sections.at(dross::CheckpointSection::machines) !=
        pending.sections.at(dross::CheckpointSection::machines));
}

TEST_CASE("movement state changes the localized capability checkpoint") {
  dross::WorldStorage world{
      dross::WorldConfig{.lineage = 7, .instance_id = dross::WorldInstanceId{9}}};
  dross::OccupancyIndex occupancy;
  dross::RandomHub random{dross::MasterSeed{12345}};
  dross::NullMachineTrace trace;
  dross::WorldLifecycle lifecycle{trace};
  dross::SimulationMode mode{trace};
  REQUIRE(lifecycle.restore(
      dross::WorldLifecycleSnapshot{.state = dross::WorldLifecycleState::running}));
  const auto movement = dross::MovementSnapshot{
      .state = dross::MovementLifecycleState::idle,
      .pose =
          dross::HexPose{
              .anchor =
                  dross::HexCellId{
                      .region = dross::RegionId{content_id("demo:room")},
                      .coord = {.q = 0, .r = 0},
                      .layer = 0,
                  },
              .facing = dross::HexFacing::east,
          },
      .path = {},
      .next_pose = 0,
      .transition_ticks = 0,
      .expected_occupancy_revision = 1,
      .cancel_requested = false,
      .combat_stop_requested = false,
  };
  const auto first = dross::canonical_checkpoint(
      dross::Tick{0}, world, occupancy, random.snapshot(), lifecycle.snapshot(), mode.snapshot(),
      {}, {.movement = movement, .combat = {}, .combat_actors = {}, .door = {}, .script = {}});
  auto changed = movement;
  changed.cancel_requested = true;
  const auto second = dross::canonical_checkpoint(
      dross::Tick{0}, world, occupancy, random.snapshot(), lifecycle.snapshot(), mode.snapshot(),
      {}, {.movement = changed, .combat = {}, .combat_actors = {}, .door = {}, .script = {}});

  const std::array first_values{first};
  const std::array second_values{second};
  const auto divergence = dross::first_divergence(first_values, second_values);
  REQUIRE(divergence);
  CHECK(divergence->section == dross::CheckpointSection::capabilities);
  CHECK(divergence->detail == "movement");
}

TEST_CASE("replay divergence localizes an unexpected capability section") {
  dross::WorldStorage world{
      dross::WorldConfig{.lineage = 7, .instance_id = dross::WorldInstanceId{9}}};
  dross::OccupancyIndex occupancy;
  dross::RandomHub random{dross::MasterSeed{12345}};
  dross::NullMachineTrace trace;
  dross::WorldLifecycle lifecycle{trace};
  dross::SimulationMode mode{trace};
  REQUIRE(lifecycle.restore(
      dross::WorldLifecycleSnapshot{.state = dross::WorldLifecycleState::running}));
  const auto expected =
      dross::canonical_checkpoint(dross::Tick{0}, world, occupancy, random.snapshot(),
                                  lifecycle.snapshot(), mode.snapshot(), {});
  const auto movement = dross::MovementSnapshot{
      .state = dross::MovementLifecycleState::idle,
      .pose =
          dross::HexPose{
              .anchor =
                  dross::HexCellId{
                      .region = dross::RegionId{content_id("demo:room")},
                      .coord = {.q = 0, .r = 0},
                      .layer = 0,
                  },
              .facing = dross::HexFacing::east,
          },
      .path = {},
      .next_pose = 0,
      .transition_ticks = 0,
      .expected_occupancy_revision = 1,
      .cancel_requested = false,
      .combat_stop_requested = false,
  };
  const auto actual = dross::canonical_checkpoint(
      dross::Tick{0}, world, occupancy, random.snapshot(), lifecycle.snapshot(), mode.snapshot(),
      {}, {.movement = movement, .combat = {}, .combat_actors = {}, .door = {}, .script = {}});

  const std::array expected_values{expected};
  const std::array actual_values{actual};
  const auto divergence = dross::first_divergence(expected_values, actual_values);

  REQUIRE(divergence);
  CHECK(divergence->section == dross::CheckpointSection::capabilities);
  CHECK(divergence->detail == "movement");
}

TEST_CASE("replay DTO has a deterministic round trip") {
  dross::ReplayLog log{
      .header =
          dross::ReplayHeader{
              .engine_version = dross::SemanticVersion{.major = 0, .minor = 1, .patch = 0},
              .schema_version = 1,
              .scenario = content_id("dross:command_event_kernel"),
              .content_manifest = dross::first_slice_content_manifest(),
              .master_seed = dross::MasterSeed{12345},
              .random_algorithm_version = dross::random_algorithm_version,
          },
      .external_commands = {},
      .machine_trace = {},
      .checkpoints = {checkpoint_for({1, 2}, 12345)},
  };

  const auto encoded = dross::encode_replay(log);
  const auto decoded = dross::decode_replay(encoded);
  REQUIRE(decoded);
  CHECK(*decoded == log);
  CHECK(dross::encode_replay(*decoded) == encoded);
  auto changed_content = decoded->header.content_manifest;
  changed_content.back().content_hash.front() ^= 0xFFU;
  const auto compatibility =
      dross::validate_content_manifest(changed_content, dross::first_slice_content_manifest());
  REQUIRE_FALSE(compatibility);
  CHECK(compatibility.error() == dross::ContentManifestError::content_hash_mismatch);
}

TEST_CASE("first replay divergence reports its tick and canonical section") {
  auto expected = checkpoint_for({1, 2}, 12345);
  auto actual = expected;
  actual.sections[dross::CheckpointSection::occupancy][0] ^= std::uint8_t{1};
  actual.overall[0] ^= std::uint8_t{1};

  const std::array expected_values{expected};
  const std::array actual_values{actual};
  const auto divergence = dross::first_divergence(expected_values, actual_values);
  REQUIRE(divergence);
  CHECK(divergence->tick == dross::Tick{3});
  CHECK(divergence->section == dross::CheckpointSection::occupancy);
  CHECK_FALSE(divergence->detail);
}

TEST_CASE("replay divergence localizes the first differing random stream") {
  const auto expected = checkpoint_for({1, 2}, 12345);
  const auto actual = checkpoint_for({1, 2}, 54321);

  const std::array expected_values{expected};
  const std::array actual_values{actual};
  const auto divergence = dross::first_divergence(expected_values, actual_values);

  REQUIRE(divergence);
  CHECK(divergence->section == dross::CheckpointSection::random);
  REQUIRE(divergence->detail);
  CHECK(*divergence->detail == "stream/dross:combat");
}

TEST_CASE("replay divergence localizes the first differing combat actor") {
  dross::WorldStorage world{
      dross::WorldConfig{.lineage = 7, .instance_id = dross::WorldInstanceId{9}}};
  dross::OccupancyIndex occupancy;
  dross::RandomHub random{dross::MasterSeed{12345}};
  dross::NullMachineTrace trace;
  dross::WorldLifecycle lifecycle{trace};
  dross::SimulationMode mode{trace};
  REQUIRE(lifecycle.restore(
      dross::WorldLifecycleSnapshot{.state = dross::WorldLifecycleState::running}));

  const auto actor_pose = dross::HexPose{
      .anchor =
          dross::HexCellId{
              .region = dross::RegionId{content_id("demo:thump_room")},
              .coord = {.q = 1, .r = 0},
              .layer = 0,
          },
      .facing = dross::HexFacing::east,
  };
  const auto expected_actors = dross::AbilityResolverSnapshot{
      .actors =
          {
              {.entity = dross::EntityId{7, 1}, .pose = actor_pose, .health = dross::HitPoints{8}},
              {.entity = dross::EntityId{7, 2}, .pose = actor_pose, .health = dross::HitPoints{3}},
          },
  };
  auto actual_actors = expected_actors;
  actual_actors.actors.back().health = dross::HitPoints{1};

  const auto expected = dross::canonical_checkpoint(
      dross::Tick{4}, world, occupancy, random.snapshot(), lifecycle.snapshot(), mode.snapshot(),
      {},
      {.movement = {}, .combat = {}, .combat_actors = expected_actors, .door = {}, .script = {}});
  const auto actual = dross::canonical_checkpoint(
      dross::Tick{4}, world, occupancy, random.snapshot(), lifecycle.snapshot(), mode.snapshot(),
      {}, {.movement = {}, .combat = {}, .combat_actors = actual_actors, .door = {}, .script = {}});

  const std::array expected_values{expected};
  const std::array actual_values{actual};
  const auto divergence = dross::first_divergence(expected_values, actual_values);

  REQUIRE(divergence);
  CHECK(divergence->section == dross::CheckpointSection::capabilities);
  REQUIRE(divergence->detail);
  CHECK(*divergence->detail == "combat/actors/7/2");
}

TEST_CASE("replay divergence localizes the first differing script state key") {
  dross::WorldStorage world{
      dross::WorldConfig{.lineage = 7, .instance_id = dross::WorldInstanceId{9}}};
  dross::OccupancyIndex occupancy;
  dross::RandomHub random{dross::MasterSeed{12345}};
  dross::NullMachineTrace trace;
  dross::WorldLifecycle lifecycle{trace};
  dross::SimulationMode mode{trace};
  REQUIRE(lifecycle.restore(
      dross::WorldLifecycleSnapshot{.state = dross::WorldLifecycleState::running}));

  const auto address = dross::ScriptStateAddress{
      .module_id = content_id("demo:field_mouse"),
      .scope = dross::ScriptScope::for_entity(content_id("demo:thump_room"), dross::EntityId{7, 2}),
      .key = dross::ScriptStateKey::parse("attacked_count").value(),
  };
  dross::ScriptStateBag expected_state;
  expected_state.apply({dross::ScriptStateWrite{.address = address, .value = std::int64_t{1}}});
  dross::ScriptStateBag actual_state;
  actual_state.apply({dross::ScriptStateWrite{.address = address, .value = std::int64_t{2}}});

  const auto expected = dross::canonical_checkpoint(
      dross::Tick{4}, world, occupancy, random.snapshot(), lifecycle.snapshot(), mode.snapshot(),
      {},
      {.movement = {}, .combat = {}, .combat_actors = {}, .door = {}, .script = expected_state});
  const auto actual = dross::canonical_checkpoint(
      dross::Tick{4}, world, occupancy, random.snapshot(), lifecycle.snapshot(), mode.snapshot(),
      {}, {.movement = {}, .combat = {}, .combat_actors = {}, .door = {}, .script = actual_state});

  const std::array expected_values{expected};
  const std::array actual_values{actual};
  const auto divergence = dross::first_divergence(expected_values, actual_values);

  REQUIRE(divergence);
  CHECK(divergence->section == dross::CheckpointSection::capabilities);
  REQUIRE(divergence->detail);
  CHECK(*divergence->detail ==
        "script/state/demo:field_mouse/entity/demo:thump_room/7/2/attacked_count");
}

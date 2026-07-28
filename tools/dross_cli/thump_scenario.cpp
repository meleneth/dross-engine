#include "thump_scenario.hpp"

#include <dross/foundation/version.hpp>
#include <dross/random/random_hub.hpp>
#include <dross/runtime/combat_runtime.hpp>
#include <dross/runtime/machine_trace.hpp>
#include <dross/runtime/simulation_mode.hpp>
#include <dross/runtime/world_lifecycle.hpp>
#include <dross/world/world_storage.hpp>

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

constexpr int scenario_error = 5;

dross::ContentId content_id(const char* value) { return dross::ContentId::parse(value).value(); }

dross::HexPose pose(const std::int32_t q) {
  return {
      .anchor =
          {
              .region = dross::RegionId{content_id("demo:thump_room")},
              .coord = {.q = q, .r = 0},
              .layer = 0,
          },
      .facing = dross::HexFacing::east,
  };
}

struct ScenarioResult {
  dross::ReplayLog replay;
  dross::HitPoints mouse_health;
  bool killed;
};

ScenarioResult execute(const std::uint64_t seed) {
  dross::WorldStorage world{
      dross::WorldConfig{.lineage = 52, .instance_id = dross::WorldInstanceId{12}}};
  const auto player = world.write().spawn(dross::SpawnPlan::authored(1)).value();
  const auto mouse = world.write().spawn(dross::SpawnPlan::authored(2)).value();
  world.write().commit_pose(player, pose(0));
  world.write().commit_pose(mouse, pose(1));

  dross::NullMachineTrace trace;
  dross::WorldLifecycle lifecycle{trace};
  dross::SimulationMode mode{trace};
  if (!lifecycle.begin_load() || !lifecycle.load_succeeded() || !lifecycle.begin_run() ||
      !mode.request_combat() || !mode.reach_safe_boundary()) {
    throw std::logic_error{"Thump scenario lifecycle setup failed"};
  }
  dross::CombatSession combat{{
      {.entity = player, .initiative = 10, .maximum_action_points = 3},
      {.entity = mouse, .initiative = 5, .maximum_action_points = 2},
  }};
  if (!combat.start()) {
    throw std::logic_error{"Thump scenario combat start failed"};
  }
  dross::RandomHub random{dross::MasterSeed{seed}};
  auto& damage_random = random.stream(dross::RandomStreamId{content_id("dross:combat_damage")});
  dross::AbilityResolver resolver{
      combat,
      {
          {.entity = player, .pose = pose(0), .health = dross::HitPoints{8}},
          {.entity = mouse, .pose = pose(1), .health = dross::HitPoints{3}},
      },
      nullptr,
      &damage_random,
  };
  const dross::AbilityDefinition thump{
      .id = content_id("dross_demo:thump"),
      .range = 1,
      .action_point_cost = 2,
      .damage = dross::HitPoints{3},
      .bonus_damage_max = 1,
      .presentation_cue = content_id("dross_demo:thump"),
  };
  dross::OccupancyIndex occupancy;
  std::vector<dross::CanonicalCheckpoint> checkpoints;
  checkpoints.push_back(dross::canonical_checkpoint(
      dross::Tick{0}, world, occupancy, random.snapshot(), lifecycle.snapshot(), mode.snapshot(),
      std::span<const dross::PlaceEntityEnvelope>{}));
  const auto result = resolver.perform(thump, player.id(), mouse.id());
  if (!result.accepted || !result.killed ||
      combat.state() != dross::CombatSessionState::completed) {
    throw std::logic_error{"Thump scenario ability resolution failed"};
  }
  checkpoints.push_back(dross::canonical_checkpoint(
      dross::Tick{1}, world, occupancy, random.snapshot(), lifecycle.snapshot(), mode.snapshot(),
      std::span<const dross::PlaceEntityEnvelope>{}));
  return {
      .replay =
          dross::ReplayLog{
              .header =
                  {
                      .engine_version = dross::engine_version(),
                      .schema_version = 1,
                      .scenario = content_id("dross:thump_on_field_mouse"),
                      .base_package = content_id("dross:base"),
                      .master_seed = dross::MasterSeed{seed},
                      .random_algorithm_version = dross::random_algorithm_version,
                  },
              .external_commands = {},
              .machine_trace = {},
              .checkpoints = std::move(checkpoints),
          },
      .mouse_health = resolver.health(mouse.id()),
      .killed = result.killed,
  };
}

bool write_replay(const std::string& path, const dross::ReplayLog& replay) {
  const auto bytes = dross::encode_replay(replay);
  std::ofstream output{path, std::ios::binary};
  output.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  return output.good();
}

} // namespace

int run_thump_scenario(const std::uint64_t seed, const std::string& record_path) {
  try {
    const auto result = execute(seed);
    if (!record_path.empty() && !write_replay(record_path, result.replay)) {
      std::cerr << "failed to write Thump replay\n";
      return scenario_error;
    }
    std::cout << "thump-on-field-mouse seed=" << seed
              << " mouse_health=" << result.mouse_health.value()
              << " killed=" << (result.killed ? "yes" : "no")
              << " checkpoints=" << result.replay.checkpoints.size() << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "Thump scenario failed: " << error.what() << '\n';
    return scenario_error;
  }
}

int verify_thump_replay(const dross::ReplayLog& recorded) {
  try {
    const auto replayed = execute(recorded.header.master_seed.value());
    const auto divergence =
        dross::first_divergence(recorded.checkpoints, replayed.replay.checkpoints);
    if (divergence) {
      std::cerr << "replay divergence tick=" << divergence->tick.value()
                << " section=" << static_cast<unsigned int>(divergence->section);
      if (divergence->detail) {
        std::cerr << " detail=" << *divergence->detail;
      }
      std::cerr << '\n';
      return scenario_error;
    }
    std::cout << "replay verified checkpoints=" << recorded.checkpoints.size() << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "Thump replay failed: " << error.what() << '\n';
    return scenario_error;
  }
}

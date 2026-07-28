#include "exploration_movement_scenario.hpp"

#include <dross/foundation/version.hpp>
#include <dross/random/random_hub.hpp>
#include <dross/runtime/movement_runtime.hpp>
#include <dross/runtime/simulation_mode.hpp>
#include <dross/runtime/world_lifecycle.hpp>
#include <dross/world/world_storage.hpp>

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int scenario_error = 5;
constexpr std::uint64_t scenario_lineage = 41;
constexpr std::uint64_t scenario_instance = 11;

dross::ContentId content_id(const char* value) { return dross::ContentId::parse(value).value(); }

dross::HexCellId cell(const std::int32_t column) {
  return {
      .region = dross::RegionId{content_id("demo:movement_room")},
      .coord = {.q = column, .r = 0},
      .layer = 0,
  };
}

dross::HexPose pose(const std::int32_t column) {
  return {.anchor = cell(column), .facing = dross::HexFacing::east};
}

dross::CompiledHexMap make_map() {
  dross::CompiledHexMapBuilder builder;
  for (std::int32_t column = 0; column < 4; ++column) {
    if (!builder.add_cell(dross::CellFacts{
            .id = cell(column),
            .surface_height = dross::Millimeters{0},
            .terrain = content_id("dross:floor"),
            .base_cost = dross::MovementCost{1},
            .clearance = dross::Clearance::open,
            .traversable = true,
            .semantic_tags = {},
        })) {
      throw std::logic_error{"movement scenario cell construction failed"};
    }
    if (column > 0 &&
        !builder.add_edge(
            cell(column - 1), cell(column),
            dross::DirectionalEdgeFacts{.traversable = true, .cost = dross::MovementCost{1}},
            dross::DirectionalEdgeFacts{.traversable = true, .cost = dross::MovementCost{1}})) {
      throw std::logic_error{"movement scenario edge construction failed"};
    }
  }
  return std::move(builder).build().value();
}

class ScenarioEvents final : public dross::MovementEventSink {
public:
  explicit ScenarioEvents(dross::WorldStorage& world) : world_{&world} {}

  void publish(const dross::movement::MovementStarted&) override { ++started_; }
  void publish(const dross::movement::ActorEnteredCell& event) override {
    world_->write().commit_pose(event.entity, event.pose);
    ++entered_;
  }
  void publish(const dross::movement::MovementCompleted&) override { ++completed_; }

  [[nodiscard]] std::size_t started() const noexcept { return started_; }
  [[nodiscard]] std::size_t entered() const noexcept { return entered_; }
  [[nodiscard]] std::size_t completed() const noexcept { return completed_; }

private:
  dross::WorldStorage* world_;
  std::size_t started_{0};
  std::size_t entered_{0};
  std::size_t completed_{0};
};

struct ScenarioResult {
  dross::ReplayLog replay;
  dross::HexPose final_pose;
  std::size_t entered_events;
};

ScenarioResult execute(const std::uint64_t seed) {
  dross::WorldStorage world{dross::WorldConfig{
      .lineage = scenario_lineage, .instance_id = dross::WorldInstanceId{scenario_instance}}};
  const auto actor = world.write().spawn(dross::SpawnPlan::runtime()).value();
  world.write().commit_pose(actor, pose(0));

  auto map = make_map();
  dross::OccupancyIndex occupancy;
  if (!occupancy.place(actor.id(), {cell(0)})) {
    throw std::logic_error{"movement scenario occupancy setup failed"};
  }
  auto footprint = dross::FootprintDefinition::create(dross::FootprintId{content_id("demo:single")},
                                                      {dross::HexCoord{.q = 0, .r = 0}})
                       .value();
  dross::WeightedAStarPathPlanner planner;
  ScenarioEvents events{world};
  dross::MovementRuntime movement{
      map, occupancy, planner, footprint, actor, pose(0), {.ticks_per_transition = 2}, &events};
  if (!movement.move_to(pose(3))) {
    throw std::logic_error{"movement scenario command rejected"};
  }

  dross::RandomHub random{dross::MasterSeed{seed}};
  dross::NullMachineTrace trace;
  dross::WorldLifecycle lifecycle{trace};
  dross::SimulationMode mode{trace};
  if (!lifecycle.begin_load() || !lifecycle.load_succeeded() || !lifecycle.begin_run()) {
    throw std::logic_error{"movement scenario lifecycle setup failed"};
  }

  std::vector<dross::CanonicalCheckpoint> checkpoints;
  for (std::uint64_t tick = 0; tick < 6; ++tick) {
    static_cast<void>(movement.advance(dross::Tick{tick}));
    checkpoints.push_back(dross::canonical_checkpoint(
        dross::Tick{tick}, world, occupancy, random.snapshot(), lifecycle.snapshot(),
        mode.snapshot(), std::span<const dross::PlaceEntityEnvelope>{}));
  }
  if (events.started() != 1 || events.entered() != 3 || events.completed() != 1) {
    throw std::logic_error{"movement scenario event sequence failed"};
  }
  return {
      .replay =
          dross::ReplayLog{
              .header =
                  dross::ReplayHeader{
                      .engine_version = dross::engine_version(),
                      .schema_version = 1,
                      .scenario = content_id("dross:exploration_movement"),
                      .content_manifest = dross::first_slice_content_manifest(),
                      .master_seed = dross::MasterSeed{seed},
                      .random_algorithm_version = dross::random_algorithm_version,
                  },
              .external_commands = {},
              .machine_trace = {},
              .checkpoints = std::move(checkpoints),
          },
      .final_pose = movement.pose(),
      .entered_events = events.entered(),
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

int run_exploration_movement_scenario(const std::uint64_t seed, const std::string& record_path) {
  try {
    const auto result = execute(seed);
    if (!record_path.empty() && !write_replay(record_path, result.replay)) {
      std::cerr << "failed to write movement replay\n";
      return scenario_error;
    }
    std::cout << "exploration-movement seed=" << seed
              << " final=" << result.final_pose.anchor.coord.q
              << " entered=" << result.entered_events
              << " checkpoints=" << result.replay.checkpoints.size() << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "exploration movement scenario failed: " << error.what() << '\n';
    return scenario_error;
  }
}

int verify_exploration_movement_replay(const dross::ReplayLog& recorded) {
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
    std::cerr << "movement replay failed: " << error.what() << '\n';
    return scenario_error;
  }
}

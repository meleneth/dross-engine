#include "command_event_kernel_scenario.hpp"
#include "exploration_movement_scenario.hpp"
#include "lifecycle_machine_scenario.hpp"
#include "thump_scenario.hpp"

#include <dross/foundation/version.hpp>
#include <dross/random/random_hub.hpp>
#include <dross/runtime/fixed_tick_runtime.hpp>
#include <dross/runtime/replay.hpp>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int scenario_error = 5;
constexpr std::uint64_t scenario_lineage = 31;
constexpr std::uint64_t scenario_instance = 8;
constexpr std::uint64_t scenario_correlation = 77;
constexpr std::uint64_t initial_causation = 101;
constexpr std::uint64_t rejected_causation = 103;

dross::ContentId content_id(const char* value) { return dross::ContentId::parse(value).value(); }

dross::HexCellId cell(const int column) {
  return dross::HexCellId{
      .region = dross::RegionId{content_id("demo:arena")},
      .coord = dross::HexCoord{.q = column, .r = 0},
      .layer = 0,
  };
}

dross::CompiledHexMap make_map() {
  dross::CompiledHexMapBuilder builder;
  for (int column = 0; column < 3; ++column) {
    if (!builder.add_cell(dross::CellFacts{
            .id = cell(column),
            .surface_height = dross::Millimeters{0},
            .terrain = content_id("demo:floor"),
            .base_cost = dross::MovementCost{1},
            .clearance = dross::Clearance::open,
            .traversable = true,
            .semantic_tags = {},
        })) {
      throw std::logic_error{"command-event scenario map construction failed"};
    }
  }
  return std::move(builder).build().value();
}

dross::PlaceEntityEnvelope command(const std::uint64_t command_id, const dross::Tick tick,
                                   const dross::EntityRef entity, const int column,
                                   const std::uint64_t causation,
                                   const dross::CommandSource source) {
  return dross::PlaceEntityEnvelope{
      .metadata =
          dross::CommandMetadata{
              .id = dross::CommandId{command_id},
              .tick = tick,
              .source = source,
              .causation = dross::CausationId{causation},
              .correlation = dross::CorrelationId{scenario_correlation},
          },
      .payload =
          dross::placement::PlaceEntity{
              .entity = entity,
              .target =
                  dross::HexPose{
                      .anchor = cell(column),
                      .facing = dross::HexFacing::east,
                  },
          },
  };
}

struct ScenarioResult {
  dross::ReplayLog replay;
  bool random_rejected{false};
  std::size_t occupancy{0};
};

ScenarioResult execute_scenario(const std::uint64_t seed,
                                const std::vector<dross::PlaceEntityEnvelope>* replay_commands) {
  dross::WorldStorage world{dross::WorldConfig{
      .lineage = scenario_lineage, .instance_id = dross::WorldInstanceId{scenario_instance}}};
  const auto first = world.write().spawn(dross::SpawnPlan::runtime()).value();
  const auto second = world.write().spawn(dross::SpawnPlan::runtime()).value();
  const auto randomized = world.write().spawn(dross::SpawnPlan::runtime()).value();

  dross::RandomHub random{dross::MasterSeed{seed}};
  auto& script_stream = random.stream(dross::RandomStreamId{content_id("dross:world_scripts")});
  dross::HeadlessPlacementScriptPort scripts;
  scripts.random_reject_cell(cell(2), dross::RationalChance{.numerator = 1, .denominator = 2},
                             script_stream, content_id("demo:unstable_ward"));
  dross::InMemoryTraceSink trace;
  dross::CommandEventKernel kernel{world, make_map(), scripts, trace};
  dross::NullMachineTrace machine_trace;
  dross::WorldLifecycle lifecycle{machine_trace};
  if (!lifecycle.begin_load() || !lifecycle.load_succeeded() || !lifecycle.begin_run()) {
    throw std::logic_error{"command-event lifecycle setup failed"};
  }
  dross::SimulationMode mode{machine_trace};
  auto follow_up = kernel.events().subscribe_capability(
      [&first, &second](const dross::placement::EntityPlaced& event,
                        dross::EventReactionContext& context) {
        if (event.entity == first) {
          context.enqueue_follow_up(command(2, dross::Tick{0}, second, 1, initial_causation,
                                            dross::CommandSource::authoritative_system));
        }
      });
  if (!follow_up) {
    throw std::logic_error{"command-event listener setup failed"};
  }

  std::vector<dross::PlaceEntityEnvelope> external{
      command(1, dross::Tick{0}, first, 0, initial_causation, dross::CommandSource::headless_test),
      command(3, dross::Tick{1}, randomized, 2, rejected_causation,
              dross::CommandSource::headless_test),
  };
  if (replay_commands != nullptr) {
    external = *replay_commands;
  }

  dross::EngineRuntime runtime{kernel, lifecycle, mode, dross::RuntimeConfig{}};
  for (const auto& value : external) {
    if (!runtime.schedule_external(value)) {
      throw std::logic_error{"command-event schedule failed"};
    }
  }

  std::vector<dross::CanonicalCheckpoint> checkpoints;
  runtime.set_checkpoint_callback([&](const dross::Tick tick) {
    const auto pending = runtime.pending_external_commands();
    checkpoints.push_back(dross::canonical_checkpoint(tick, world, kernel.occupancy(),
                                                      random.snapshot(), lifecycle.snapshot(),
                                                      mode.snapshot(), pending));
  });
  static_cast<void>(runtime.advance_tick());
  static_cast<void>(runtime.advance_tick());

  const bool random_rejected =
      trace.commands().back().result.rejection == dross::CommandRejection::script_rejected;
  return ScenarioResult{
      .replay =
          dross::ReplayLog{
              .header =
                  dross::ReplayHeader{
                      .engine_version = dross::engine_version(),
                      .schema_version = 1,
                      .scenario = content_id("dross:command_event_kernel"),
                      .content_manifest = dross::first_slice_content_manifest(),
                      .master_seed = dross::MasterSeed{seed},
                      .random_algorithm_version = dross::random_algorithm_version,
                  },
              .external_commands = replay_commands == nullptr ? external : *replay_commands,
              .machine_trace = {},
              .canonical_events = {},
              .checkpoints = std::move(checkpoints),
          },
      .random_rejected = random_rejected,
      .occupancy = kernel.occupancy().entries().size(),
  };
}

bool write_replay(const std::string& path, const dross::ReplayLog& replay) {
  const auto bytes = dross::encode_replay(replay);
  std::ofstream output{path, std::ios::binary};
  output.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  return output.good();
}

std::optional<dross::ReplayLog> read_replay(const std::string& path) {
  std::ifstream input{path, std::ios::binary};
  const std::vector<char> characters{std::istreambuf_iterator<char>{input},
                                     std::istreambuf_iterator<char>{}};
  std::vector<std::byte> bytes;
  bytes.reserve(characters.size());
  for (const auto value : characters) {
    bytes.push_back(static_cast<std::byte>(value));
  }
  const auto replay = dross::decode_replay(bytes);
  if (!replay) {
    return std::nullopt;
  }
  return *replay;
}

} // namespace

int run_command_event_kernel_scenario(const std::uint64_t seed, const std::string& record_path) {
  try {
    const auto result = execute_scenario(seed, nullptr);
    if (!record_path.empty() && !write_replay(record_path, result.replay)) {
      std::cerr << "failed to write replay\n";
      return scenario_error;
    }
    std::cout << "command-event-kernel seed=" << seed
              << " randomized=" << (result.random_rejected ? "rejected" : "accepted")
              << " occupancy=" << result.occupancy
              << " checkpoints=" << result.replay.checkpoints.size() << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "command-event kernel scenario failed: " << error.what() << '\n';
    return scenario_error;
  }
}

int run_replay_verification(const std::string& path) {
  const auto recorded = read_replay(path);
  if (!recorded) {
    std::cerr << "invalid replay\n";
    return scenario_error;
  }
  const auto manifest = dross::validate_content_manifest(recorded->header.content_manifest,
                                                         dross::first_slice_content_manifest());
  if (!manifest) {
    std::cerr << "replay content manifest mismatch=" << static_cast<unsigned int>(manifest.error())
              << '\n';
    return scenario_error;
  }
  if (recorded->header.scenario == content_id("dross:lifecycle_machines")) {
    return verify_lifecycle_replay(*recorded);
  }
  if (recorded->header.scenario == content_id("dross:exploration_movement")) {
    return verify_exploration_movement_replay(*recorded);
  }
  if (recorded->header.scenario == content_id("dross:thump_on_field_mouse")) {
    return verify_thump_replay(*recorded);
  }
  try {
    const auto replayed =
        execute_scenario(recorded->header.master_seed.value(), &recorded->external_commands);
    const auto divergence =
        dross::first_divergence(recorded->checkpoints, replayed.replay.checkpoints);
    if (recorded->canonical_events != replayed.replay.canonical_events) {
      std::cerr << "replay event trace divergence\n";
      return scenario_error;
    }
    if (divergence) {
      std::cerr << "replay divergence tick=" << divergence->tick.value()
                << " section=" << static_cast<unsigned int>(divergence->section);
      if (divergence->detail) {
        std::cerr << " detail=" << *divergence->detail;
      }
      std::cerr << '\n';
      return scenario_error;
    }
    std::cout << "replay verified checkpoints=" << recorded->checkpoints.size() << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "replay failed: " << error.what() << '\n';
    return scenario_error;
  }
}

int compare_runs(const std::string& expected_path, const std::string& actual_path) {
  const auto expected = read_replay(expected_path);
  const auto actual = read_replay(actual_path);
  if (!expected || !actual) {
    std::cerr << "invalid run trace\n";
    return scenario_error;
  }
  if (expected->header != actual->header) {
    std::cerr << "run divergence header\n";
    return scenario_error;
  }
  if (expected->external_commands != actual->external_commands) {
    std::cerr << "run divergence external_commands\n";
    return scenario_error;
  }
  if (expected->machine_trace != actual->machine_trace) {
    std::cerr << "run divergence machine_trace\n";
    return scenario_error;
  }
  if (expected->canonical_events != actual->canonical_events) {
    std::cerr << "run divergence canonical_events\n";
    return scenario_error;
  }
  const auto divergence = dross::first_divergence(expected->checkpoints, actual->checkpoints);
  if (divergence) {
    std::cerr << "run divergence tick=" << divergence->tick.value()
              << " section=" << static_cast<unsigned int>(divergence->section);
    if (divergence->detail) {
      std::cerr << " detail=" << *divergence->detail;
    }
    std::cerr << '\n';
    return scenario_error;
  }
  std::cout << "runs match checkpoints=" << expected->checkpoints.size()
            << " events=" << expected->canonical_events.size() << '\n';
  return 0;
}

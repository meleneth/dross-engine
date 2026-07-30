#include "persistence_scenario.hpp"

#include <dross/foundation/version.hpp>
#include <dross/persistence/save_container.hpp>
#include <dross/runtime/fixed_tick_runtime.hpp>

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int scenario_error = 5;
constexpr std::uint64_t scenario_lineage = 57;
constexpr std::uint64_t initial_instance = 14;
constexpr std::uint64_t loaded_instance = 15;
constexpr std::uint64_t causation_offset = 100;
constexpr std::uint64_t scenario_correlation = 900;
constexpr std::uint8_t low_nibble_mask = 0x0FU;
constexpr std::uint32_t scenario_ticks_per_second = 30;
constexpr std::size_t scenario_command_cycle_budget = 64;

dross::ContentId content_id(const char* value) { return dross::ContentId::parse(value).value(); }

dross::HexCellId cell(const int column) {
  return dross::HexCellId{
      .region = dross::RegionId{content_id("demo:persistence_arena")},
      .coord = dross::HexCoord{.q = column, .r = 0},
      .layer = 0,
  };
}

dross::CompiledHexMap make_map() {
  dross::CompiledHexMapBuilder builder;
  for (int column = 0; column < 3; ++column) {
    const auto added = builder.add_cell(dross::CellFacts{
        .id = cell(column),
        .surface_height = dross::Millimeters{0},
        .terrain = content_id("demo:floor"),
        .base_cost = dross::MovementCost{1},
        .clearance = dross::Clearance::open,
        .traversable = true,
        .semantic_tags = {},
    });
    if (!added) {
      throw std::logic_error{"persistence map construction failed"};
    }
  }
  return std::move(builder).build().value();
}

dross::PlaceEntityEnvelope command(const std::uint64_t command_id, const dross::Tick tick,
                                   const dross::EntityRef entity, const int column) {
  return dross::PlaceEntityEnvelope{
      .metadata =
          dross::CommandMetadata{
              .id = dross::CommandId{command_id},
              .tick = tick,
              .source = dross::CommandSource::headless_test,
              .causation = dross::CausationId{command_id + causation_offset},
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

std::vector<std::byte> read_bytes(const std::string& path) {
  std::ifstream input{path, std::ios::binary};
  const std::vector<char> characters{std::istreambuf_iterator<char>{input},
                                     std::istreambuf_iterator<char>{}};
  std::vector<std::byte> bytes;
  bytes.reserve(characters.size());
  for (const auto value : characters) {
    bytes.push_back(static_cast<std::byte>(value));
  }
  return bytes;
}

bool write_bytes(const std::string& path, const std::span<const std::byte> bytes) {
  std::ofstream output{path, std::ios::binary};
  output.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  return output.good();
}

std::string hash_text(const dross::CheckpointHash& hash) {
  constexpr std::string_view hexadecimal{"0123456789abcdef"};
  std::string result;
  result.reserve(hash.size() * 2U);
  for (const auto value : hash) {
    result.push_back(hexadecimal[value >> 4U]);
    result.push_back(hexadecimal[value & low_nibble_mask]);
  }
  return result;
}

struct ContinuationResult {
  dross::CanonicalCheckpoint checkpoint;
  std::vector<dross::EventTrace> events;
};

struct TailCommand {
  std::uint64_t command_id;
  std::uint64_t tick;
  std::uint64_t entity_sequence;
  int column;
};

ContinuationResult continue_from(dross::WorldStorage& world, dross::RandomHub& random,
                                 dross::WorldLifecycle& lifecycle, dross::SimulationMode& mode,
                                 const dross::WorldInstanceId instance, const TailCommand& tail) {
  dross::HeadlessPlacementScriptPort scripts;
  dross::InMemoryTraceSink trace;
  dross::CommandEventKernel kernel{world, make_map(), scripts, trace};
  dross::EngineRuntime runtime{
      kernel, lifecycle, mode,
      dross::RuntimeConfig{.ticks_per_second = scenario_ticks_per_second,
                           .max_command_cycles_per_tick = scenario_command_cycle_budget,
                           .initial_tick = dross::Tick{tail.tick}}};
  const auto second = world.read().find(dross::EntityId{scenario_lineage, tail.entity_sequence});
  if (!second || second->world_instance() != instance ||
      !runtime.schedule_external(
          command(tail.command_id, dross::Tick{tail.tick}, *second, tail.column))) {
    throw std::logic_error{"persistence continuation scheduling failed"};
  }
  static_cast<void>(runtime.advance_tick());
  static_cast<void>(
      random.stream(dross::RandomStreamId{content_id("dross:persistence")}).next_u64());
  return ContinuationResult{
      .checkpoint =
          dross::canonical_checkpoint(runtime.clock().current(), world, kernel.occupancy(),
                                      random.snapshot(), lifecycle.snapshot(), mode.snapshot(), {}),
      .events = trace.events(),
  };
}

bool same_events(const std::vector<dross::EventTrace>& left,
                 const std::vector<dross::EventTrace>& right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (std::size_t index = 0; index < left.size(); ++index) {
    if (left[index].source_command != right[index].source_command ||
        left[index].causation != right[index].causation ||
        left[index].correlation != right[index].correlation) {
      return false;
    }
  }
  return true;
}

dross::SaveContainer decode_file(const std::string& path) {
  const auto decoded = dross::decode_save_container(read_bytes(path));
  if (!decoded) {
    throw std::logic_error{"invalid save"};
  }
  return *decoded;
}

ContinuationResult load_and_continue(const dross::SaveContainer& save, const TailCommand& tail) {
  dross::ComponentCodecRegistry registry;
  if (!dross::register_current_component_codecs(registry)) {
    throw std::logic_error{"persistence codec registration failed"};
  }
  const auto plan = dross::build_world_load_plan(
      save, registry, content_id("demo:persistence_arena"), dross::canonical_map_hash(make_map()),
      dross::engine_content_manifest());
  if (!plan) {
    throw std::logic_error{"persistence load plan failed"};
  }
  auto world = plan->construct(dross::WorldInstanceId{loaded_instance});
  if (!world) {
    throw std::logic_error{"persistence world construction failed"};
  }
  dross::RandomHub random{save.runtime.random.master_seed};
  dross::NullMachineTrace machine_trace;
  dross::WorldLifecycle lifecycle{machine_trace};
  dross::SimulationMode mode{machine_trace};
  if (!random.restore(save.runtime.random) || !lifecycle.restore(save.runtime.lifecycle) ||
      !mode.restore(save.runtime.mode)) {
    throw std::logic_error{"persistence runtime restore failed"};
  }
  if (tail.tick != save.header.current_tick.value()) {
    throw std::logic_error{"tail command does not begin at saved tick"};
  }
  return continue_from(**world, random, lifecycle, mode, dross::WorldInstanceId{loaded_instance},
                       tail);
}

} // namespace

int run_persistence_scenario(const std::uint64_t seed, const std::string& save_path) {
  try {
    dross::WorldStorage world{dross::WorldConfig{
        .lineage = scenario_lineage,
        .instance_id = dross::WorldInstanceId{initial_instance},
    }};
    const auto first = world.write().spawn(dross::SpawnPlan::runtime()).value();
    static_cast<void>(world.write().spawn(dross::SpawnPlan::runtime()).value());
    dross::RandomHub random{dross::MasterSeed{seed}};
    static_cast<void>(
        random.stream(dross::RandomStreamId{content_id("dross:persistence")}).next_u64());
    dross::HeadlessPlacementScriptPort scripts;
    dross::InMemoryTraceSink trace;
    dross::NullMachineTrace machine_trace;
    dross::WorldLifecycle lifecycle{machine_trace};
    dross::SimulationMode mode{machine_trace};
    if (!lifecycle.begin_load() || !lifecycle.load_succeeded() || !lifecycle.begin_run()) {
      throw std::logic_error{"persistence lifecycle setup failed"};
    }
    dross::CommandEventKernel kernel{world, make_map(), scripts, trace};
    dross::EngineRuntime runtime{kernel, lifecycle, mode, dross::RuntimeConfig{}};
    if (!runtime.schedule_external(command(1, dross::Tick{0}, first, 0))) {
      throw std::logic_error{"persistence initial scheduling failed"};
    }
    static_cast<void>(runtime.advance_tick());
    if (!runtime.save_boundary()) {
      throw std::logic_error{"persistence save boundary refused"};
    }

    const auto save = dross::SaveContainer{
        .header =
            dross::SaveHeader{
                .container_version = 1,
                .simulation_schema_version = 1,
                .engine_version = dross::engine_version(),
                .ticks_per_second = runtime.clock().ticks_per_second(),
                .current_tick = runtime.clock().current(),
                .world_lineage = scenario_lineage,
                .allocator = world.allocator_snapshot(),
                .map_id = content_id("demo:persistence_arena"),
                .map_hash = dross::canonical_map_hash(make_map()),
            },
        .runtime =
            dross::SaveRuntimeSnapshot{
                .random = random.snapshot(),
                .lifecycle = lifecycle.snapshot(),
                .mode = mode.snapshot(),
            },
        .content_manifest = dross::engine_content_manifest(),
        .combat = {},
        .movement = {},
        .door = {},
        .script = {},
        .inventory = {},
        .components = dross::snapshot_world_components(world),
    };
    const auto bytes = dross::encode_save_container(save);
    if (!write_bytes(save_path, bytes)) {
      throw std::logic_error{"failed to write persistence save"};
    }

    const auto tail = TailCommand{.command_id = 2, .tick = 1, .entity_sequence = 2, .column = 1};
    const auto uninterrupted = continue_from(world, random, lifecycle, mode,
                                             dross::WorldInstanceId{initial_instance}, tail);
    const auto decoded = dross::decode_save_container(bytes);
    if (!decoded) {
      throw std::logic_error{"newly encoded persistence save did not decode"};
    }
    const auto resumed = load_and_continue(*decoded, tail);
    const bool matches = uninterrupted.checkpoint.overall == resumed.checkpoint.overall &&
                         same_events(uninterrupted.events, resumed.events);
    if (!matches) {
      throw std::logic_error{"uninterrupted and resumed state diverged"};
    }
    std::cout << "persistence-foundation seed=" << seed << " bytes=" << bytes.size()
              << " entities=" << world.read().entity_count()
              << " final=" << hash_text(uninterrupted.checkpoint.overall) << " match=yes\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "persistence scenario failed: " << error.what() << '\n';
    return scenario_error;
  }
}

int inspect_save(const std::string& save_path) {
  try {
    const auto save = decode_file(save_path);
    std::cout << "save version=" << save.header.container_version
              << " tick=" << save.header.current_tick.value()
              << " components=" << save.components.size()
              << " streams=" << save.runtime.random.streams.size()
              << " lifecycle=" << static_cast<unsigned int>(save.runtime.lifecycle.state)
              << " mode=" << static_cast<unsigned int>(save.runtime.mode.state) << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "inspect save failed: " << error.what() << '\n';
    return scenario_error;
  }
}

int resume_save(const ResumeSaveArguments& arguments) {
  try {
    std::ifstream commands{arguments.commands_path};
    std::string verb;
    std::uint64_t command_id = 0;
    std::uint64_t tick = 0;
    std::uint64_t entity_sequence = 0;
    int column = 0;
    if (!(commands >> verb >> command_id >> tick >> entity_sequence >> column) || verb != "place") {
      throw std::logic_error{"invalid persistence tail commands"};
    }
    const auto result = load_and_continue(decode_file(arguments.save_path),
                                          TailCommand{.command_id = command_id,
                                                      .tick = tick,
                                                      .entity_sequence = entity_sequence,
                                                      .column = column});
    std::cout << "resume final=" << hash_text(result.checkpoint.overall)
              << " events=" << result.events.size() << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "resume failed: " << error.what() << '\n';
    return scenario_error;
  }
}

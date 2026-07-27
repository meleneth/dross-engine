#include "lifecycle_machine_scenario.hpp"

#include <dross/foundation/version.hpp>

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace {

constexpr int scenario_error = 5;
constexpr std::uint64_t lifecycle_seed = 12345;
constexpr std::uint64_t lifecycle_lineage = 91;
constexpr std::uint64_t lifecycle_instance = 17;

dross::ContentId content_id(const char* value) { return dross::ContentId::parse(value).value(); }

dross::ReplayLog execute_lifecycle_scenario() {
  dross::WorldStorage world{dross::WorldConfig{
      .lineage = lifecycle_lineage, .instance_id = dross::WorldInstanceId{lifecycle_instance}}};
  dross::OccupancyIndex occupancy;
  dross::RandomHub random{dross::MasterSeed{lifecycle_seed}};
  dross::InMemoryMachineTrace trace;
  dross::WorldLifecycle lifecycle{trace};
  dross::SimulationMode mode{trace};
  std::vector<dross::CanonicalCheckpoint> checkpoints;
  std::uint64_t tick = 0;
  auto checkpoint = [&](const dross::WorldLifecycle& current_lifecycle,
                        const dross::SimulationMode& current_mode) {
    checkpoints.push_back(
        dross::canonical_checkpoint(dross::Tick{tick}, world, occupancy, random.snapshot(),
                                    current_lifecycle.snapshot(), current_mode.snapshot(), {}));
    ++tick;
  };

  checkpoint(lifecycle, mode);
  if (!lifecycle.begin_load()) {
    throw std::logic_error{"lifecycle begin load failed"};
  }
  checkpoint(lifecycle, mode);
  if (!lifecycle.load_succeeded() || !lifecycle.begin_run()) {
    throw std::logic_error{"lifecycle run setup failed"};
  }
  checkpoint(lifecycle, mode);
  if (!mode.request_combat()) {
    throw std::logic_error{"combat request failed"};
  }
  checkpoint(lifecycle, mode);
  if (!mode.reach_safe_boundary()) {
    throw std::logic_error{"combat safe boundary failed"};
  }
  checkpoint(lifecycle, mode);
  if (!mode.end_combat()) {
    throw std::logic_error{"combat end failed"};
  }
  checkpoint(lifecycle, mode);

  const auto lifecycle_snapshot = lifecycle.snapshot();
  const auto mode_snapshot = mode.snapshot();
  dross::WorldLifecycle restored_lifecycle{trace};
  dross::SimulationMode restored_mode{trace};
  if (!restored_lifecycle.restore(lifecycle_snapshot) || !restored_mode.restore(mode_snapshot)) {
    throw std::logic_error{"machine restore failed"};
  }
  checkpoint(restored_lifecycle, restored_mode);
  if (!restored_lifecycle.fatal_fault()) {
    throw std::logic_error{"forced lifecycle fault failed"};
  }
  checkpoint(restored_lifecycle, restored_mode);

  return dross::ReplayLog{
      .header =
          dross::ReplayHeader{
              .engine_version = dross::engine_version(),
              .schema_version = 1,
              .scenario = content_id("dross:lifecycle_machines"),
              .base_package = content_id("dross:base"),
              .master_seed = dross::MasterSeed{lifecycle_seed},
              .random_algorithm_version = dross::random_algorithm_version,
          },
      .external_commands = {},
      .machine_trace = trace.entries(),
      .checkpoints = std::move(checkpoints),
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

int run_lifecycle_machine_scenario(const std::string& record_path) {
  try {
    const auto replay = execute_lifecycle_scenario();
    if (!record_path.empty() && !write_replay(record_path, replay)) {
      std::cerr << "failed to write lifecycle replay\n";
      return scenario_error;
    }
    std::cout << "lifecycle-machines checkpoints=" << replay.checkpoints.size()
              << " trace=" << replay.machine_trace.size() << " final=faulted\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "lifecycle machine scenario failed: " << error.what() << '\n';
    return scenario_error;
  }
}

int verify_lifecycle_replay(const dross::ReplayLog& recorded) {
  try {
    const auto replayed = execute_lifecycle_scenario();
    const auto divergence = dross::first_divergence(recorded.checkpoints, replayed.checkpoints);
    if (divergence || recorded.machine_trace != replayed.machine_trace) {
      std::cerr << "lifecycle replay divergence\n";
      return scenario_error;
    }
    std::cout << "replay verified checkpoints=" << recorded.checkpoints.size()
              << " machine_trace=" << recorded.machine_trace.size() << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "lifecycle replay failed: " << error.what() << '\n';
    return scenario_error;
  }
}

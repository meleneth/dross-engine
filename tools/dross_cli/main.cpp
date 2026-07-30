#include <dross/foundation/byte_codec.hpp>
#include <dross/foundation/quantities.hpp>
#include <dross/foundation/version.hpp>
#include <dross/hex/grid_bake.hpp>
#include <dross/identity/content_id.hpp>
#include <dross/world/world_storage.hpp>

#include "command_event_kernel_scenario.hpp"
#include "exploration_movement_scenario.hpp"
#include "hex_pathing_scenario.hpp"
#include "lifecycle_machine_scenario.hpp"
#include "persistence_scenario.hpp"
#include "thump_scenario.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

namespace {

constexpr int usage_error = 2;
constexpr int validation_error = 3;
constexpr int self_check_error = 4;
constexpr int scenario_error = 5;
constexpr std::uint64_t default_scenario_seed = 12345;
constexpr int lifecycle_record_argument_count = 5;
constexpr int persistence_minimum_argument_count = 5;
constexpr int resume_argument_count = 5;

void print_usage(std::ostream& output) {
  output << "usage:\n"
            "  dross_headless version\n"
            "  dross_headless validate-id <namespace:name>\n"
            "  dross_headless scenario identity-lifecycle\n"
            "  dross_headless scenario hex-pathing\n"
            "  dross_headless scenario grid-bake\n"
            "  dross_headless scenario command-event-kernel [--seed N] [--record PATH]\n"
            "  dross_headless scenario exploration-movement [--seed N] [--record PATH]\n"
            "  dross_headless scenario thump-on-field-mouse [--seed N] [--record PATH]"
            " [--save-checkpoints DIR]\n"
            "  dross_headless scenario lifecycle-machines [--record PATH]\n"
            "  dross_headless scenario persistence-foundation [--seed N] --save PATH\n"
            "  dross_headless inspect-save PATH\n"
            "  dross_headless resume PATH --commands PATH\n"
            "  dross_headless replay --verify-checkpoints PATH\n"
            "  dross_headless compare-runs EXPECTED ACTUAL\n";
}

[[nodiscard]] bool foundation_self_check() {
  const auto content_id = dross::ContentId::parse("dross:foundation");
  if (!content_id) {
    return false;
  }

  dross::ByteWriter writer;
  writer.write(*content_id);
  dross::ByteReader reader{writer.bytes()};
  const auto decoded = reader.read_content_id();
  const auto next_tick = dross::Tick{0}.checked_add(1);
  return decoded && *decoded == *content_id && reader.remaining() == 0 && next_tick &&
         next_tick->value() == 1;
}

[[nodiscard]] std::string_view reason_text(const dross::ContentIdErrorReason reason) {
  switch (reason) {
  case dross::ContentIdErrorReason::missing_separator:
    return "missing ':' separator";
  case dross::ContentIdErrorReason::extra_separator:
    return "extra ':' separator";
  case dross::ContentIdErrorReason::empty_namespace:
    return "namespace is empty";
  case dross::ContentIdErrorReason::empty_name:
    return "name is empty";
  case dross::ContentIdErrorReason::invalid_character:
    return "invalid character";
  }
  return "unknown error";
}

[[nodiscard]] int run_identity_lifecycle() {
  constexpr std::uint64_t lineage = 17;
  constexpr std::uint64_t instance = 42;
  auto alias_id = dross::ContentId::parse("demo:named");
  if (!alias_id) {
    return scenario_error;
  }

  dross::WorldStorage world{
      dross::WorldConfig{.lineage = lineage, .instance_id = dross::WorldInstanceId{instance}}};
  auto write = world.write();
  const auto named =
      write.spawn(dross::SpawnPlan::authored(40, dross::EntityAlias{std::move(*alias_id)}));
  const auto removed = write.spawn(dross::SpawnPlan::runtime());
  const auto survivor = write.spawn(dross::SpawnPlan::runtime());
  if (!named || !removed || !survivor ||
      !world.read().find(dross::EntityAlias{dross::ContentId::parse("demo:named").value()}) ||
      !write.destroy(*removed)) {
    std::cerr << "identity lifecycle scenario failed\n";
    return scenario_error;
  }

  const auto alive = world.read().stable_entity_ids();
  const auto found_named = world.read().find(named->id());
  if (alive.size() != 2 || !found_named) {
    std::cerr << "identity lifecycle summary failed\n";
    return scenario_error;
  }

  std::cout << "identity-lifecycle world=" << instance << " alive=[" << alive[0] << ',' << alive[1]
            << "] named=" << found_named->id()
            << " next=" << world.allocator_snapshot().next_runtime_sequence << '\n';
  return 0;
}

[[nodiscard]] int run_grid_bake() {
  const auto region = dross::RegionId{dross::ContentId::parse("demo:room").value()};
  const auto identity = dross::GridIdentity{
      .region = region, .origin_x_mm = 0, .origin_y_mm = 0, .origin_z_mm = 0, .radius_mm = 1000};
  const auto first = dross::HexCellId{.region = region, .coord = {.q = 0, .r = 0}, .layer = 0};
  const auto second = dross::HexCellId{.region = region, .coord = {.q = 1, .r = 0}, .layer = 0};
  const auto profile = dross::HexBakeProfile{
      .algorithm_version = 1,
      .quantization_mm = 10,
      .maximum_height_variance_mm = 30,
      .required_sample_count = 7,
      .terrain = dross::ContentId::parse("dross:floor").value(),
      .movement_cost = dross::MovementCost{1},
  };
  const dross::GridBake bake{
      .identity = identity,
      .profile_version = 1,
      .cells =
          {
              {.id = first,
               .surface_samples_mm = {1000, 0, 0, 0, 0, 0, 0},
               .standing_clearance = true,
               .source = dross::ContentId::parse("godot:physics_geometry").value()},
              {.id = second,
               .surface_samples_mm = {0, 0, 0, 0, 0, 0, 0},
               .standing_clearance = true,
               .source = dross::ContentId::parse("godot:physics_geometry").value()},
          },
      .edges = {{.from = first, .to = second, .from_to_clear = false, .to_from_clear = false}},
  };
  const dross::GridOverrides overrides{
      .identity = identity,
      .cells = {{first, dross::CellTraversabilityOverride::force_traversable}},
  };
  const auto compiled = dross::compile_grid_bake(bake, overrides, profile);
  if (!compiled) {
    std::cerr << "grid bake compile failed\n";
    return scenario_error;
  }
  std::cout << "grid-bake cells=" << compiled->map.cell_ids().size()
            << " edges=" << compiled->map.edge_count() << " manual="
            << std::ranges::count_if(compiled->provenance,
                                     [](const auto& entry) {
                                       return entry.second != dross::CellProvenance::automatic;
                                     })
            << '\n';
  return 0;
}

[[nodiscard]] int run_command_scenario(const std::span<const char* const> arguments) {
  std::uint64_t seed = default_scenario_seed;
  std::string record_path;
  for (std::size_t index = 3; index < arguments.size(); index += 2) {
    if (index + 1 >= arguments.size()) {
      print_usage(std::cerr);
      return usage_error;
    }
    const std::string_view option{arguments[index]};
    if (option == "--record") {
      record_path = arguments[index + 1];
      continue;
    }
    if (option != "--seed") {
      print_usage(std::cerr);
      return usage_error;
    }
    const std::string_view value{arguments[index + 1]};
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), seed);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
      print_usage(std::cerr);
      return usage_error;
    }
  }
  return run_command_event_kernel_scenario(seed, record_path);
}

[[nodiscard]] int run_persistence(const std::span<const char* const> arguments) {
  std::uint64_t seed = default_scenario_seed;
  std::string save_path;
  for (std::size_t index = 3; index < arguments.size(); index += 2) {
    if (index + 1 >= arguments.size()) {
      print_usage(std::cerr);
      return usage_error;
    }
    const std::string_view option{arguments[index]};
    if (option == "--save") {
      save_path = arguments[index + 1];
      continue;
    }
    if (option != "--seed") {
      print_usage(std::cerr);
      return usage_error;
    }
    const std::string_view value{arguments[index + 1]};
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), seed);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
      print_usage(std::cerr);
      return usage_error;
    }
  }
  if (save_path.empty()) {
    print_usage(std::cerr);
    return usage_error;
  }
  return run_persistence_scenario(seed, save_path);
}

[[nodiscard]] std::optional<int>
dispatch_persistence_command(const std::span<const char* const> arguments) {
  if (arguments.size() >= persistence_minimum_argument_count &&
      std::string_view{arguments[1]} == "scenario" &&
      std::string_view{arguments[2]} == "persistence-foundation") {
    return run_persistence(arguments);
  }
  if (arguments.size() == 3 && std::string_view{arguments[1]} == "inspect-save") {
    return inspect_save(arguments[2]);
  }
  if (arguments.size() == resume_argument_count && std::string_view{arguments[1]} == "resume" &&
      std::string_view{arguments[3]} == "--commands") {
    return resume_save(ResumeSaveArguments{
        .save_path = arguments[2],
        .commands_path = arguments[4],
    });
  }
  return std::nullopt;
}

[[nodiscard]] int run_exploration_scenario(const std::span<const char* const> arguments) {
  std::uint64_t seed = default_scenario_seed;
  std::string record_path;
  for (std::size_t index = 3; index < arguments.size(); index += 2) {
    if (index + 1 >= arguments.size()) {
      print_usage(std::cerr);
      return usage_error;
    }
    const std::string_view option{arguments[index]};
    if (option == "--record") {
      record_path = arguments[index + 1];
      continue;
    }
    if (option != "--seed") {
      print_usage(std::cerr);
      return usage_error;
    }
    const std::string_view value{arguments[index + 1]};
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), seed);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
      print_usage(std::cerr);
      return usage_error;
    }
  }
  return run_exploration_movement_scenario(seed, record_path);
}

[[nodiscard]] int run_thump_command(const std::span<const char* const> arguments) {
  std::uint64_t seed = default_scenario_seed;
  std::string record_path;
  std::string save_checkpoint_directory;
  for (std::size_t index = 3; index < arguments.size(); index += 2) {
    if (index + 1 >= arguments.size()) {
      print_usage(std::cerr);
      return usage_error;
    }
    const std::string_view option{arguments[index]};
    if (option == "--record") {
      record_path = arguments[index + 1];
    } else if (option == "--save-checkpoints") {
      save_checkpoint_directory = arguments[index + 1];
    } else if (option == "--seed") {
      const std::string_view value{arguments[index + 1]};
      const auto parsed = std::from_chars(value.data(), value.data() + value.size(), seed);
      if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
        print_usage(std::cerr);
        return usage_error;
      }
    } else {
      print_usage(std::cerr);
      return usage_error;
    }
  }
  return run_thump_scenario(seed, record_path, save_checkpoint_directory);
}

[[nodiscard]] std::optional<int>
dispatch_information_command(const std::span<const char* const> arguments) {
  if (arguments.size() == 2 && std::string_view{arguments[1]} == "version") {
    if (!foundation_self_check()) {
      std::cerr << "foundation self-check failed\n";
      return self_check_error;
    }
    std::cout << dross::build_information() << '\n';
    return 0;
  }
  if (arguments.size() != 3 || std::string_view{arguments[1]} != "validate-id") {
    return std::nullopt;
  }
  const std::string_view input{arguments[2]};
  const auto parsed = dross::ContentId::parse(input);
  if (!parsed) {
    std::cerr << "invalid ContentId at byte " << parsed.error().position << ": "
              << reason_text(parsed.error().reason) << '\n';
    return validation_error;
  }
  std::cout << parsed->canonical() << '\n';
  return 0;
}

[[nodiscard]] std::optional<int>
dispatch_scenario_command(const std::span<const char* const> arguments) {
  if (arguments.size() < 3 || std::string_view{arguments[1]} != "scenario") {
    return std::nullopt;
  }
  const std::string_view scenario{arguments[2]};
  if (arguments.size() == 3 && scenario == "identity-lifecycle") {
    return run_identity_lifecycle();
  }
  if (arguments.size() == 3 && scenario == "hex-pathing") {
    return run_hex_pathing_scenario();
  }
  if (arguments.size() == 3 && scenario == "grid-bake") {
    return run_grid_bake();
  }
  if (scenario == "command-event-kernel") {
    return run_command_scenario(arguments);
  }
  if (scenario == "exploration-movement") {
    return run_exploration_scenario(arguments);
  }
  if (scenario == "thump-on-field-mouse") {
    return run_thump_command(arguments);
  }
  if (scenario == "lifecycle-machines" &&
      (arguments.size() == 3 || arguments.size() == lifecycle_record_argument_count)) {
    if (arguments.size() == 3) {
      return run_lifecycle_machine_scenario({});
    }
    if (std::string_view{arguments[3]} == "--record") {
      return run_lifecycle_machine_scenario(arguments[4]);
    }
  }
  return std::nullopt;
}

[[nodiscard]] std::optional<int>
dispatch_replay_command(const std::span<const char* const> arguments) {
  if (arguments.size() == 4 && std::string_view{arguments[1]} == "replay" &&
      std::string_view{arguments[2]} == "--verify-checkpoints") {
    return run_replay_verification(arguments[3]);
  }
  if (arguments.size() == 4 && std::string_view{arguments[1]} == "compare-runs") {
    return compare_runs(arguments[2], arguments[3]);
  }
  return std::nullopt;
}

} // namespace

int main(const int argument_count, const char* const arguments[]) {
  const std::span<const char* const> command_line{arguments,
                                                  static_cast<std::size_t>(argument_count)};
  if (const auto information = dispatch_information_command(command_line)) {
    return *information;
  }
  if (const auto scenario = dispatch_scenario_command(command_line)) {
    return *scenario;
  }
  const auto persistence = dispatch_persistence_command(command_line);
  if (persistence) {
    return *persistence;
  }
  if (const auto replay = dispatch_replay_command(command_line)) {
    return *replay;
  }

  print_usage(std::cerr);
  return usage_error;
}

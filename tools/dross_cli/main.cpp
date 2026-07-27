#include <dross/foundation/byte_codec.hpp>
#include <dross/foundation/quantities.hpp>
#include <dross/foundation/version.hpp>
#include <dross/identity/content_id.hpp>
#include <dross/world/world_storage.hpp>

#include "command_event_kernel_scenario.hpp"
#include "hex_pathing_scenario.hpp"

#include <cstdint>
#include <iostream>
#include <string_view>
#include <utility>

namespace {

constexpr int usage_error = 2;
constexpr int validation_error = 3;
constexpr int self_check_error = 4;
constexpr int scenario_error = 5;

void print_usage(std::ostream& output) {
  output << "usage:\n"
            "  dross_headless version\n"
            "  dross_headless validate-id <namespace:name>\n"
            "  dross_headless scenario identity-lifecycle\n"
            "  dross_headless scenario hex-pathing\n"
            "  dross_headless scenario command-event-kernel\n";
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

} // namespace

int main(const int argument_count, const char* const arguments[]) {
  if (argument_count == 2 && std::string_view{arguments[1]} == "version") {
    if (!foundation_self_check()) {
      std::cerr << "foundation self-check failed\n";
      return self_check_error;
    }
    std::cout << dross::build_information() << '\n';
    return 0;
  }

  if (argument_count == 3 && std::string_view{arguments[1]} == "validate-id") {
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

  if (argument_count == 3 && std::string_view{arguments[1]} == "scenario" &&
      std::string_view{arguments[2]} == "identity-lifecycle") {
    return run_identity_lifecycle();
  }
  if (argument_count == 3 && std::string_view{arguments[1]} == "scenario" &&
      std::string_view{arguments[2]} == "hex-pathing") {
    return run_hex_pathing_scenario();
  }
  if (argument_count == 3 && std::string_view{arguments[1]} == "scenario" &&
      std::string_view{arguments[2]} == "command-event-kernel") {
    return run_command_event_kernel_scenario();
  }

  print_usage(std::cerr);
  return usage_error;
}

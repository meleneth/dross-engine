#include "dross_world_host.hpp"

#include <dross/foundation/quantities.hpp>
#include <dross/hex/compiled_hex_map.hpp>
#include <dross/random/random_hub.hpp>
#include <dross/runtime/command_event_kernel.hpp>
#include <dross/runtime/fixed_tick_runtime.hpp>
#include <dross/runtime/machine_trace.hpp>
#include <dross/runtime/simulation_mode.hpp>
#include <dross/runtime/world_lifecycle.hpp>
#include <dross/world/world_storage.hpp>

#include <godot_cpp/core/class_db.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

namespace dross::godot_adapter {
namespace {

constexpr std::uint64_t synthetic_lineage = 8;
constexpr std::uint64_t synthetic_instance = 1;

CompiledHexMap make_synthetic_map() {
  const auto region = RegionId{ContentId::parse("dross:phase08").value()};
  CompiledHexMapBuilder builder;
  static_cast<void>(builder.add_cell(CellFacts{
      .id = HexCellId{.region = region, .coord = HexCoord{.q = 0, .r = 0}, .layer = 0},
      .surface_height = Millimeters{0},
      .terrain = ContentId::parse("dross:floor").value(),
      .base_cost = MovementCost{1},
      .clearance = Clearance::open,
      .traversable = true,
      .semantic_tags = {},
  }));
  return std::move(builder).build().value();
}

} // namespace

struct DrossWorldHost::RuntimeState {
  explicit RuntimeState(CompiledActorDefinition actor_definition)
      : actor{std::move(actor_definition)},
        world{WorldConfig{.lineage = synthetic_lineage,
                          .instance_id = WorldInstanceId{synthetic_instance}}},
        map{make_synthetic_map()}, lifecycle{machine_trace}, mode{machine_trace},
        kernel{world, map, scripts, trace} {
    static_cast<void>(world.write().spawn(SpawnPlan::runtime()));
    if (!lifecycle.begin_load() || !lifecycle.load_succeeded() || !lifecycle.begin_run()) {
      return;
    }
    runtime = std::make_unique<EngineRuntime>(kernel, lifecycle, mode, RuntimeConfig{});
  }

  CompiledActorDefinition actor;
  WorldStorage world;
  CompiledHexMap map;
  HeadlessPlacementScriptPort scripts;
  NullTraceSink trace;
  NullMachineTrace machine_trace;
  WorldLifecycle lifecycle;
  SimulationMode mode;
  CommandEventKernel kernel;
  std::unique_ptr<EngineRuntime> runtime;
};

struct DrossWorldHost::ScriptScenarioState {
  explicit ScriptScenarioState(const MasterSeed seed)
      : random{seed}, runtime{port, random}, mode{machine_trace} {}

  GodotScriptRuntime port;
  RandomHub random;
  TypedScriptRuntime runtime;
  NullMachineTrace machine_trace;
  SimulationMode mode;
  Tick tick{0};
};

DrossWorldHost::DrossWorldHost() = default;
DrossWorldHost::~DrossWorldHost() = default;

godot::Ref<DrossValidationError>
DrossWorldHost::start_synthetic_world(const godot::Ref<DrossActorDefinition>& actor) {
  if (actor.is_null()) {
    return DrossValidationError::create("<memory>", "actor", "actor is required");
  }
  auto compiled = actor->compile_core();
  if (!compiled) {
    return DrossValidationError::create(compiled.error().resource_path,
                                        compiled.error().property_name, compiled.error().message);
  }
  auto next = std::make_unique<RuntimeState>(std::move(*compiled));
  if (!next->runtime) {
    return DrossValidationError::create("<runtime>", "lifecycle",
                                        "synthetic world failed to enter running state");
  }
  state_ = std::move(next);
  return {};
}

void DrossWorldHost::stop_world() { state_.reset(); }

bool DrossWorldHost::is_running() const { return state_ && state_->runtime; }

bool DrossWorldHost::advance_test_tick() {
  if (!is_running()) {
    return false;
  }
  const auto report = state_->runtime->advance_tick();
  return !report.fault.has_value();
}

std::int64_t DrossWorldHost::get_tick() const {
  return is_running() ? static_cast<std::int64_t>(state_->runtime->clock().current().value()) : 0;
}

std::int64_t DrossWorldHost::get_entity_count() const {
  return is_running() ? static_cast<std::int64_t>(state_->world.read().entity_count()) : 0;
}

godot::String DrossWorldHost::get_actor_id() const {
  return is_running() ? godot::String{state_->actor.id.canonical().data()} : godot::String{};
}

std::int64_t DrossWorldHost::get_footprint_cell_count() const {
  return is_running() ? static_cast<std::int64_t>(state_->actor.footprint.offsets().size()) : 0;
}

bool DrossWorldHost::start_script_scenario(
    const godot::TypedArray<DrossScriptModuleDefinition>& modules, const std::int64_t seed) {
  if (seed < 0) {
    return false;
  }
  auto next = std::make_unique<ScriptScenarioState>(MasterSeed{static_cast<std::uint64_t>(seed)});
  for (std::int64_t index = 0; index < modules.size(); ++index) {
    godot::Ref<DrossScriptModuleDefinition> definition = modules[index];
    auto module = next->port.add_definition(definition);
    if (!module || !next->runtime.install(std::move(*module))) {
      return false;
    }
  }
  script_state_ = std::move(next);
  return true;
}

bool DrossWorldHost::run_script_scenario() {
  if (!script_state_) {
    return false;
  }
  auto& scenario = *script_state_;
  scenario.port.set_tick(scenario.tick);
  const auto region = RegionId{ContentId::parse("demo:region").value()};
  const auto entity = EntityRef{WorldInstanceId{1}, EntityId{7, 1}};
  const placement::PlaceEntity query{
      .entity = entity,
      .target = HexPose{
          .anchor = HexCellId{.region = region, .coord = HexCoord{.q = 0, .r = 0}, .layer = 0},
          .facing = HexFacing::east}};
  const auto rules = scenario.runtime.contribute_placement(query, scenario.tick);
  if (!rules.accepted || rules.fault) {
    return false;
  }
  for (const auto command : rules.deferred_mode_commands) {
    if (command == ScriptModeCommand::request_combat && !scenario.mode.request_combat()) {
      return false;
    }
  }
  static_cast<void>(scenario.mode.reach_safe_boundary());
  const auto event = scenario.runtime.on_entity_placed(
      placement::EntityPlaced{.entity = entity, .pose = query.target}, scenario.tick);
  if (event.fault) {
    return false;
  }
  scenario.tick = Tick{scenario.tick.value() + 1U};
  return true;
}

godot::String DrossWorldHost::get_script_call_order() const {
  if (!script_state_) {
    return {};
  }
  godot::String result;
  for (const auto& call : script_state_->port.calls()) {
    if (!result.is_empty()) {
      result += ",";
    }
    result += godot::String{call.c_str()};
  }
  return result;
}

godot::String DrossWorldHost::get_script_mode() const {
  if (!script_state_) {
    return "none";
  }
  switch (script_state_->mode.state()) {
  case SimulationModeState::exploration:
    return "exploration";
  case SimulationModeState::combat_pending:
    return "combat_pending";
  case SimulationModeState::combat:
    return "combat";
  }
  return "none";
}

bool DrossWorldHost::is_script_world_faulted() const {
  return script_state_ && script_state_->runtime.world_faulted();
}

namespace {
const ScriptStateValue* find_script_value(const ScriptStateBag& state,
                                          const godot::String& module_id,
                                          const std::int64_t entity_sequence,
                                          const godot::String& key) {
  const auto module_text = module_id.utf8();
  const auto key_text = key.utf8();
  auto module = ContentId::parse(
      std::string_view{module_text.get_data(), static_cast<std::size_t>(module_text.length())});
  auto parsed_key = ScriptStateKey::parse(
      std::string{key_text.get_data(), static_cast<std::size_t>(key_text.length())});
  if (!module || !parsed_key || entity_sequence < 0) {
    return nullptr;
  }
  const auto region = ContentId::parse("demo:region").value();
  const auto scope = entity_sequence == 0
                         ? ScriptScope::for_region(region)
                         : ScriptScope::for_entity(
                               region, EntityId{7, static_cast<std::uint64_t>(entity_sequence)});
  return state.find(ScriptStateAddress{
      .module_id = std::move(*module), .scope = scope, .key = std::move(*parsed_key)});
}
} // namespace

bool DrossWorldHost::get_script_state_bool(const godot::String& module_id,
                                           const std::int64_t entity_sequence,
                                           const godot::String& key) const {
  const auto* value = script_state_ ? find_script_value(script_state_->runtime.state(), module_id,
                                                        entity_sequence, key)
                                    : nullptr;
  return value && std::holds_alternative<bool>(*value) && std::get<bool>(*value);
}

std::int64_t DrossWorldHost::get_script_state_int(const godot::String& module_id,
                                                  const std::int64_t entity_sequence,
                                                  const godot::String& key) const {
  const auto* value = script_state_ ? find_script_value(script_state_->runtime.state(), module_id,
                                                        entity_sequence, key)
                                    : nullptr;
  return value && std::holds_alternative<std::int64_t>(*value) ? std::get<std::int64_t>(*value)
                                                               : -1;
}

godot::PackedByteArray DrossWorldHost::save_script_state() const {
  godot::PackedByteArray output;
  if (!script_state_) {
    return output;
  }
  const auto encoded = encode_script_state(script_state_->runtime.state());
  output.resize(static_cast<std::int64_t>(encoded.size()));
  for (std::size_t index = 0; index < encoded.size(); ++index) {
    output.set(static_cast<std::int64_t>(index), std::to_integer<std::uint8_t>(encoded[index]));
  }
  return output;
}

bool DrossWorldHost::restore_script_state(const godot::PackedByteArray& bytes) {
  if (!script_state_) {
    return false;
  }
  std::vector<std::byte> encoded(static_cast<std::size_t>(bytes.size()));
  for (std::int64_t index = 0; index < bytes.size(); ++index) {
    encoded[static_cast<std::size_t>(index)] = static_cast<std::byte>(bytes[index]);
  }
  auto decoded = decode_script_state(encoded);
  if (!decoded) {
    return false;
  }
  script_state_->runtime.restore_state(std::move(*decoded));
  return true;
}

void DrossWorldHost::_bind_methods() {
  godot::ClassDB::bind_method(godot::D_METHOD("start_synthetic_world", "actor"),
                              &DrossWorldHost::start_synthetic_world);
  godot::ClassDB::bind_method(godot::D_METHOD("stop_world"), &DrossWorldHost::stop_world);
  godot::ClassDB::bind_method(godot::D_METHOD("is_running"), &DrossWorldHost::is_running);
  godot::ClassDB::bind_method(godot::D_METHOD("advance_test_tick"),
                              &DrossWorldHost::advance_test_tick);
  godot::ClassDB::bind_method(godot::D_METHOD("get_tick"), &DrossWorldHost::get_tick);
  godot::ClassDB::bind_method(godot::D_METHOD("get_entity_count"),
                              &DrossWorldHost::get_entity_count);
  godot::ClassDB::bind_method(godot::D_METHOD("get_actor_id"), &DrossWorldHost::get_actor_id);
  godot::ClassDB::bind_method(godot::D_METHOD("get_footprint_cell_count"),
                              &DrossWorldHost::get_footprint_cell_count);
  godot::ClassDB::bind_method(godot::D_METHOD("start_script_scenario", "modules", "seed"),
                              &DrossWorldHost::start_script_scenario);
  godot::ClassDB::bind_method(godot::D_METHOD("run_script_scenario"),
                              &DrossWorldHost::run_script_scenario);
  godot::ClassDB::bind_method(godot::D_METHOD("get_script_call_order"),
                              &DrossWorldHost::get_script_call_order);
  godot::ClassDB::bind_method(godot::D_METHOD("get_script_mode"), &DrossWorldHost::get_script_mode);
  godot::ClassDB::bind_method(godot::D_METHOD("is_script_world_faulted"),
                              &DrossWorldHost::is_script_world_faulted);
  godot::ClassDB::bind_method(
      godot::D_METHOD("get_script_state_bool", "module_id", "entity_sequence", "key"),
      &DrossWorldHost::get_script_state_bool);
  godot::ClassDB::bind_method(
      godot::D_METHOD("get_script_state_int", "module_id", "entity_sequence", "key"),
      &DrossWorldHost::get_script_state_int);
  godot::ClassDB::bind_method(godot::D_METHOD("save_script_state"),
                              &DrossWorldHost::save_script_state);
  godot::ClassDB::bind_method(godot::D_METHOD("restore_script_state", "bytes"),
                              &DrossWorldHost::restore_script_state);
}

} // namespace dross::godot_adapter

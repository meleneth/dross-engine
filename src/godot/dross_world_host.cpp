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
}

} // namespace dross::godot_adapter

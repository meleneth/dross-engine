#include "dross_world_host.hpp"

#include <dross/foundation/quantities.hpp>
#include <dross/hex/compiled_hex_map.hpp>
#include <dross/random/random_hub.hpp>
#include <dross/runtime/command_event_kernel.hpp>
#include <dross/runtime/fixed_tick_runtime.hpp>
#include <dross/runtime/machine_trace.hpp>
#include <dross/runtime/movement_runtime.hpp>
#include <dross/runtime/simulation_mode.hpp>
#include <dross/runtime/world_lifecycle.hpp>
#include <dross/world/world_storage.hpp>

#include <godot_cpp/core/class_db.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
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

HexCellId movement_cell(const std::int32_t q) {
  return {.region = RegionId{ContentId::parse("dross:phase11").value()},
          .coord = {.q = q, .r = 0},
          .layer = 0};
}

HexPose movement_pose(const std::int32_t q) {
  return {.anchor = movement_cell(q), .facing = HexFacing::east};
}

CompiledHexMap make_movement_map() {
  CompiledHexMapBuilder builder;
  for (std::int32_t q = 0; q < 4; ++q) {
    static_cast<void>(builder.add_cell(CellFacts{
        .id = movement_cell(q),
        .surface_height = Millimeters{0},
        .terrain = ContentId::parse("dross:floor").value(),
        .base_cost = MovementCost{1},
        .clearance = Clearance::open,
        .traversable = true,
        .semantic_tags = {},
    }));
    if (q > 0) {
      static_cast<void>(
          builder.add_edge(movement_cell(q - 1), movement_cell(q),
                           DirectionalEdgeFacts{.traversable = true, .cost = MovementCost{1}},
                           DirectionalEdgeFacts{.traversable = true, .cost = MovementCost{1}}));
    }
  }
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

struct DrossWorldHost::MovementScenarioState final : MovementEventSink {
  MovementScenarioState()
      : map{make_movement_map()},
        footprint{
            FootprintDefinition::create(
                FootprintId{ContentId::parse("dross:phase11_actor").value()}, {{.q = 0, .r = 0}})
                .value()},
        movement{map,
                 occupancy,
                 planner,
                 footprint,
                 entity,
                 movement_pose(0),
                 MovementConfig{.ticks_per_transition = 2},
                 this} {
    if (!occupancy.place(entity.id(), {movement_cell(0)})) {
      throw std::logic_error{"movement boundary occupancy setup failed"};
    }
  }

  void publish(const movement::MovementStarted&) override {}
  void publish(const movement::ActorEnteredCell&) override {}
  void publish(const movement::MovementCompleted&) override {}

  CompiledHexMap map;
  OccupancyIndex occupancy;
  WeightedAStarPathPlanner planner;
  FootprintDefinition footprint;
  EntityRef entity{WorldInstanceId{synthetic_instance}, EntityId{7, 1}};
  MovementRuntime movement;
  Tick tick{0};
  bool combat_pending{false};
  bool combat{false};
};

struct DrossWorldHost::CombatScenarioState final : AbilityResolver::EventSink,
                                                   AbilityResolver::RuleSource {
  CombatScenarioState(AbilityDefinition definition, ScriptScenarioState* script_scenario,
                      const std::int32_t player_q)
      : ability{std::move(definition)}, scripts{script_scenario},
        session{{
            {.entity = player, .initiative = 10, .maximum_action_points = 3},
            {.entity = mouse, .initiative = 5, .maximum_action_points = 2},
        }},
        resolver{session,
                 {
                     {.entity = player, .pose = movement_pose(player_q), .health = HitPoints{8}},
                     {.entity = mouse, .pose = movement_pose(player_q + 1), .health = HitPoints{3}},
                 },
                 this,
                 nullptr,
                 this} {
    if (!session.start()) {
      throw std::logic_error{"Godot Thump scenario combat start failed"};
    }
  }

  bool allows(const AbilityDefinition& definition, const EntityRef& actor,
              const EntityRef& target) override {
    if (scripts == nullptr) {
      return true;
    }
    scripts->port.set_tick(scripts->tick);
    const auto result = scripts->runtime.contribute_ability(
        combat::PerformAbility{.actor = actor, .target = target, .ability = definition.id},
        scripts->tick);
    script_fault = result.fault.has_value();
    return result.accepted;
  }

  void publish(const combat::AbilityCommitted& event) override {
    if (event.ability == ability.id) {
      last_cue = godot::String{ability.presentation_cue.canonical().data()};
    }
  }

  void publish(const combat::DamageApplied& event) override {
    if (scripts != nullptr) {
      scripts->port.set_tick(scripts->tick);
      script_fault = scripts->runtime.on_damage_applied(event, scripts->tick).fault.has_value();
    }
  }

  void publish(const combat::ActorKilled& event) override {
    if (scripts != nullptr) {
      scripts->port.set_tick(scripts->tick);
      script_fault =
          script_fault || scripts->runtime.on_actor_killed(event, scripts->tick).fault.has_value();
    }
  }

  EntityRef player{WorldInstanceId{synthetic_instance}, EntityId{7, 1}};
  EntityRef mouse{WorldInstanceId{synthetic_instance}, EntityId{7, 2}};
  AbilityDefinition ability;
  ScriptScenarioState* scripts;
  CombatSession session;
  AbilityResolver resolver;
  bool killed{false};
  bool script_fault{false};
  godot::String last_cue;
};

struct DrossWorldHost::DoorScenarioState final : DoorRuntime::EventSink {
  explicit DoorScenarioState(CompiledDoorDefinition definition)
      : edge{definition.footprint.edges().front()},
        runtime{entity, std::move(definition.footprint), DoorState::closed, this, 2} {}

  void publish(const door::DoorOpened&) override { last_event = "dross:door_opened"; }
  void publish(const door::DoorClosed&) override { last_event = "dross:door_closed"; }

  EntityRef entity{WorldInstanceId{synthetic_instance}, EntityId{7, 3}};
  EdgeKey edge;
  DoorRuntime runtime;
  godot::String last_event;
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

bool DrossWorldHost::start_movement_scenario() {
  movement_state_ = std::make_unique<MovementScenarioState>();
  return true;
}

godot::Ref<DrossMovementPreview>
DrossWorldHost::preview_movement(const std::int64_t destination_q) const {
  godot::Ref<DrossMovementPreview> output;
  output.instantiate();
  if (!movement_state_ || destination_q < 0 || destination_q > 3) {
    output->initialize(false, 0, 0, {});
    return output;
  }
  const auto preview =
      movement_state_->movement.preview(movement_pose(static_cast<std::int32_t>(destination_q)));
  godot::PackedInt32Array columns;
  for (const auto& path_pose : preview.path) {
    columns.push_back(path_pose.anchor.coord.q);
  }
  output->initialize(preview.accepted, static_cast<std::int64_t>(preview.cost.value()),
                     static_cast<std::int64_t>(preview.duration_ticks), std::move(columns));
  return output;
}

bool DrossWorldHost::move_to(const std::int64_t destination_q) {
  return movement_state_ && destination_q >= 0 && destination_q <= 3 &&
         movement_state_->movement.move_to(movement_pose(static_cast<std::int32_t>(destination_q)));
}

bool DrossWorldHost::cancel_movement() {
  return movement_state_ && movement_state_->movement.cancel();
}

bool DrossWorldHost::request_movement_combat() {
  if (!movement_state_ || movement_state_->combat_pending || movement_state_->combat) {
    return false;
  }
  movement_state_->combat_pending = true;
  movement_state_->movement.request_combat_stop();
  return true;
}

bool DrossWorldHost::advance_movement_tick() {
  if (!movement_state_) {
    return false;
  }
  const auto result = movement_state_->movement.advance(movement_state_->tick);
  if (result == MovementAdvance::combat_boundary ||
      (movement_state_->combat_pending &&
       movement_state_->movement.state() == MovementLifecycleState::idle)) {
    movement_state_->combat_pending = false;
    movement_state_->combat = true;
  }
  movement_state_->tick = Tick{movement_state_->tick.value() + 1};
  return true;
}

std::int64_t DrossWorldHost::get_movement_tick() const {
  return movement_state_ ? static_cast<std::int64_t>(movement_state_->tick.value()) : 0;
}

std::int64_t DrossWorldHost::get_movement_column() const {
  return movement_state_ ? movement_state_->movement.pose().anchor.coord.q : -1;
}

godot::String DrossWorldHost::get_movement_state() const {
  if (!movement_state_) {
    return "none";
  }
  switch (movement_state_->movement.state()) {
  case MovementLifecycleState::idle:
    return "idle";
  case MovementLifecycleState::traversing:
    return "traversing";
  case MovementLifecycleState::blocked:
    return "blocked";
  }
  return "none";
}

godot::String DrossWorldHost::get_movement_mode() const {
  if (!movement_state_) {
    return "none";
  }
  if (movement_state_->combat) {
    return "combat";
  }
  return movement_state_->combat_pending ? "combat_pending" : "exploration";
}

bool DrossWorldHost::start_thump_scenario(
    const godot::Ref<DrossAbilityDefinition>& ability_definition) {
  if (ability_definition.is_null()) {
    return false;
  }
  if (movement_state_ && movement_state_->movement.state() != MovementLifecycleState::idle) {
    return false;
  }
  auto ability = ability_definition->compile_core();
  if (!ability) {
    return false;
  }
  const auto player_q = movement_state_ ? movement_state_->movement.pose().anchor.coord.q : 0;
  combat_state_ =
      std::make_unique<CombatScenarioState>(std::move(*ability), script_state_.get(), player_q);
  return true;
}

bool DrossWorldHost::perform_thump() {
  if (!combat_state_) {
    return false;
  }
  const auto result = combat_state_->resolver.perform(
      combat_state_->ability, combat_state_->player.id(), combat_state_->mouse.id());
  if (!result.accepted) {
    return false;
  }
  combat_state_->killed = result.killed;
  return !combat_state_->script_fault;
}

std::int64_t DrossWorldHost::get_mouse_health() const {
  return combat_state_ ? combat_state_->resolver.health(combat_state_->mouse.id()).value() : -1;
}

bool DrossWorldHost::is_mouse_killed() const { return combat_state_ && combat_state_->killed; }

godot::String DrossWorldHost::get_last_presentation_cue() const {
  return combat_state_ ? combat_state_->last_cue : godot::String{};
}

bool DrossWorldHost::start_door_scenario(const godot::Ref<DrossDoorDefinition>& definition) {
  if (definition.is_null()) {
    return false;
  }
  auto compiled = definition->compile_core();
  if (!compiled) {
    return false;
  }
  door_state_ = std::make_unique<DoorScenarioState>(std::move(*compiled));
  return true;
}

bool DrossWorldHost::open_door() { return door_state_ && door_state_->runtime.open(); }
bool DrossWorldHost::close_door() { return door_state_ && door_state_->runtime.close(); }
bool DrossWorldHost::is_door_open() const {
  return door_state_ && door_state_->runtime.state() == DoorState::open;
}
bool DrossWorldHost::is_door_edge_traversable() const {
  return door_state_ && door_state_->runtime.allows(door_state_->edge);
}
bool DrossWorldHost::is_door_presentation_pending() const {
  return door_state_ && door_state_->runtime.presentation_pending();
}
std::int64_t DrossWorldHost::get_door_presentation_acknowledgement_id() const {
  return door_state_
             ? static_cast<std::int64_t>(door_state_->runtime.presentation_acknowledgement_id())
             : 0;
}
bool DrossWorldHost::acknowledge_door_presentation(const std::int64_t acknowledgement_id) {
  return door_state_ && acknowledgement_id > 0 &&
         door_state_->runtime.acknowledge_presentation(
             static_cast<std::uint64_t>(acknowledgement_id));
}
bool DrossWorldHost::advance_door_presentation() {
  return door_state_ && door_state_->runtime.advance_presentation();
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
  godot::ClassDB::bind_method(godot::D_METHOD("start_movement_scenario"),
                              &DrossWorldHost::start_movement_scenario);
  godot::ClassDB::bind_method(godot::D_METHOD("preview_movement", "destination_q"),
                              &DrossWorldHost::preview_movement);
  godot::ClassDB::bind_method(godot::D_METHOD("move_to", "destination_q"),
                              &DrossWorldHost::move_to);
  godot::ClassDB::bind_method(godot::D_METHOD("cancel_movement"), &DrossWorldHost::cancel_movement);
  godot::ClassDB::bind_method(godot::D_METHOD("request_movement_combat"),
                              &DrossWorldHost::request_movement_combat);
  godot::ClassDB::bind_method(godot::D_METHOD("advance_movement_tick"),
                              &DrossWorldHost::advance_movement_tick);
  godot::ClassDB::bind_method(godot::D_METHOD("get_movement_tick"),
                              &DrossWorldHost::get_movement_tick);
  godot::ClassDB::bind_method(godot::D_METHOD("get_movement_column"),
                              &DrossWorldHost::get_movement_column);
  godot::ClassDB::bind_method(godot::D_METHOD("get_movement_state"),
                              &DrossWorldHost::get_movement_state);
  godot::ClassDB::bind_method(godot::D_METHOD("get_movement_mode"),
                              &DrossWorldHost::get_movement_mode);
  godot::ClassDB::bind_method(godot::D_METHOD("start_thump_scenario", "ability_definition"),
                              &DrossWorldHost::start_thump_scenario);
  godot::ClassDB::bind_method(godot::D_METHOD("perform_thump"), &DrossWorldHost::perform_thump);
  godot::ClassDB::bind_method(godot::D_METHOD("get_mouse_health"),
                              &DrossWorldHost::get_mouse_health);
  godot::ClassDB::bind_method(godot::D_METHOD("is_mouse_killed"), &DrossWorldHost::is_mouse_killed);
  godot::ClassDB::bind_method(godot::D_METHOD("get_last_presentation_cue"),
                              &DrossWorldHost::get_last_presentation_cue);
  godot::ClassDB::bind_method(godot::D_METHOD("start_door_scenario", "definition"),
                              &DrossWorldHost::start_door_scenario);
  godot::ClassDB::bind_method(godot::D_METHOD("open_door"), &DrossWorldHost::open_door);
  godot::ClassDB::bind_method(godot::D_METHOD("close_door"), &DrossWorldHost::close_door);
  godot::ClassDB::bind_method(godot::D_METHOD("is_door_open"), &DrossWorldHost::is_door_open);
  godot::ClassDB::bind_method(godot::D_METHOD("is_door_edge_traversable"),
                              &DrossWorldHost::is_door_edge_traversable);
  godot::ClassDB::bind_method(godot::D_METHOD("is_door_presentation_pending"),
                              &DrossWorldHost::is_door_presentation_pending);
  godot::ClassDB::bind_method(godot::D_METHOD("get_door_presentation_acknowledgement_id"),
                              &DrossWorldHost::get_door_presentation_acknowledgement_id);
  godot::ClassDB::bind_method(
      godot::D_METHOD("acknowledge_door_presentation", "acknowledgement_id"),
      &DrossWorldHost::acknowledge_door_presentation);
  godot::ClassDB::bind_method(godot::D_METHOD("advance_door_presentation"),
                              &DrossWorldHost::advance_door_presentation);
}

} // namespace dross::godot_adapter

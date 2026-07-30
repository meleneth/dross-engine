#include "dross_world_host.hpp"

#include "content.hpp"

#include <dross/foundation/quantities.hpp>
#include <dross/hex/compiled_hex_map.hpp>
#include <dross/persistence/save_container.hpp>
#include <dross/random/random_hub.hpp>
#include <dross/runtime/command_event_kernel.hpp>
#include <dross/runtime/dialogue_runtime.hpp>
#include <dross/runtime/fixed_tick_runtime.hpp>
#include <dross/runtime/machine_trace.hpp>
#include <dross/runtime/movement_runtime.hpp>
#include <dross/runtime/replay.hpp>
#include <dross/runtime/simulation_mode.hpp>
#include <dross/runtime/world_lifecycle.hpp>
#include <dross/world/world_storage.hpp>

#include <godot_cpp/core/class_db.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>

namespace dross::godot_adapter {
namespace {

constexpr std::uint64_t synthetic_lineage = 8;
constexpr std::uint64_t synthetic_instance = 1;
constexpr std::uint32_t movement_transition_ticks = 2;

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

HexCellId movement_cell(const std::int32_t q, const std::int32_t r = 0) {
  return {.region = RegionId{ContentId::parse("dross:phase11").value()},
          .coord = {.q = q, .r = r},
          .layer = 0};
}

HexPose movement_pose(const std::int32_t q, const std::int32_t r = 0) {
  return {.anchor = movement_cell(q, r), .facing = HexFacing::east};
}

CompiledHexMap make_movement_map() {
  CompiledHexMapBuilder builder;
  for (std::int32_t r = -1; r <= 1; ++r) {
    for (std::int32_t q = 0; q < 4; ++q) {
      static_cast<void>(builder.add_cell(CellFacts{
          .id = movement_cell(q, r),
          .surface_height = Millimeters{0},
          .terrain = ContentId::parse("dross:floor").value(),
          .base_cost = MovementCost{1},
          .clearance = Clearance::open,
          .traversable = true,
          .semantic_tags = {},
      }));
    }
  }
  constexpr std::array forward_directions{
      HexDirection::east,
      HexDirection::southeast,
      HexDirection::southwest,
  };
  for (std::int32_t r = -1; r <= 1; ++r) {
    for (std::int32_t q = 0; q < 4; ++q) {
      for (const auto direction : forward_directions) {
        const auto adjacent = neighbor(HexCoord{.q = q, .r = r}, direction);
        if (adjacent.q < 0 || adjacent.q > 3 || adjacent.r < -1 || adjacent.r > 1) {
          continue;
        }
        static_cast<void>(
            builder.add_edge(movement_cell(q, r), movement_cell(adjacent.q, adjacent.r),
                             DirectionalEdgeFacts{.traversable = true, .cost = MovementCost{1}},
                             DirectionalEdgeFacts{.traversable = true, .cost = MovementCost{1}}));
      }
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

struct DrossWorldHost::ScriptScenarioState final : DialogueRuntime::EventSink {
  explicit ScriptScenarioState(const MasterSeed seed)
      : random{seed}, runtime{port, random}, mode{machine_trace},
        inventory{WorldInstanceId{synthetic_instance}, {EntityId{7, 1}}}, quests{machine_trace},
        dialogue{WorldInstanceId{synthetic_instance},
                 {player.id(), caretaker.id()},
                 machine_trace,
                 this} {
    port.set_world_instance(WorldInstanceId{synthetic_instance});
    port.set_inventory(&inventory);
    port.set_quests(&quests);
  }

  void publish(const dialogue::DialogueStarted&) override {}
  void publish(const dialogue::DialogueOptionChosen& event) override {
    port.set_tick(tick);
    const auto result = runtime.on_dialogue_option_chosen(event, tick);
    script_fault = !commit(result);
  }
  void publish(const dialogue::DialogueEnded&) override {}

  [[nodiscard]] bool commit(const ScriptEventResult& result) {
    if (result.fault) {
      return false;
    }
    const auto inventory_before = inventory.snapshot();
    const auto quests_before = quests.snapshot();
    for (const auto& command : result.deferred_inventory_commands) {
      const auto handled =
          std::visit([this](const auto& typed) { return inventory.handle(typed); }, command);
      if (!handled) {
        static_cast<void>(inventory.restore(inventory_before));
        static_cast<void>(quests.restore(quests_before));
        return false;
      }
    }
    for (const auto& command : result.deferred_quest_commands) {
      const auto handled =
          std::visit([this](const auto& typed) { return quests.handle(typed); }, command);
      if (!handled) {
        static_cast<void>(inventory.restore(inventory_before));
        static_cast<void>(quests.restore(quests_before));
        return false;
      }
    }
    return true;
  }

  [[nodiscard]] std::optional<std::vector<ContentId>>
  dialogue_options(const ContentId& dialogue_id) {
    port.set_tick(tick);
    const auto result = runtime.contribute_dialogue_options(
        ScriptDialogueOptionQuery{
            .initiator = player,
            .partner = caretaker,
            .dialogue = dialogue_id,
        },
        tick);
    if (result.fault) {
      script_fault = true;
      return std::nullopt;
    }
    return result.options;
  }

  [[nodiscard]] bool choose_dialogue_option(const ContentId& dialogue_id, const ContentId& option) {
    const auto before = dialogue.snapshot();
    const auto offered = dialogue_options(dialogue_id);
    if (!offered ||
        !dialogue.handle(dialogue::BeginDialogue{
            .initiator = player, .partner = caretaker, .dialogue = dialogue_id}) ||
        !dialogue.offer_options(*offered) ||
        !dialogue.handle(dialogue::ChooseDialogueOption{.initiator = player,
                                                        .partner = caretaker,
                                                        .dialogue = dialogue_id,
                                                        .option = option}) ||
        script_fault ||
        !dialogue.handle(dialogue::EndDialogue{
            .initiator = player, .partner = caretaker, .dialogue = dialogue_id})) {
      static_cast<void>(dialogue.restore(before));
      return false;
    }
    return true;
  }

  EntityRef player{WorldInstanceId{synthetic_instance}, EntityId{7, 1}};
  EntityRef caretaker{WorldInstanceId{synthetic_instance}, EntityId{7, 3}};
  GodotScriptRuntime port;
  RandomHub random;
  TypedScriptRuntime runtime;
  NullMachineTrace machine_trace;
  SimulationMode mode;
  InventoryRuntime inventory;
  QuestRuntime quests;
  DialogueRuntime dialogue;
  bool script_fault{false};
  Tick tick{0};
};

struct DrossWorldHost::MovementScenarioState final : MovementEventSink {
  explicit MovementScenarioState(const MovementSnapshot* restored = nullptr)
      : map{make_movement_map()},
        footprint{
            FootprintDefinition::create(
                FootprintId{ContentId::parse("dross:phase11_actor").value()}, {{.q = 0, .r = 0}})
                .value()},
        entity{WorldInstanceId{synthetic_instance}, EntityId{7, 1}},
        occupancy{make_occupancy(entity.id(), footprint, restored)},
        movement{map,
                 occupancy,
                 planner,
                 footprint,
                 entity,
                 restored ? restored->pose : movement_pose(0),
                 MovementConfig{.ticks_per_transition = movement_transition_ticks},
                 this},
        mode{machine_trace} {
    if (restored != nullptr && !movement.restore(*restored)) {
      throw std::logic_error{"movement boundary snapshot restore failed"};
    }
  }

  static OccupancyIndex make_occupancy(const EntityId entity, const FootprintDefinition& footprint,
                                       const MovementSnapshot* restored) {
    OccupancyIndex result;
    if (restored != nullptr) {
      const std::array placement{OccupancyPlacement{
          .entity = entity,
          .cells = footprint.expand(restored->pose),
      }};
      const auto revision = restored->state == MovementLifecycleState::idle
                                ? 1U
                                : restored->expected_occupancy_revision;
      if (!result.restore(placement, revision)) {
        throw std::logic_error{"movement boundary occupancy restore failed"};
      }
    } else if (!result.place(entity, {movement_cell(0)})) {
      throw std::logic_error{"movement boundary occupancy setup failed"};
    }
    return result;
  }

  void publish(const movement::MovementStarted&) override {
    record_event("dross:movement_started");
  }
  void publish(const movement::ActorEnteredCell&) override {
    record_event("dross:actor_entered_cell");
  }
  void publish(const movement::MovementCompleted&) override {
    record_event("dross:movement_completed");
  }

  void record_event(const godot::String& event_id) {
    constexpr std::size_t diagnostic_event_limit = 8;
    if (recent_events.size() == diagnostic_event_limit) {
      recent_events.erase(recent_events.begin());
    }
    recent_events.push_back(event_id);
  }

  CompiledHexMap map;
  FootprintDefinition footprint;
  EntityRef entity;
  OccupancyIndex occupancy;
  WeightedAStarPathPlanner planner;
  MovementRuntime movement;
  NullMachineTrace machine_trace;
  SimulationMode mode;
  Tick tick{0};
  std::vector<godot::String> recent_events;
};

struct DrossWorldHost::CombatScenarioState final : AbilityResolver::EventSink,
                                                   AbilityResolver::RuleSource {
  CombatScenarioState(AbilityDefinition definition, ScriptScenarioState* script_scenario,
                      const HexCoord player_coord)
      : ability{std::move(definition)}, scripts{script_scenario},
        session{{
            {.entity = player, .initiative = 10, .maximum_action_points = 3},
            {.entity = mouse, .initiative = 5, .maximum_action_points = 2},
        }},
        resolver{session,
                 {
                     {.entity = player,
                      .pose = movement_pose(player_coord.q, player_coord.r),
                      .health = HitPoints{8}},
                     {.entity = mouse,
                      .pose = movement_pose(player_coord.q + 1, player_coord.r),
                      .health = HitPoints{3}},
                 },
                 this,
                 nullptr,
                 this} {
    if (!session.start()) {
      throw std::logic_error{"Godot Thump scenario combat start failed"};
    }
  }

  CombatScenarioState(AbilityDefinition definition, ScriptScenarioState* script_scenario,
                      const CombatBoundarySnapshot& restored)
      : ability{std::move(definition)}, scripts{script_scenario},
        session{combatant_definitions(restored.session)},
        resolver{session, actor_states(restored.actors), this, nullptr, this} {
    if (restored.ability != ability.id || !session.restore(restored.session)) {
      throw std::logic_error{"Godot Thump combat snapshot restore failed"};
    }
  }

  static std::vector<CombatantDefinition>
  combatant_definitions(const CombatSessionSnapshot& snapshot) {
    std::vector<CombatantDefinition> result;
    result.reserve(snapshot.combatants.size());
    for (const auto& combatant : snapshot.combatants) {
      result.push_back({
          .entity = EntityRef{WorldInstanceId{synthetic_instance}, combatant.entity},
          .initiative = combatant.initiative,
          .maximum_action_points = combatant.maximum_action_points,
      });
    }
    return result;
  }

  static std::vector<AbilityActorState> actor_states(const AbilityResolverSnapshot& snapshot) {
    std::vector<AbilityActorState> result;
    result.reserve(snapshot.actors.size());
    for (const auto& actor : snapshot.actors) {
      result.push_back({
          .entity = EntityRef{WorldInstanceId{synthetic_instance}, actor.entity},
          .pose = actor.pose,
          .health = actor.health,
      });
    }
    return result;
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
    record_event("dross:ability_committed");
    if (event.ability == ability.id) {
      last_cue = godot::String{ability.presentation_cue.canonical().data()};
    }
  }

  void publish(const combat::DamageApplied& event) override {
    record_event("dross:damage_applied");
    if (scripts != nullptr) {
      scripts->port.set_tick(scripts->tick);
      script_fault = scripts->runtime.on_damage_applied(event, scripts->tick).fault.has_value();
    }
  }

  void publish(const combat::ActorKilled& event) override {
    record_event("dross:actor_killed");
    if (scripts != nullptr) {
      scripts->port.set_tick(scripts->tick);
      const auto result = scripts->runtime.on_actor_killed(event, scripts->tick);
      script_fault = script_fault || !scripts->commit(result);
    }
  }

  void record_event(const godot::String& event_id) {
    constexpr std::size_t diagnostic_event_limit = 8;
    if (recent_events.size() == diagnostic_event_limit) {
      recent_events.erase(recent_events.begin());
    }
    recent_events.push_back(event_id);
  }

  EntityRef player{WorldInstanceId{synthetic_instance}, EntityId{7, 1}};
  EntityRef mouse{WorldInstanceId{synthetic_instance}, EntityId{7, 2}};
  AbilityDefinition ability;
  ScriptScenarioState* scripts;
  CombatSession session;
  AbilityResolver resolver;
  bool script_fault{false};
  godot::String last_cue;
  std::vector<godot::String> recent_events;
};

struct DrossWorldHost::DoorScenarioState final : DoorRuntime::EventSink {
  explicit DoorScenarioState(CompiledDoorDefinition definition,
                             const DoorState initial_state = DoorState::closed)
      : definition_id{definition.id}, edges{definition.footprint.edges()},
        edge{definition.footprint.edges().front()},
        runtime{entity, std::move(definition.footprint), initial_state, this, 2} {}

  void publish(const door::DoorOpened&) override { last_event = "dross:door_opened"; }
  void publish(const door::DoorClosed&) override { last_event = "dross:door_closed"; }

  EntityRef entity{WorldInstanceId{synthetic_instance}, EntityId{7, 3}};
  ContentId definition_id;
  std::vector<EdgeKey> edges;
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

std::int64_t DrossWorldHost::get_inventory_count(const std::int64_t owner_sequence,
                                                 const godot::String& item) const {
  const auto item_text = item.utf8();
  const auto parsed = ContentId::parse(
      std::string_view{item_text.get_data(), static_cast<std::size_t>(item_text.length())});
  if (!script_state_ || !parsed || owner_sequence <= 0) {
    return 0;
  }
  return static_cast<std::int64_t>(script_state_->inventory.count(
      EntityRef{WorldInstanceId{synthetic_instance},
                EntityId{7, static_cast<std::uint64_t>(owner_sequence)}},
      *parsed));
}

bool DrossWorldHost::accept_mouse_quest_dialogue() {
  if (!script_state_) {
    return false;
  }
  auto& scenario = *script_state_;
  const auto dialogue_id = ContentId::parse("thump_demo:caretaker_dialogue").value();
  const auto option = ContentId::parse("thump_demo:accept_mouse_quest").value();
  return scenario.choose_dialogue_option(dialogue_id, option);
}

bool DrossWorldHost::hand_in_mouse_tail_dialogue() {
  if (!script_state_) {
    return false;
  }
  auto& scenario = *script_state_;
  const auto dialogue_id = ContentId::parse("thump_demo:caretaker_dialogue").value();
  const auto option = ContentId::parse("thump_demo:hand_over_mouse_tail").value();
  if (scenario.quests.stage(ContentId::parse("thump_demo:mouse_quest").value()) !=
          ContentId::parse("thump_demo:return_tail").value() ||
      !scenario.inventory.has(scenario.player, ContentId::parse("thump_demo:mouse_tail").value(),
                              1)) {
    return false;
  }
  return scenario.choose_dialogue_option(dialogue_id, option);
}

godot::String DrossWorldHost::get_quest_status(const godot::String& quest) const {
  const auto text = quest.utf8();
  const auto parsed =
      ContentId::parse(std::string_view{text.get_data(), static_cast<std::size_t>(text.length())});
  if (!script_state_ || !parsed) {
    return {};
  }
  switch (script_state_->quests.status(*parsed)) {
  case QuestStatus::inactive:
    return "inactive";
  case QuestStatus::active:
    return "active";
  case QuestStatus::completed:
    return "completed";
  case QuestStatus::failed:
    return "failed";
  }
  return {};
}

godot::String DrossWorldHost::get_quest_stage(const godot::String& quest) const {
  const auto text = quest.utf8();
  const auto parsed =
      ContentId::parse(std::string_view{text.get_data(), static_cast<std::size_t>(text.length())});
  if (!script_state_ || !parsed) {
    return {};
  }
  const auto stage = script_state_->quests.stage(*parsed);
  return stage ? godot::String{stage->canonical().data()} : godot::String{};
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

godot::Ref<DrossCompiledHexMap> DrossWorldHost::get_movement_compiled_map() const {
  godot::Ref<DrossCompiledHexMap> output;
  output.instantiate();
  if (!movement_state_) {
    return output;
  }
  std::map<HexCellId, CellProvenance> provenance;
  for (const auto& cell : movement_state_->map.cell_ids()) {
    provenance.emplace(cell, CellProvenance::automatic);
  }
  output->set_from_core(CompiledGridBake{
      .map = movement_state_->map,
      .provenance = std::move(provenance),
      .reasons = {},
  });
  return output;
}

godot::Ref<DrossMovementPreview>
DrossWorldHost::preview_movement(const std::int64_t destination_q,
                                 const std::int64_t destination_r) const {
  godot::Ref<DrossMovementPreview> output;
  output.instantiate();
  if (!movement_state_ || destination_q < 0 || destination_q > 3 || destination_r < -1 ||
      destination_r > 1) {
    output->initialize(false, 0, 0, {}, {});
    return output;
  }
  const auto preview = movement_state_->movement.preview(movement_pose(
      static_cast<std::int32_t>(destination_q), static_cast<std::int32_t>(destination_r)));
  godot::PackedInt32Array columns;
  godot::PackedInt32Array rows;
  for (const auto& path_pose : preview.path) {
    columns.push_back(path_pose.anchor.coord.q);
    rows.push_back(path_pose.anchor.coord.r);
  }
  output->initialize(preview.accepted, static_cast<std::int64_t>(preview.cost.value()),
                     static_cast<std::int64_t>(preview.duration_ticks), std::move(columns),
                     std::move(rows));
  return output;
}

bool DrossWorldHost::move_to(const std::int64_t destination_q, const std::int64_t destination_r) {
  return movement_state_ && destination_q >= 0 && destination_q <= 3 && destination_r >= -1 &&
         destination_r <= 1 &&
         movement_state_->movement
             .handle(movement::MoveTo{
                 .entity = movement_state_->entity,
                 .destination = movement_pose(static_cast<std::int32_t>(destination_q),
                                              static_cast<std::int32_t>(destination_r)),
             })
             .has_value();
}

bool DrossWorldHost::cancel_movement() {
  return movement_state_ && movement_state_->movement
                                .handle(movement::CancelMovement{.entity = movement_state_->entity})
                                .has_value();
}

bool DrossWorldHost::request_movement_combat() {
  if (!movement_state_ ||
      !movement_state_->mode
           .handle(combat::RequestCombatStart{
               .requester = movement_state_->entity,
               .opponent = EntityRef{WorldInstanceId{synthetic_instance}, EntityId{7, 2}},
           })
           .has_value()) {
    return false;
  }
  movement_state_->movement.request_combat_stop();
  return true;
}

bool DrossWorldHost::advance_movement_tick() {
  if (!movement_state_) {
    return false;
  }
  const auto result = movement_state_->movement.advance(movement_state_->tick);
  if ((result == MovementAdvance::combat_boundary ||
       movement_state_->movement.state() == MovementLifecycleState::idle) &&
      movement_state_->mode.state() == SimulationModeState::combat_pending) {
    static_cast<void>(movement_state_->mode.reach_safe_boundary());
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

std::int64_t DrossWorldHost::get_movement_row() const {
  return movement_state_ ? movement_state_->movement.pose().anchor.coord.r : 0;
}

std::int64_t DrossWorldHost::get_movement_presentation_to_column() const {
  if (!movement_state_) {
    return -1;
  }
  const auto snapshot = movement_state_->movement.snapshot();
  if (snapshot.state == MovementLifecycleState::traversing &&
      snapshot.next_pose < snapshot.path.size()) {
    return snapshot.path[snapshot.next_pose].anchor.coord.q;
  }
  return snapshot.pose.anchor.coord.q;
}

std::int64_t DrossWorldHost::get_movement_presentation_to_row() const {
  if (!movement_state_) {
    return 0;
  }
  const auto snapshot = movement_state_->movement.snapshot();
  if (snapshot.state == MovementLifecycleState::traversing &&
      snapshot.next_pose < snapshot.path.size()) {
    return snapshot.path[snapshot.next_pose].anchor.coord.r;
  }
  return snapshot.pose.anchor.coord.r;
}

double DrossWorldHost::get_movement_presentation_alpha() const {
  if (!movement_state_) {
    return 0.0;
  }
  const auto snapshot = movement_state_->movement.snapshot();
  return snapshot.state == MovementLifecycleState::traversing
             ? static_cast<double>(snapshot.transition_ticks) /
                   static_cast<double>(movement_transition_ticks)
             : 0.0;
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
  if (movement_state_->mode.state() == SimulationModeState::combat) {
    return "combat";
  }
  return movement_state_->mode.state() == SimulationModeState::combat_pending ? "combat_pending"
                                                                              : "exploration";
}

godot::PackedStringArray DrossWorldHost::get_recent_movement_events() const {
  godot::PackedStringArray events;
  if (movement_state_) {
    for (const auto& event : movement_state_->recent_events) {
      events.push_back(event);
    }
  }
  return events;
}

godot::String DrossWorldHost::get_canonical_capability_hash() const {
  CanonicalCapabilitySnapshot capabilities;
  if (movement_state_) {
    capabilities.movement = movement_state_->movement.snapshot();
  }
  if (combat_state_) {
    capabilities.combat = combat_state_->session.snapshot();
    capabilities.combat_actors = combat_state_->resolver.snapshot();
  }
  if (door_state_) {
    capabilities.door = door_state_->runtime.snapshot();
  }
  if (script_state_) {
    capabilities.script = script_state_->runtime.state();
    capabilities.inventory = script_state_->inventory.snapshot();
    capabilities.quest = script_state_->quests.snapshot();
    capabilities.dialogue = script_state_->dialogue.snapshot();
  }
  const auto tick = movement_state_ ? movement_state_->tick : Tick{0};
  const auto hash = canonical_capability_hash(tick, capabilities);
  constexpr std::string_view hexadecimal{"0123456789abcdef"};
  std::string text;
  text.reserve(hash.size() * 2U);
  for (const auto value : hash) {
    text.push_back(hexadecimal[value >> 4U]);
    text.push_back(hexadecimal[value & 0x0FU]);
  }
  return godot::String{text.c_str()};
}

godot::PackedByteArray DrossWorldHost::save_integrated_state() const {
  godot::PackedByteArray output;
  if (!movement_state_ || !script_state_ || !door_state_ ||
      movement_state_->movement.state() != MovementLifecycleState::idle ||
      door_state_->runtime.presentation_pending()) {
    return output;
  }
  SaveContainer container{
      .header =
          SaveHeader{
              .container_version = 1,
              .simulation_schema_version = 1,
              .engine_version = engine_version(),
              .ticks_per_second = 30,
              .current_tick = movement_state_->tick,
              .world_lineage = synthetic_lineage,
              .allocator = EntityIdAllocatorSnapshot{4},
              .map_id = ContentId::parse("dross:phase11").value(),
              .map_hash = canonical_map_hash(movement_state_->map),
          },
      .runtime =
          SaveRuntimeSnapshot{
              .random = script_state_->random.snapshot(),
              .lifecycle = WorldLifecycleSnapshot{.state = WorldLifecycleState::running},
              .mode = movement_state_->mode.snapshot(),
          },
      .content_manifest = thump_demo::content_manifest(),
      .combat = {},
      .movement =
          MovementBoundarySnapshot{
              .actor = movement_state_->entity.id(),
              .footprint = movement_state_->footprint.id().content_id(),
              .runtime = movement_state_->movement.snapshot(),
          },
      .door =
          DoorBoundarySnapshot{
              .door = door_state_->entity.id(),
              .definition = door_state_->definition_id,
              .edges = door_state_->edges,
              .runtime = door_state_->runtime.snapshot(),
          },
      .script =
          ScriptBoundarySnapshot{
              .modules = script_state_->runtime.modules(),
              .state = script_state_->runtime.state(),
          },
      .inventory = script_state_->inventory.snapshot(),
      .quest = script_state_->quests.snapshot(),
      .dialogue = script_state_->dialogue.snapshot(),
      .components = {},
  };
  if (combat_state_) {
    container.combat = CombatBoundarySnapshot{
        .ability = combat_state_->ability.id,
        .session = combat_state_->session.snapshot(),
        .actors = combat_state_->resolver.snapshot(),
    };
  }
  const auto encoded = encode_save_container(container);
  output.resize(static_cast<std::int64_t>(encoded.size()));
  for (std::size_t index = 0; index < encoded.size(); ++index) {
    output.set(static_cast<std::int64_t>(index), std::to_integer<std::uint8_t>(encoded[index]));
  }
  return output;
}

bool DrossWorldHost::restore_integrated_state(const godot::PackedByteArray& bytes) {
  const auto reject = [this](const char* message) {
    last_load_error_ = message;
    return false;
  };
  if (!movement_state_ || !script_state_ || !door_state_ || bytes.is_empty()) {
    return reject("integrated world and non-empty save bytes are required");
  }
  std::vector<std::byte> encoded(static_cast<std::size_t>(bytes.size()));
  for (std::int64_t index = 0; index < bytes.size(); ++index) {
    encoded[static_cast<std::size_t>(index)] = static_cast<std::byte>(bytes[index]);
  }
  auto decoded = decode_save_container(encoded);
  if (!decoded) {
    return reject("save container is malformed or truncated");
  }
  const auto expected_map_id = ContentId::parse("dross:phase11").value();
  if (decoded->header.container_version != 1 || decoded->header.simulation_schema_version != 1 ||
      decoded->header.ticks_per_second != 30 ||
      decoded->header.world_lineage != synthetic_lineage ||
      decoded->header.map_id != expected_map_id ||
      decoded->header.map_hash != canonical_map_hash(movement_state_->map)) {
    return reject("save header or compiled map identity does not match the running demo");
  }
  if (!validate_content_manifest(decoded->content_manifest, thump_demo::content_manifest())) {
    return reject("save content manifest does not match installed content");
  }
  if (decoded->runtime.lifecycle.state != WorldLifecycleState::running ||
      decoded->components.size() != 0 || !decoded->movement || !decoded->door || !decoded->script ||
      !decoded->inventory || !decoded->quest || !decoded->dialogue) {
    return reject("save omits a required integrated capability boundary");
  }
  if (decoded->movement->actor != movement_state_->entity.id() ||
      decoded->movement->footprint != movement_state_->footprint.id().content_id()) {
    return reject("save movement actor or footprint identity does not match");
  }
  if (decoded->door->door != door_state_->entity.id() ||
      decoded->door->definition != door_state_->definition_id ||
      decoded->door->edges != door_state_->edges) {
    return reject("save door identity or edge footprint does not match");
  }
  if (decoded->script->modules != script_state_->runtime.modules()) {
    return reject("save script modules do not match installed scripts");
  }
  if (decoded->runtime.random.master_seed != script_state_->random.snapshot().master_seed) {
    return reject("save random seed does not match the running scenario");
  }
  RandomHub validated_random{decoded->runtime.random.master_seed};
  if (!validated_random.restore(decoded->runtime.random)) {
    return reject("save random streams are invalid");
  }
  InventoryRuntime validated_inventory{WorldInstanceId{synthetic_instance}, {EntityId{7, 1}}};
  if (!validated_inventory.restore(*decoded->inventory)) {
    return reject("save inventory snapshot is invalid");
  }
  QuestRuntime validated_quests{script_state_->machine_trace};
  if (!validated_quests.restore(*decoded->quest)) {
    return reject("save quest snapshot is invalid");
  }
  DialogueRuntime validated_dialogue{
      WorldInstanceId{synthetic_instance},
      {script_state_->player.id(), script_state_->caretaker.id()},
      script_state_->machine_trace,
  };
  if (!validated_dialogue.restore(*decoded->dialogue)) {
    return reject("save dialogue snapshot is invalid");
  }

  std::unique_ptr<MovementScenarioState> next_movement;
  std::unique_ptr<DoorScenarioState> next_door;
  std::unique_ptr<CombatScenarioState> next_combat;
  try {
    next_movement = std::make_unique<MovementScenarioState>(&decoded->movement->runtime);
    for (const auto& pose : decoded->movement->runtime.path) {
      if (!next_movement->map.cell(pose.anchor)) {
        return reject("save movement path contains a cell outside the compiled map");
      }
    }
    auto footprint = EdgeFootprint::create(decoded->door->edges);
    if (!footprint) {
      return reject("save door edge footprint is invalid");
    }
    next_door = std::make_unique<DoorScenarioState>(
        CompiledDoorDefinition{
            .id = decoded->door->definition,
            .footprint = std::move(*footprint),
        },
        decoded->door->runtime.state);

    if (decoded->combat) {
      const auto player = EntityId{7, 1};
      const auto mouse = EntityId{7, 2};
      const auto valid_actor = [player, mouse](const AbilityActorSnapshot& actor) {
        return (actor.entity == player || actor.entity == mouse) && actor.health.value() >= 0;
      };
      if (decoded->combat->actors.actors.size() != 2 ||
          !std::ranges::all_of(decoded->combat->actors.actors, valid_actor) ||
          std::ranges::count(decoded->combat->actors.actors, player,
                             &AbilityActorSnapshot::entity) != 1 ||
          std::ranges::count(decoded->combat->actors.actors, mouse,
                             &AbilityActorSnapshot::entity) != 1 ||
          std::ranges::count(decoded->combat->session.combatants, player,
                             &CombatantSnapshot::entity) != 1 ||
          std::ranges::count(decoded->combat->session.combatants, mouse,
                             &CombatantSnapshot::entity) != 1) {
        return reject("save combat actors do not match the installed demo definition");
      }
      auto ability = AbilityDefinition{
          .id = thump_demo::thump_ability_id(),
          .range = 1,
          .action_point_cost = 2,
          .damage = HitPoints{3},
          .presentation_cue = thump_demo::thump_ability_id(),
      };
      next_combat = std::make_unique<CombatScenarioState>(std::move(ability), script_state_.get(),
                                                          *decoded->combat);
    }
  } catch (const std::logic_error&) {
    return reject("save capability snapshot failed deterministic reconstruction");
  }

  const auto saved_mode = decoded->runtime.mode.state;
  if ((saved_mode == SimulationModeState::combat) != static_cast<bool>(next_combat)) {
    return reject("save mode and combat capability disagree");
  }
  next_movement->tick = decoded->header.current_tick;
  if (!next_movement->mode.restore(decoded->runtime.mode)) {
    return reject("save simulation mode snapshot is invalid");
  }

  movement_state_ = std::move(next_movement);
  door_state_ = std::move(next_door);
  combat_state_ = std::move(next_combat);
  script_state_->runtime.restore_state(decoded->script->state);
  if (!script_state_->inventory.restore(*decoded->inventory)) {
    return reject("validated inventory could not be installed");
  }
  if (!script_state_->quests.restore(*decoded->quest)) {
    return reject("validated quest progress could not be installed");
  }
  if (!script_state_->dialogue.restore(*decoded->dialogue)) {
    return reject("validated dialogue session could not be installed");
  }
  if (!script_state_->random.restore(decoded->runtime.random)) {
    return reject("validated random streams could not be installed");
  }
  script_state_->tick = decoded->header.current_tick;
  last_load_error_ = godot::String{};
  return true;
}

bool DrossWorldHost::start_thump_scenario(
    const godot::Ref<DrossAbilityDefinition>& ability_definition) {
  if (ability_definition.is_null()) {
    return false;
  }
  if (movement_state_ && movement_state_->movement.state() != MovementLifecycleState::idle) {
    return false;
  }
  if (movement_state_ && movement_state_->mode.state() != SimulationModeState::combat) {
    return false;
  }
  auto ability = ability_definition->compile_core();
  if (!ability) {
    return false;
  }
  const auto player_coord =
      movement_state_ ? movement_state_->movement.pose().anchor.coord : HexCoord{.q = 0, .r = 0};
  combat_state_ =
      std::make_unique<CombatScenarioState>(std::move(*ability), script_state_.get(), player_coord);
  return true;
}

bool DrossWorldHost::perform_thump() {
  if (!combat_state_) {
    return false;
  }
  const auto result = combat_state_->resolver.perform(combat_state_->ability,
                                                      combat::PerformAbility{
                                                          .actor = combat_state_->player,
                                                          .target = combat_state_->mouse,
                                                          .ability = combat_state_->ability.id,
                                                      });
  if (!result.accepted) {
    return false;
  }
  return !combat_state_->script_fault;
}

bool DrossWorldHost::is_player_turn() const {
  return combat_state_ && !is_mouse_killed() &&
         combat_state_->session.active_actor() == combat_state_->player.id();
}

bool DrossWorldHost::end_player_turn() {
  if (!is_player_turn() || !combat_state_->session.end_turn(combat_state_->player.id())) {
    return false;
  }
  if (!is_mouse_killed() && combat_state_->session.active_actor() == combat_state_->mouse.id()) {
    return combat_state_->session.end_turn(combat_state_->mouse.id());
  }
  return true;
}

std::int64_t DrossWorldHost::get_player_action_points() const {
  return combat_state_ ? static_cast<std::int64_t>(
                             combat_state_->session.action_points(combat_state_->player.id()))
                       : -1;
}

std::int64_t DrossWorldHost::get_mouse_health() const {
  return combat_state_ ? combat_state_->resolver.health(combat_state_->mouse.id()).value() : -1;
}

bool DrossWorldHost::is_mouse_killed() const {
  return combat_state_ && combat_state_->resolver.health(combat_state_->mouse.id()).value() == 0;
}

godot::String DrossWorldHost::get_last_presentation_cue() const {
  return combat_state_ ? combat_state_->last_cue : godot::String{};
}

godot::PackedStringArray DrossWorldHost::get_recent_combat_events() const {
  godot::PackedStringArray events;
  if (combat_state_) {
    for (const auto& event : combat_state_->recent_events) {
      events.push_back(event);
    }
  }
  return events;
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

bool DrossWorldHost::open_door() {
  return door_state_ &&
         door_state_->runtime.handle(door::OpenDoor{.door = door_state_->entity}).has_value();
}

bool DrossWorldHost::close_door() {
  return door_state_ &&
         door_state_->runtime.handle(door::CloseDoor{.door = door_state_->entity}).has_value();
}
bool DrossWorldHost::is_door_open() const {
  return door_state_ && door_state_->runtime.state() == DoorState::open;
}
bool DrossWorldHost::is_door_edge_traversable() const {
  return door_state_ && door_state_->runtime.allows(door_state_->edge);
}
bool DrossWorldHost::is_door_presentation_pending() const {
  return door_state_ && door_state_->runtime.presentation_pending();
}
godot::String DrossWorldHost::get_last_door_event() const {
  return door_state_ ? door_state_->last_event : godot::String{};
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
  godot::ClassDB::bind_method(godot::D_METHOD("get_inventory_count", "owner_sequence", "item"),
                              &DrossWorldHost::get_inventory_count);
  godot::ClassDB::bind_method(godot::D_METHOD("accept_mouse_quest_dialogue"),
                              &DrossWorldHost::accept_mouse_quest_dialogue);
  godot::ClassDB::bind_method(godot::D_METHOD("hand_in_mouse_tail_dialogue"),
                              &DrossWorldHost::hand_in_mouse_tail_dialogue);
  godot::ClassDB::bind_method(godot::D_METHOD("get_quest_status", "quest"),
                              &DrossWorldHost::get_quest_status);
  godot::ClassDB::bind_method(godot::D_METHOD("get_quest_stage", "quest"),
                              &DrossWorldHost::get_quest_stage);
  godot::ClassDB::bind_method(godot::D_METHOD("save_script_state"),
                              &DrossWorldHost::save_script_state);
  godot::ClassDB::bind_method(godot::D_METHOD("restore_script_state", "bytes"),
                              &DrossWorldHost::restore_script_state);
  godot::ClassDB::bind_method(godot::D_METHOD("start_movement_scenario"),
                              &DrossWorldHost::start_movement_scenario);
  godot::ClassDB::bind_method(godot::D_METHOD("get_movement_compiled_map"),
                              &DrossWorldHost::get_movement_compiled_map);
  godot::ClassDB::bind_method(godot::D_METHOD("preview_movement", "destination_q", "destination_r"),
                              &DrossWorldHost::preview_movement);
  godot::ClassDB::bind_method(godot::D_METHOD("move_to", "destination_q", "destination_r"),
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
  godot::ClassDB::bind_method(godot::D_METHOD("get_movement_row"),
                              &DrossWorldHost::get_movement_row);
  godot::ClassDB::bind_method(godot::D_METHOD("get_movement_presentation_to_column"),
                              &DrossWorldHost::get_movement_presentation_to_column);
  godot::ClassDB::bind_method(godot::D_METHOD("get_movement_presentation_to_row"),
                              &DrossWorldHost::get_movement_presentation_to_row);
  godot::ClassDB::bind_method(godot::D_METHOD("get_movement_presentation_alpha"),
                              &DrossWorldHost::get_movement_presentation_alpha);
  godot::ClassDB::bind_method(godot::D_METHOD("get_movement_state"),
                              &DrossWorldHost::get_movement_state);
  godot::ClassDB::bind_method(godot::D_METHOD("get_movement_mode"),
                              &DrossWorldHost::get_movement_mode);
  godot::ClassDB::bind_method(godot::D_METHOD("get_recent_movement_events"),
                              &DrossWorldHost::get_recent_movement_events);
  godot::ClassDB::bind_method(godot::D_METHOD("get_canonical_capability_hash"),
                              &DrossWorldHost::get_canonical_capability_hash);
  godot::ClassDB::bind_method(godot::D_METHOD("save_integrated_state"),
                              &DrossWorldHost::save_integrated_state);
  godot::ClassDB::bind_method(godot::D_METHOD("restore_integrated_state", "bytes"),
                              &DrossWorldHost::restore_integrated_state);
  godot::ClassDB::bind_method(godot::D_METHOD("get_last_load_error"),
                              &DrossWorldHost::get_last_load_error);
  godot::ClassDB::bind_method(godot::D_METHOD("start_thump_scenario", "ability_definition"),
                              &DrossWorldHost::start_thump_scenario);
  godot::ClassDB::bind_method(godot::D_METHOD("perform_thump"), &DrossWorldHost::perform_thump);
  godot::ClassDB::bind_method(godot::D_METHOD("is_player_turn"), &DrossWorldHost::is_player_turn);
  godot::ClassDB::bind_method(godot::D_METHOD("end_player_turn"), &DrossWorldHost::end_player_turn);
  godot::ClassDB::bind_method(godot::D_METHOD("get_player_action_points"),
                              &DrossWorldHost::get_player_action_points);
  godot::ClassDB::bind_method(godot::D_METHOD("get_mouse_health"),
                              &DrossWorldHost::get_mouse_health);
  godot::ClassDB::bind_method(godot::D_METHOD("is_mouse_killed"), &DrossWorldHost::is_mouse_killed);
  godot::ClassDB::bind_method(godot::D_METHOD("get_last_presentation_cue"),
                              &DrossWorldHost::get_last_presentation_cue);
  godot::ClassDB::bind_method(godot::D_METHOD("get_recent_combat_events"),
                              &DrossWorldHost::get_recent_combat_events);
  godot::ClassDB::bind_method(godot::D_METHOD("start_door_scenario", "definition"),
                              &DrossWorldHost::start_door_scenario);
  godot::ClassDB::bind_method(godot::D_METHOD("open_door"), &DrossWorldHost::open_door);
  godot::ClassDB::bind_method(godot::D_METHOD("close_door"), &DrossWorldHost::close_door);
  godot::ClassDB::bind_method(godot::D_METHOD("is_door_open"), &DrossWorldHost::is_door_open);
  godot::ClassDB::bind_method(godot::D_METHOD("is_door_edge_traversable"),
                              &DrossWorldHost::is_door_edge_traversable);
  godot::ClassDB::bind_method(godot::D_METHOD("is_door_presentation_pending"),
                              &DrossWorldHost::is_door_presentation_pending);
  godot::ClassDB::bind_method(godot::D_METHOD("get_last_door_event"),
                              &DrossWorldHost::get_last_door_event);
  godot::ClassDB::bind_method(godot::D_METHOD("get_door_presentation_acknowledgement_id"),
                              &DrossWorldHost::get_door_presentation_acknowledgement_id);
  godot::ClassDB::bind_method(
      godot::D_METHOD("acknowledge_door_presentation", "acknowledgement_id"),
      &DrossWorldHost::acknowledge_door_presentation);
  godot::ClassDB::bind_method(godot::D_METHOD("advance_door_presentation"),
                              &DrossWorldHost::advance_door_presentation);
}

} // namespace dross::godot_adapter

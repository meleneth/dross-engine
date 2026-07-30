#include "thump_scenario.hpp"

#include "content.hpp"

#include <dross/foundation/version.hpp>
#include <dross/persistence/save_container.hpp>
#include <dross/random/random_hub.hpp>
#include <dross/runtime/combat_runtime.hpp>
#include <dross/runtime/machine_trace.hpp>
#include <dross/runtime/simulation_mode.hpp>
#include <dross/runtime/world_lifecycle.hpp>
#include <dross/world/world_storage.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

constexpr int scenario_error = 5;
constexpr std::uint64_t world_lineage = 52;
constexpr std::uint64_t initial_world_instance = 12;
constexpr std::uint64_t restored_world_instance = 13;
constexpr std::uint32_t ticks_per_second = 30;
constexpr std::int32_t player_initiative = 10;
constexpr std::int32_t mouse_initiative = 5;
constexpr std::int32_t player_health = 8;

dross::ContentId content_id(const char* value) { return dross::ContentId::parse(value).value(); }

dross::HexPose pose(const std::int32_t column) {
  return {
      .anchor =
          {
              .region = dross::RegionId{thump_demo::room_id()},
              .coord = {.q = column, .r = 0},
              .layer = 0,
          },
      .facing = dross::HexFacing::east,
  };
}

const dross::CompiledHexMap& thump_map() {
  static const auto map = [] {
    dross::CompiledHexMapBuilder builder;
    for (std::int32_t column = 0; column < 2; ++column) {
      if (!builder.add_cell(dross::CellFacts{
              .id = pose(column).anchor,
              .surface_height = dross::Millimeters{0},
              .terrain = content_id("dross:floor"),
              .base_cost = dross::MovementCost{1},
              .clearance = dross::Clearance::open,
              .traversable = true,
              .semantic_tags = {},
          })) {
        throw std::logic_error{"Thump map cell construction failed"};
      }
    }
    if (!builder.add_edge(
            pose(0).anchor, pose(1).anchor,
            dross::DirectionalEdgeFacts{.traversable = true, .cost = dross::MovementCost{1}},
            dross::DirectionalEdgeFacts{.traversable = true, .cost = dross::MovementCost{1}})) {
      throw std::logic_error{"Thump map edge construction failed"};
    }
    return std::move(builder).build().value();
  }();
  return map;
}

dross::OccupancyIndex rebuild_occupancy(const dross::WorldStorage& world) {
  std::vector<dross::OccupancyPlacement> placements;
  for (const auto entity : world.read().stable_entity_ids()) {
    const auto reference = world.read().find(entity);
    if (!reference) {
      throw std::logic_error{"Thump occupancy rebuild could not resolve entity"};
    }
    const auto entity_pose = world.read().pose(*reference);
    if (entity_pose) {
      placements.push_back(dross::OccupancyPlacement{
          .entity = entity,
          .cells = {entity_pose->anchor},
      });
    }
  }
  dross::OccupancyIndex occupancy;
  if (!occupancy.rebuild(placements)) {
    throw std::logic_error{"Thump occupancy rebuild failed"};
  }
  return occupancy;
}

class CanonicalCombatEvents final : public dross::CombatSession::EventSink,
                                    public dross::AbilityResolver::EventSink {
public:
  void publish(const dross::combat::CombatStarted& event) override {
    trace_.push_back("combat_started/" + entity_text(event.active_actor.id()));
  }

  void publish(const dross::combat::TurnStarted& event) override {
    trace_.push_back("turn_started/" + entity_text(event.actor.id()) + "/" +
                     std::to_string(event.action_points));
  }

  void publish(const dross::combat::ActionPointsSpent& event) override {
    trace_.push_back("action_points_spent/" + entity_text(event.actor.id()) + "/" +
                     std::to_string(event.amount) + "/" + std::to_string(event.remaining));
  }

  void publish(const dross::combat::AbilityCommitted& event) override {
    trace_.push_back("ability_committed/" + entity_text(event.actor.id()) + "/" +
                     entity_text(event.target.id()) + "/" + std::string{event.ability.canonical()});
  }

  void publish(const dross::combat::DamageApplied& event) override {
    trace_.push_back("damage_applied/" + entity_text(event.source.id()) + "/" +
                     entity_text(event.target.id()) + "/" + std::to_string(event.amount.value()) +
                     "/" + std::string{event.damage_type.canonical()});
  }

  void publish(const dross::combat::ActorKilled& event) override {
    trace_.push_back("actor_killed/" + entity_text(event.killer.id()) + "/" +
                     entity_text(event.target.id()) + "/" + std::string{event.ability.canonical()});
  }

  [[nodiscard]] const std::vector<std::string>& trace() const noexcept { return trace_; }

private:
  static std::string entity_text(const dross::EntityId entity) {
    return std::to_string(entity.lineage()) + "/" + std::to_string(entity.sequence());
  }

  std::vector<std::string> trace_;
};

struct SaveCheckpoint {
  std::string name;
  dross::SaveContainer save;
};

struct ScenarioResult {
  dross::ReplayLog replay;
  std::vector<SaveCheckpoint> save_checkpoints;
  dross::HitPoints mouse_health;
  bool killed;
  dross::CombatSessionSnapshot combat;
  dross::AbilityResolverSnapshot actors;
  std::vector<std::string> events;
};

enum class ResumeBoundary : std::uint8_t {
  none,
  exploration,
  combat,
};

struct ExplorationResumeTarget {
  dross::WorldInstanceId& instance;
  std::unique_ptr<dross::WorldStorage>& world;
  dross::EntityRef& player;
  dross::EntityRef& mouse;
  std::unique_ptr<dross::RandomHub>& random;
  dross::WorldLifecycle& lifecycle;
  dross::SimulationMode& mode;
};

void resume_from_exploration_save(const dross::SaveContainer& save,
                                  ExplorationResumeTarget target) {
  const auto decoded = dross::decode_save_container(dross::encode_save_container(save));
  dross::ComponentCodecRegistry registry;
  if (!decoded || decoded->combat || !dross::register_current_component_codecs(registry)) {
    throw std::logic_error{"Thump exploration-boundary save decode failed"};
  }
  const auto plan = dross::build_world_load_plan(*decoded, registry, thump_demo::room_id(),
                                                 dross::canonical_map_hash(thump_map()),
                                                 thump_demo::content_manifest());
  if (!plan) {
    throw std::logic_error{"Thump exploration-boundary load plan failed"};
  }
  target.instance = dross::WorldInstanceId{restored_world_instance};
  auto restored_world = plan->construct(target.instance);
  if (!restored_world || !target.lifecycle.restore(decoded->runtime.lifecycle) ||
      !target.mode.restore(decoded->runtime.mode)) {
    throw std::logic_error{"Thump exploration-boundary world restore failed"};
  }
  target.world = std::move(*restored_world);
  const auto restored_player = target.world->read().find(dross::EntityId{world_lineage, 1});
  const auto restored_mouse = target.world->read().find(dross::EntityId{world_lineage, 2});
  target.random = std::make_unique<dross::RandomHub>(decoded->runtime.random.master_seed);
  if (!restored_player || !restored_mouse || !target.random->restore(decoded->runtime.random)) {
    throw std::logic_error{"Thump exploration-boundary runtime restore failed"};
  }
  target.player = *restored_player;
  target.mouse = *restored_mouse;
}

ScenarioResult execute(const std::uint64_t seed, const ResumeBoundary resume_boundary) {
  auto instance = dross::WorldInstanceId{initial_world_instance};
  auto world = std::make_unique<dross::WorldStorage>(
      dross::WorldConfig{.lineage = world_lineage, .instance_id = instance});
  auto player = world->write().spawn(dross::SpawnPlan::authored(1)).value();
  auto mouse = world->write().spawn(dross::SpawnPlan::authored(2)).value();
  world->write().commit_pose(player, pose(0));
  world->write().commit_pose(mouse, pose(1));

  dross::NullMachineTrace trace;
  CanonicalCombatEvents events;
  dross::WorldLifecycle lifecycle{trace};
  dross::SimulationMode mode{trace};
  if (!lifecycle.begin_load() || !lifecycle.load_succeeded() || !lifecycle.begin_run()) {
    throw std::logic_error{"Thump scenario lifecycle setup failed"};
  }
  auto random = std::make_unique<dross::RandomHub>(dross::MasterSeed{seed});
  std::unique_ptr<dross::CombatSession> combat;
  dross::RandomStream* damage_random = nullptr;
  std::unique_ptr<dross::AbilityResolver> resolver;
  const dross::AbilityDefinition thump{
      .id = thump_demo::thump_ability_id(),
      .range = 1,
      .action_point_cost = 2,
      .damage = dross::HitPoints{3},
      .bonus_damage_max = 1,
      .presentation_cue = thump_demo::thump_ability_id(),
  };
  const auto save_checkpoint = [&](const dross::Tick tick, const bool include_combat) {
    return dross::SaveContainer{
        .header = {.container_version = 1,
                   .simulation_schema_version = 1,
                   .engine_version = dross::engine_version(),
                   .ticks_per_second = ticks_per_second,
                   .current_tick = tick,
                   .world_lineage = world_lineage,
                   .allocator = world->allocator_snapshot(),
                   .map_id = thump_demo::room_id(),
                   .map_hash = dross::canonical_map_hash(thump_map())},
        .runtime = {.random = random->snapshot(),
                    .lifecycle = lifecycle.snapshot(),
                    .mode = mode.snapshot()},
        .content_manifest = thump_demo::content_manifest(),
        .combat = include_combat ? std::optional{dross::CombatBoundarySnapshot{
                                       .ability = thump.id,
                                       .session = combat->snapshot(),
                                       .actors = resolver->snapshot(),
                                   }}
                                 : std::nullopt,
        .movement = {},
        .door = {},
        .script = {},
        .components = dross::snapshot_world_components(*world),
    };
  };
  std::vector<SaveCheckpoint> save_checkpoints;
  save_checkpoints.push_back(
      SaveCheckpoint{.name = "exploration-tick-0", .save = save_checkpoint(dross::Tick{0}, false)});

  if (resume_boundary == ResumeBoundary::exploration) {
    resume_from_exploration_save(save_checkpoints.back().save, {.instance = instance,
                                                                .world = world,
                                                                .player = player,
                                                                .mouse = mouse,
                                                                .random = random,
                                                                .lifecycle = lifecycle,
                                                                .mode = mode});
  }

  if (!mode.request_combat() || !mode.reach_safe_boundary()) {
    throw std::logic_error{"Thump scenario combat-mode setup failed"};
  }
  combat = std::make_unique<dross::CombatSession>(
      std::vector<dross::CombatantDefinition>{
          {.entity = player, .initiative = player_initiative, .maximum_action_points = 3},
          {.entity = mouse, .initiative = mouse_initiative, .maximum_action_points = 2},
      },
      &events);
  if (!combat->start()) {
    throw std::logic_error{"Thump scenario combat start failed"};
  }
  damage_random = &random->stream(dross::RandomStreamId{content_id("dross:combat_damage")});
  resolver = std::make_unique<dross::AbilityResolver>(
      *combat,
      std::vector<dross::AbilityActorState>{
          {.entity = player, .pose = pose(0), .health = dross::HitPoints{player_health}},
          {.entity = mouse, .pose = pose(1), .health = dross::HitPoints{3}},
      },
      &events, damage_random);
  auto occupancy = rebuild_occupancy(*world);
  std::vector<dross::CanonicalCheckpoint> checkpoints;
  checkpoints.push_back(dross::canonical_checkpoint(
      dross::Tick{0}, *world, occupancy, random->snapshot(), lifecycle.snapshot(), mode.snapshot(),
      std::span<const dross::PlaceEntityEnvelope>{},
      {.movement = {},
       .combat = combat->snapshot(),
       .combat_actors = resolver->snapshot(),
       .door = {},
       .script = {}}));
  save_checkpoints.push_back(
      SaveCheckpoint{.name = "combat-tick-0", .save = save_checkpoint(dross::Tick{0}, true)});

  if (resume_boundary == ResumeBoundary::combat) {
    const auto& save = save_checkpoints.back().save;
    const auto decoded = dross::decode_save_container(dross::encode_save_container(save));
    if (!decoded) {
      throw std::logic_error{"Thump combat-boundary save decode failed"};
    }
    const auto& combat_boundary = decoded->combat;
    if (!combat_boundary || combat_boundary->ability != thump.id) {
      throw std::logic_error{"Thump combat-boundary snapshot invalid"};
    }
    random = std::make_unique<dross::RandomHub>(decoded->runtime.random.master_seed);
    if (!random->restore(decoded->runtime.random)) {
      throw std::logic_error{"Thump combat-boundary random restore failed"};
    }
    auto restored_combat =
        dross::CombatSession::from_snapshot(combat_boundary->session, instance, &events);
    if (!restored_combat) {
      throw std::logic_error{"Thump combat session restore failed"};
    }
    combat = std::move(*restored_combat);
    damage_random = &random->stream(dross::RandomStreamId{content_id("dross:combat_damage")});
    auto restored_resolver = dross::AbilityResolver::from_snapshot(
        *combat, combat_boundary->actors, instance, &events, damage_random);
    if (!restored_resolver) {
      throw std::logic_error{"Thump ability state restore failed"};
    }
    resolver = std::move(*restored_resolver);
  }
  const auto result = resolver->perform(thump, player.id(), mouse.id());
  if (!result.accepted || !result.killed ||
      combat->state() != dross::CombatSessionState::completed) {
    throw std::logic_error{"Thump scenario ability resolution failed"};
  }
  checkpoints.push_back(dross::canonical_checkpoint(
      dross::Tick{1}, *world, occupancy, random->snapshot(), lifecycle.snapshot(), mode.snapshot(),
      std::span<const dross::PlaceEntityEnvelope>{},
      {.movement = {},
       .combat = combat->snapshot(),
       .combat_actors = resolver->snapshot(),
       .door = {},
       .script = {}}));
  save_checkpoints.push_back(
      SaveCheckpoint{.name = "completed-tick-1", .save = save_checkpoint(dross::Tick{1}, true)});
  return {
      .replay =
          dross::ReplayLog{
              .header =
                  {
                      .engine_version = dross::engine_version(),
                      .schema_version = 1,
                      .scenario = content_id("dross:thump_on_field_mouse"),
                      .content_manifest = thump_demo::content_manifest(),
                      .master_seed = dross::MasterSeed{seed},
                      .random_algorithm_version = dross::random_algorithm_version,
                  },
              .external_commands = {},
              .machine_trace = {},
              .canonical_events = events.trace(),
              .checkpoints = std::move(checkpoints),
          },
      .save_checkpoints = std::move(save_checkpoints),
      .mouse_health = resolver->health(mouse.id()),
      .killed = result.killed,
      .combat = combat->snapshot(),
      .actors = resolver->snapshot(),
      .events = events.trace(),
  };
}

bool write_replay(const std::string& path, const dross::ReplayLog& replay) {
  const auto bytes = dross::encode_replay(replay);
  std::ofstream output{path, std::ios::binary};
  output.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  return output.good();
}

bool write_save_checkpoints(const std::string& directory,
                            const std::vector<SaveCheckpoint>& checkpoints) {
  std::error_code error;
  if (!std::filesystem::create_directories(directory, error) && error) {
    return false;
  }
  for (const auto& checkpoint : checkpoints) {
    const auto bytes = dross::encode_save_container(checkpoint.save);
    const auto path = std::filesystem::path{directory} / (checkpoint.name + ".dross-save");
    std::ofstream output{path, std::ios::binary};
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!output.good()) {
      return false;
    }
  }
  return true;
}

} // namespace

int run_thump_scenario(const std::uint64_t seed, const std::string& record_path,
                       const std::string& save_checkpoint_directory) {
  try {
    const auto uninterrupted = execute(seed, ResumeBoundary::none);
    const auto exploration_resumed = execute(seed, ResumeBoundary::exploration);
    const auto combat_resumed = execute(seed, ResumeBoundary::combat);
    const auto exploration_divergence = dross::first_divergence(
        uninterrupted.replay.checkpoints, exploration_resumed.replay.checkpoints);
    const auto combat_divergence = dross::first_divergence(uninterrupted.replay.checkpoints,
                                                           combat_resumed.replay.checkpoints);
    const auto matches = [&](const ScenarioResult& resumed) {
      return uninterrupted.mouse_health == resumed.mouse_health &&
             uninterrupted.killed == resumed.killed && uninterrupted.combat == resumed.combat &&
             uninterrupted.actors == resumed.actors && uninterrupted.events == resumed.events;
    };
    if (exploration_divergence || combat_divergence || !matches(exploration_resumed) ||
        !matches(combat_resumed)) {
      throw std::logic_error{"resumed Thump scenario diverged from uninterrupted execution"};
    }
    if (!record_path.empty() && !write_replay(record_path, uninterrupted.replay)) {
      std::cerr << "failed to write Thump replay\n";
      return scenario_error;
    }
    if (!save_checkpoint_directory.empty() &&
        !write_save_checkpoints(save_checkpoint_directory, uninterrupted.save_checkpoints)) {
      std::cerr << "failed to write Thump save checkpoints\n";
      return scenario_error;
    }
    std::cout << "thump-on-field-mouse seed=" << seed
              << " mouse_health=" << uninterrupted.mouse_health.value()
              << " killed=" << (uninterrupted.killed ? "yes" : "no")
              << " checkpoints=" << uninterrupted.replay.checkpoints.size()
              << " exploration_resumed=match combat_resumed=match\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "Thump scenario failed: " << error.what() << '\n';
    return scenario_error;
  }
}

int verify_thump_replay(const dross::ReplayLog& recorded) {
  try {
    const auto replayed = execute(recorded.header.master_seed.value(), ResumeBoundary::none);
    const auto divergence =
        dross::first_divergence(recorded.checkpoints, replayed.replay.checkpoints);
    if (const auto event = dross::first_event_divergence(recorded.canonical_events,
                                                         replayed.replay.canonical_events)) {
      std::cerr << "replay event divergence index=" << event->index
                << " expected=" << event->expected.value_or("<missing>")
                << " actual=" << event->actual.value_or("<missing>") << '\n';
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
    std::cout << "replay verified checkpoints=" << recorded.checkpoints.size()
              << " events=" << recorded.canonical_events.size() << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "Thump replay failed: " << error.what() << '\n';
    return scenario_error;
  }
}

#pragma once

#include <dross/foundation/quantities.hpp>
#include <dross/foundation/result.hpp>
#include <dross/generated/entity_placed.hpp>
#include <dross/generated/place_entity.hpp>
#include <dross/hex/compiled_hex_map.hpp>
#include <dross/hex/occupancy.hpp>
#include <dross/identity/content_id.hpp>
#include <dross/identity/ids.hpp>
#include <dross/random/random_hub.hpp>
#include <dross/world/world_storage.hpp>

#include <compare>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace dross {

enum class CommandSource : std::uint8_t {
  player_input,
  gdscript,
  authoritative_system,
  replay,
  headless_test,
};

struct CommandMetadata {
  CommandId id;
  Tick tick;
  CommandSource source;
  CausationId causation;
  CorrelationId correlation;

  [[nodiscard]] auto operator<=>(const CommandMetadata&) const = default;
};

struct PlaceEntityEnvelope {
  CommandMetadata metadata;
  placement::PlaceEntity payload;

  [[nodiscard]] auto operator<=>(const PlaceEntityEnvelope&) const = default;
};

enum class CommandRejection : std::uint8_t {
  none,
  invalid_entity,
  invalid_target,
  occupied,
  script_rejected,
};

struct CommandResult {
  bool accepted;
  CommandRejection rejection;

  [[nodiscard]] static CommandResult accepted_result() noexcept;
  [[nodiscard]] static CommandResult rejected(CommandRejection reason) noexcept;
  [[nodiscard]] auto operator<=>(const CommandResult&) const = default;
};

enum class RegistrationError : std::uint8_t {
  duplicate_command_handler,
};

class CommandRouter {
public:
  using PlaceEntityHandler = std::function<CommandResult(const PlaceEntityEnvelope&)>;

  [[nodiscard]] Result<void, RegistrationError> register_place_entity(PlaceEntityHandler handler);
  [[nodiscard]] CommandResult dispatch(const PlaceEntityEnvelope& command) const;

private:
  PlaceEntityHandler place_entity_handler_;
};

enum class PlacementRulePhase : std::uint8_t {
  engine_invariant,
  hex_capability,
  script,
};

struct PlacementRuleContribution {
  PlacementRulePhase phase;
  bool accepted;
  std::optional<ContentId> reason;
};

class PlacementScriptPort {
public:
  virtual ~PlacementScriptPort() = default;
  [[nodiscard]] virtual PlacementRuleContribution
  contribute(const placement::PlaceEntity& command) const = 0;
};

class HeadlessPlacementScriptPort final : public PlacementScriptPort {
public:
  void reject_cell(HexCellId cell, ContentId reason);
  void random_reject_cell(HexCellId cell, RationalChance chance, RandomStream& stream,
                          ContentId reason);
  [[nodiscard]] PlacementRuleContribution
  contribute(const placement::PlaceEntity& command) const override;

private:
  std::vector<std::pair<HexCellId, ContentId>> rejections_;
  struct RandomRejection {
    HexCellId cell;
    RationalChance chance;
    RandomStream* stream;
    ContentId reason;
  };
  std::vector<RandomRejection> random_rejections_;
};

struct CommandTrace {
  CommandMetadata metadata;
  bool duplicate;
  CommandResult result;
  std::vector<PlacementRulePhase> contributions;
};

struct EventTrace {
  CommandId source_command;
  CausationId causation;
  CorrelationId correlation;
};

class TraceSink {
public:
  virtual ~TraceSink() = default;
  virtual void record(CommandTrace trace) = 0;
  virtual void record(EventTrace trace) = 0;
};

class NullTraceSink final : public TraceSink {
public:
  void record(CommandTrace) override {}
  void record(EventTrace) override {}
};

class InMemoryTraceSink final : public TraceSink {
public:
  void record(CommandTrace trace) override;
  void record(EventTrace trace) override;
  [[nodiscard]] const std::vector<CommandTrace>& commands() const noexcept;
  [[nodiscard]] const std::vector<EventTrace>& events() const noexcept;

private:
  std::vector<CommandTrace> commands_;
  std::vector<EventTrace> events_;
};

enum class EventListenerPhase : std::uint8_t {
  native_invariant,
  native_capability,
};

enum class EventRegistrationError : std::uint8_t {
  invalid_listener_phase,
  subscription_during_dispatch,
};

class CommandEventKernel;

class EventReactionContext {
public:
  void enqueue_follow_up(PlaceEntityEnvelope command);

private:
  friend class CommandEventKernel;
  explicit EventReactionContext(CommandEventKernel& kernel) : kernel_{&kernel} {}
  CommandEventKernel* kernel_;
};

class PlacementEventQueue {
public:
  using Listener = std::function<void(const placement::EntityPlaced&, EventReactionContext&)>;

  class Subscription {
  public:
    Subscription() = default;
    ~Subscription();
    Subscription(Subscription&& other) noexcept;
    Subscription& operator=(Subscription&& other) noexcept;
    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;

  private:
    friend class PlacementEventQueue;
    explicit Subscription(std::function<void()> remove);
    std::function<void()> remove_;
  };

  PlacementEventQueue();
  ~PlacementEventQueue();
  PlacementEventQueue(const PlacementEventQueue&) = delete;
  PlacementEventQueue& operator=(const PlacementEventQueue&) = delete;

  [[nodiscard]] Result<Subscription, EventRegistrationError> subscribe_invariant(Listener listener);
  [[nodiscard]] Result<Subscription, EventRegistrationError>
  subscribe_capability(Listener listener);
  [[nodiscard]] Result<Subscription, EventRegistrationError> subscribe(EventListenerPhase phase,
                                                                       Listener listener);

private:
  friend class CommandEventKernel;
  struct Impl;
  std::shared_ptr<Impl> impl_;
};

struct LastPlacementInspection {
  EntityRef entity;
  HexPose pose;
};

class CommandEventKernel {
public:
  CommandEventKernel(WorldStorage& world, CompiledHexMap map, PlacementScriptPort& scripts,
                     TraceSink& trace);
  ~CommandEventKernel();

  void enqueue(PlaceEntityEnvelope command);
  [[nodiscard]] std::vector<CommandResult> run_cycle();
  [[nodiscard]] std::size_t pending_command_count() const noexcept {
    return pending_commands_.size();
  }
  [[nodiscard]] bool command_active() const noexcept { return command_active_; }
  [[nodiscard]] const OccupancyIndex& occupancy() const noexcept { return occupancy_; }
  [[nodiscard]] PlacementEventQueue& events() noexcept { return events_; }
  [[nodiscard]] std::optional<LastPlacementInspection> last_placement() const;
  [[nodiscard]] std::string canonical_summary() const;

private:
  friend class EventReactionContext;
  struct PendingEvent;
  [[nodiscard]] CommandResult handle(const PlaceEntityEnvelope& command);
  void enqueue_follow_up(PlaceEntityEnvelope command);
  void drain_events();

  WorldStorage* world_;
  CompiledHexMap map_;
  PlacementScriptPort* scripts_;
  TraceSink* trace_;
  CommandRouter router_;
  OccupancyIndex occupancy_;
  PlacementEventQueue events_;
  PlacementEventQueue::Subscription inspection_subscription_;
  std::vector<PlaceEntityEnvelope> pending_commands_;
  std::vector<PlaceEntityEnvelope> follow_up_commands_;
  std::vector<PendingEvent> pending_events_;
  std::vector<std::pair<CommandId, CommandResult>> completed_;
  std::optional<LastPlacementInspection> last_placement_;
  bool command_active_{false};
};

} // namespace dross

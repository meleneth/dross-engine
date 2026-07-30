#pragma once

#include <dross/foundation/result.hpp>
#include <dross/generated/actor_killed.hpp>
#include <dross/generated/advance_quest.hpp>
#include <dross/generated/complete_quest.hpp>
#include <dross/generated/damage_applied.hpp>
#include <dross/generated/dialogue_option_chosen.hpp>
#include <dross/generated/entity_placed.hpp>
#include <dross/generated/grant_item.hpp>
#include <dross/generated/perform_ability.hpp>
#include <dross/generated/place_entity.hpp>
#include <dross/generated/remove_item.hpp>
#include <dross/generated/start_quest.hpp>
#include <dross/identity/content_id.hpp>
#include <dross/identity/ids.hpp>
#include <dross/random/random_hub.hpp>
#include <dross/runtime/command_event_kernel.hpp>

#include <compare>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace dross {

enum class ScriptScopeKind : std::uint8_t {
  region,
  entity,
};

struct ScriptScope {
  ScriptScopeKind kind;
  ContentId region;
  std::optional<EntityId> entity;

  [[nodiscard]] static ScriptScope for_region(ContentId region);
  [[nodiscard]] static ScriptScope for_entity(ContentId region, EntityId entity);
  [[nodiscard]] auto operator<=>(const ScriptScope&) const = default;
};

enum class ScriptModuleError : std::uint8_t {
  scope_identity_mismatch,
  duplicate_module_in_scope,
  state_schema_zero,
};

struct ScriptModule {
  ContentId module_id;
  ScriptScope scope;
  std::uint32_t state_schema_version;

  [[nodiscard]] auto operator<=>(const ScriptModule&) const = default;
};

enum class ScriptStateKeyError : std::uint8_t {
  empty,
  too_long,
  invalid_character,
};

class ScriptStateKey {
public:
  [[nodiscard]] static Result<ScriptStateKey, ScriptStateKeyError> parse(std::string value);
  [[nodiscard]] const std::string& value() const noexcept { return value_; }
  [[nodiscard]] auto operator<=>(const ScriptStateKey&) const = default;

private:
  explicit ScriptStateKey(std::string value) : value_{std::move(value)} {}
  std::string value_;
};

using ScriptStateValue = std::variant<bool, std::int64_t, ContentId, EntityId>;

struct ScriptStateAddress {
  ContentId module_id;
  ScriptScope scope;
  ScriptStateKey key;

  [[nodiscard]] auto operator<=>(const ScriptStateAddress&) const = default;
};

struct ScriptStateWrite {
  ScriptStateAddress address;
  ScriptStateValue value;
};

class ScriptStateBag {
public:
  [[nodiscard]] const ScriptStateValue* find(const ScriptStateAddress& address) const;
  void apply(const std::vector<ScriptStateWrite>& writes);
  [[nodiscard]] const std::map<ScriptStateAddress, ScriptStateValue>& values() const noexcept {
    return values_;
  }

private:
  std::map<ScriptStateAddress, ScriptStateValue> values_;
};

enum class ScriptStateDecodeError : std::uint8_t {
  invalid_format,
  invalid_scope,
  invalid_key,
  invalid_value,
};

[[nodiscard]] std::vector<std::byte> encode_script_state(const ScriptStateBag& state);
[[nodiscard]] Result<ScriptStateBag, ScriptStateDecodeError>
decode_script_state(std::span<const std::byte> bytes);

struct ScriptRuleContribution {
  bool accepted;
  std::optional<ContentId> reason;
};

enum class ScriptModeCommand : std::uint8_t {
  request_combat,
};

struct ScriptDialogueOptionQuery {
  EntityRef initiator;
  EntityRef partner;
  ContentId dialogue;
};

using ScriptQuestCommand =
    std::variant<quest::StartQuest, quest::AdvanceQuest, quest::CompleteQuest>;
using ScriptInventoryCommand = std::variant<inventory::GrantItem, inventory::RemoveItem>;

class ScriptCallbackTransaction {
public:
  ScriptCallbackTransaction(const ScriptModule& module, const ScriptStateBag& state);

  void add_rule(ScriptRuleContribution contribution);
  void submit(PlaceEntityEnvelope command);
  void request_combat();
  void submit(ScriptInventoryCommand command);
  void submit(ScriptQuestCommand command);
  void add_dialogue_option(ContentId option);
  void set_state(ScriptStateKey key, ScriptStateValue value);
  [[nodiscard]] const ScriptStateValue* state(const ScriptStateKey& key) const;

  [[nodiscard]] const std::vector<ScriptRuleContribution>& rules() const noexcept { return rules_; }
  [[nodiscard]] const std::vector<PlaceEntityEnvelope>& commands() const noexcept {
    return commands_;
  }
  [[nodiscard]] const std::vector<ScriptStateWrite>& state_writes() const noexcept {
    return state_writes_;
  }
  [[nodiscard]] const std::vector<ScriptModeCommand>& mode_commands() const noexcept {
    return mode_commands_;
  }
  [[nodiscard]] const std::vector<ScriptInventoryCommand>& inventory_commands() const noexcept {
    return inventory_commands_;
  }
  [[nodiscard]] const std::vector<ScriptQuestCommand>& quest_commands() const noexcept {
    return quest_commands_;
  }
  [[nodiscard]] const std::vector<ContentId>& dialogue_options() const noexcept {
    return dialogue_options_;
  }

private:
  const ScriptModule* module_;
  const ScriptStateBag* state_;
  std::vector<ScriptRuleContribution> rules_;
  std::vector<PlaceEntityEnvelope> commands_;
  std::vector<ScriptStateWrite> state_writes_;
  std::vector<ScriptModeCommand> mode_commands_;
  std::vector<ScriptInventoryCommand> inventory_commands_;
  std::vector<ScriptQuestCommand> quest_commands_;
  std::vector<ContentId> dialogue_options_;
};

struct ScriptCallbackError {
  ContentId module_id;
  ScriptScope scope;
  std::string callback;
  Tick tick;
  std::string message;
};

class ScriptRuntimePort {
public:
  virtual ~ScriptRuntimePort() = default;
  [[nodiscard]] virtual bool discover_callbacks(const ScriptModule& module) = 0;
  [[nodiscard]] virtual Result<void, std::string>
  contribute_placement(const ScriptModule& module, const placement::PlaceEntity& query,
                       ScriptCallbackTransaction& transaction, RandomStream& random) = 0;
  [[nodiscard]] virtual Result<void, std::string> contribute_ability(const ScriptModule&,
                                                                     const combat::PerformAbility&,
                                                                     ScriptCallbackTransaction&,
                                                                     RandomStream&) {
    return {};
  }
  [[nodiscard]] virtual Result<void, std::string>
  on_entity_placed(const ScriptModule& module, const placement::EntityPlaced& event,
                   ScriptCallbackTransaction& transaction, RandomStream& random) = 0;
  [[nodiscard]] virtual Result<void, std::string> on_damage_applied(const ScriptModule&,
                                                                    const combat::DamageApplied&,
                                                                    ScriptCallbackTransaction&,
                                                                    RandomStream&) {
    return {};
  }
  [[nodiscard]] virtual Result<void, std::string> on_actor_killed(const ScriptModule&,
                                                                  const combat::ActorKilled&,
                                                                  ScriptCallbackTransaction&,
                                                                  RandomStream&) {
    return {};
  }
  [[nodiscard]] virtual Result<void, std::string>
  on_dialogue_option_chosen(const ScriptModule&, const dialogue::DialogueOptionChosen&,
                            ScriptCallbackTransaction&, RandomStream&) {
    return {};
  }
  [[nodiscard]] virtual Result<void, std::string>
  contribute_dialogue_options(const ScriptModule&, const ScriptDialogueOptionQuery&,
                              ScriptCallbackTransaction&, RandomStream&) {
    return {};
  }
};

struct ScriptRuleResult {
  bool accepted;
  std::optional<ContentId> reason;
  std::vector<PlaceEntityEnvelope> deferred_commands;
  std::vector<ScriptModeCommand> deferred_mode_commands;
  std::vector<ScriptInventoryCommand> deferred_inventory_commands;
  std::vector<ScriptQuestCommand> deferred_quest_commands;
  std::optional<ScriptCallbackError> fault;
};

struct ScriptEventResult {
  std::vector<PlaceEntityEnvelope> deferred_commands;
  std::vector<ScriptModeCommand> deferred_mode_commands;
  std::vector<ScriptInventoryCommand> deferred_inventory_commands;
  std::vector<ScriptQuestCommand> deferred_quest_commands;
  std::optional<ScriptCallbackError> fault;
};

struct ScriptDialogueOptionResult {
  std::vector<ContentId> options;
  std::optional<ScriptCallbackError> fault;
};

class TypedScriptRuntime {
public:
  TypedScriptRuntime(ScriptRuntimePort& port, RandomHub& random) : port_{&port}, random_{&random} {}

  [[nodiscard]] Result<void, ScriptModuleError> install(ScriptModule module);
  [[nodiscard]] ScriptRuleResult contribute_placement(const placement::PlaceEntity& query,
                                                      Tick tick);
  [[nodiscard]] ScriptRuleResult contribute_ability(const combat::PerformAbility& query, Tick tick);
  [[nodiscard]] ScriptEventResult on_entity_placed(const placement::EntityPlaced& event, Tick tick);
  [[nodiscard]] ScriptEventResult on_damage_applied(const combat::DamageApplied& event, Tick tick);
  [[nodiscard]] ScriptEventResult on_actor_killed(const combat::ActorKilled& event, Tick tick);
  [[nodiscard]] ScriptEventResult
  on_dialogue_option_chosen(const dialogue::DialogueOptionChosen& event, Tick tick);
  [[nodiscard]] ScriptDialogueOptionResult
  contribute_dialogue_options(const ScriptDialogueOptionQuery& query, Tick tick);

  [[nodiscard]] const std::vector<ScriptModule>& modules() const noexcept { return modules_; }
  [[nodiscard]] const ScriptStateBag& state() const noexcept { return state_; }
  void restore_state(ScriptStateBag state) { state_ = std::move(state); }
  [[nodiscard]] bool world_faulted() const noexcept { return world_faulted_; }

private:
  [[nodiscard]] RandomStream& stream_for(const ScriptModule& module);
  void sort_modules();

  ScriptRuntimePort* port_;
  RandomHub* random_;
  std::vector<ScriptModule> modules_;
  ScriptStateBag state_;
  bool world_faulted_{false};
};

} // namespace dross

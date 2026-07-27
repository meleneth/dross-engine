#pragma once

#include <dross/foundation/result.hpp>
#include <dross/generated/entity_placed.hpp>
#include <dross/generated/place_entity.hpp>
#include <dross/identity/content_id.hpp>
#include <dross/identity/ids.hpp>
#include <dross/random/random_hub.hpp>
#include <dross/runtime/command_event_kernel.hpp>

#include <compare>
#include <cstdint>
#include <map>
#include <optional>
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

struct ScriptRuleContribution {
  bool accepted;
  std::optional<ContentId> reason;
};

class ScriptCallbackTransaction {
public:
  ScriptCallbackTransaction(const ScriptModule& module, const ScriptStateBag& state);

  void add_rule(ScriptRuleContribution contribution);
  void submit(PlaceEntityEnvelope command);
  void set_state(ScriptStateKey key, ScriptStateValue value);
  [[nodiscard]] const ScriptStateValue* state(const ScriptStateKey& key) const;

  [[nodiscard]] const std::vector<ScriptRuleContribution>& rules() const noexcept { return rules_; }
  [[nodiscard]] const std::vector<PlaceEntityEnvelope>& commands() const noexcept {
    return commands_;
  }
  [[nodiscard]] const std::vector<ScriptStateWrite>& state_writes() const noexcept {
    return state_writes_;
  }

private:
  const ScriptModule* module_;
  const ScriptStateBag* state_;
  std::vector<ScriptRuleContribution> rules_;
  std::vector<PlaceEntityEnvelope> commands_;
  std::vector<ScriptStateWrite> state_writes_;
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
  [[nodiscard]] virtual Result<void, std::string>
  on_entity_placed(const ScriptModule& module, const placement::EntityPlaced& event,
                   ScriptCallbackTransaction& transaction, RandomStream& random) = 0;
};

struct ScriptRuleResult {
  bool accepted;
  std::optional<ContentId> reason;
  std::vector<PlaceEntityEnvelope> deferred_commands;
  std::optional<ScriptCallbackError> fault;
};

struct ScriptEventResult {
  std::vector<PlaceEntityEnvelope> deferred_commands;
  std::optional<ScriptCallbackError> fault;
};

class TypedScriptRuntime {
public:
  TypedScriptRuntime(ScriptRuntimePort& port, RandomHub& random) : port_{&port}, random_{&random} {}

  [[nodiscard]] Result<void, ScriptModuleError> install(ScriptModule module);
  [[nodiscard]] ScriptRuleResult contribute_placement(const placement::PlaceEntity& query,
                                                      Tick tick);
  [[nodiscard]] ScriptEventResult on_entity_placed(const placement::EntityPlaced& event, Tick tick);

  [[nodiscard]] const std::vector<ScriptModule>& modules() const noexcept { return modules_; }
  [[nodiscard]] const ScriptStateBag& state() const noexcept { return state_; }
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

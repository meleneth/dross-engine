#pragma once

#include <dross/random/random_hub.hpp>
#include <dross/runtime/inventory_runtime.hpp>
#include <dross/runtime/quest_runtime.hpp>
#include <dross/runtime/script_runtime.hpp>

#include <godot_cpp/classes/logger.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/script.hpp>
#include <godot_cpp/classes/script_backtrace.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/typed_array.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace dross::godot_adapter {

class DrossScriptModuleDefinition final : public godot::Resource {
  GDCLASS(DrossScriptModuleDefinition, godot::Resource)

public:
  void set_module_id(const godot::String& value) { module_id_ = value; }
  [[nodiscard]] godot::String get_module_id() const { return module_id_; }
  void set_scope_kind(std::int64_t value) { scope_kind_ = value; }
  [[nodiscard]] std::int64_t get_scope_kind() const { return scope_kind_; }
  void set_entity_sequence(std::int64_t value) { entity_sequence_ = value; }
  [[nodiscard]] std::int64_t get_entity_sequence() const { return entity_sequence_; }
  void set_state_schema_version(std::int64_t value) { state_schema_version_ = value; }
  [[nodiscard]] std::int64_t get_state_schema_version() const { return state_schema_version_; }
  void set_script(const godot::Ref<godot::Script>& value) { script_ = value; }
  [[nodiscard]] godot::Ref<godot::Script> get_script() const { return script_; }

protected:
  static void _bind_methods();

private:
  godot::String module_id_;
  std::int64_t scope_kind_{0};
  std::int64_t entity_sequence_{0};
  std::int64_t state_schema_version_{1};
  godot::Ref<godot::Script> script_;
};

class DrossScriptStateApi final : public godot::RefCounted {
  GDCLASS(DrossScriptStateApi, godot::RefCounted)

public:
  [[nodiscard]] bool get_bool(const godot::String& key, bool fallback) const;
  void set_bool(const godot::String& key, bool value);
  [[nodiscard]] std::int64_t get_int(const godot::String& key, std::int64_t fallback) const;
  void set_int(const godot::String& key, std::int64_t value);
  void attach(ScriptCallbackTransaction* transaction) { transaction_ = transaction; }

protected:
  static void _bind_methods();

private:
  ScriptCallbackTransaction* transaction_{nullptr};
};

class DrossRandomApi final : public godot::RefCounted {
  GDCLASS(DrossRandomApi, godot::RefCounted)

public:
  [[nodiscard]] std::int64_t below(std::int64_t upper_exclusive);
  void attach(RandomStream* random) { random_ = random; }

protected:
  static void _bind_methods();

private:
  RandomStream* random_{nullptr};
};

class DrossCommandApi final : public godot::RefCounted {
  GDCLASS(DrossCommandApi, godot::RefCounted)

public:
  void request_combat();
  [[nodiscard]] bool grant_item(std::int64_t owner_lineage, std::int64_t owner_sequence,
                                const godot::String& item, std::int64_t count);
  [[nodiscard]] bool start_quest(const godot::String& quest, const godot::String& stage);
  [[nodiscard]] bool advance_quest(const godot::String& quest, const godot::String& expected_stage,
                                   const godot::String& next_stage);
  [[nodiscard]] bool complete_quest(const godot::String& quest,
                                    const godot::String& expected_stage);
  void attach(ScriptCallbackTransaction* transaction, WorldInstanceId world_instance) {
    transaction_ = transaction;
    world_instance_ = world_instance;
  }

protected:
  static void _bind_methods();

private:
  ScriptCallbackTransaction* transaction_{nullptr};
  WorldInstanceId world_instance_{0};
};

class DrossQueryApi final : public godot::RefCounted {
  GDCLASS(DrossQueryApi, godot::RefCounted)

public:
  [[nodiscard]] bool is_owner(std::int64_t lineage, std::int64_t sequence) const;
  [[nodiscard]] std::int64_t inventory_count(std::int64_t owner_lineage,
                                             std::int64_t owner_sequence,
                                             const godot::String& item) const;
  [[nodiscard]] bool has_item(std::int64_t owner_lineage, std::int64_t owner_sequence,
                              const godot::String& item, std::int64_t count) const;
  [[nodiscard]] godot::String quest_status(const godot::String& quest) const;
  [[nodiscard]] godot::String quest_stage(const godot::String& quest) const;
  void attach(const ScriptScope* scope, const InventoryRuntime* inventory,
              const QuestRuntime* quests, WorldInstanceId world_instance) {
    scope_ = scope;
    inventory_ = inventory;
    quests_ = quests;
    world_instance_ = world_instance;
  }

protected:
  static void _bind_methods();

private:
  const ScriptScope* scope_{nullptr};
  const InventoryRuntime* inventory_{nullptr};
  const QuestRuntime* quests_{nullptr};
  WorldInstanceId world_instance_{0};
};

class DrossScriptContext final : public godot::RefCounted {
  GDCLASS(DrossScriptContext, godot::RefCounted)

public:
  DrossScriptContext();
  [[nodiscard]] godot::Ref<DrossScriptStateApi> get_state() const { return state_; }
  [[nodiscard]] godot::Ref<DrossRandomApi> get_random() const { return random_; }
  [[nodiscard]] godot::Ref<DrossCommandApi> get_commands() const { return commands_; }
  [[nodiscard]] godot::Ref<DrossQueryApi> get_query() const { return query_; }
  [[nodiscard]] std::int64_t get_tick() const { return tick_; }
  [[nodiscard]] std::int64_t get_owner_lineage() const;
  [[nodiscard]] std::int64_t get_owner_sequence() const;
  void attach(const ScriptModule& module, ScriptCallbackTransaction& transaction,
              RandomStream& random, Tick tick, WorldInstanceId world_instance,
              const InventoryRuntime* inventory, const QuestRuntime* quests);
  void detach();

protected:
  static void _bind_methods();

private:
  const ScriptModule* module_{nullptr};
  std::int64_t tick_{0};
  godot::Ref<DrossScriptStateApi> state_;
  godot::Ref<DrossRandomApi> random_;
  godot::Ref<DrossCommandApi> commands_;
  godot::Ref<DrossQueryApi> query_;
};

class DrossCallbackLogger final : public godot::Logger {
  GDCLASS(DrossCallbackLogger, godot::Logger)

public:
  void _log_error(const godot::String& function, const godot::String& file, std::int32_t line,
                  const godot::String& code, const godot::String& rationale, bool editor_notify,
                  std::int32_t error_type,
                  const godot::TypedArray<godot::Ref<godot::ScriptBacktrace>>& backtraces) override;
  [[nodiscard]] bool faulted() const noexcept { return faulted_; }
  [[nodiscard]] std::string message() const { return message_; }

protected:
  static void _bind_methods() {}

private:
  bool faulted_{false};
  std::string message_;
};

class GodotScriptRuntime final : public ScriptRuntimePort {
public:
  [[nodiscard]] Result<ScriptModule, std::string>
  add_definition(const godot::Ref<DrossScriptModuleDefinition>& definition);
  [[nodiscard]] bool discover_callbacks(const ScriptModule& module) override;
  [[nodiscard]] Result<void, std::string>
  contribute_placement(const ScriptModule& module, const placement::PlaceEntity& query,
                       ScriptCallbackTransaction& transaction, RandomStream& random) override;
  [[nodiscard]] Result<void, std::string> contribute_ability(const ScriptModule& module,
                                                             const combat::PerformAbility& query,
                                                             ScriptCallbackTransaction& transaction,
                                                             RandomStream& random) override;
  [[nodiscard]] Result<void, std::string> on_entity_placed(const ScriptModule& module,
                                                           const placement::EntityPlaced& event,
                                                           ScriptCallbackTransaction& transaction,
                                                           RandomStream& random) override;
  [[nodiscard]] Result<void, std::string> on_damage_applied(const ScriptModule& module,
                                                            const combat::DamageApplied& event,
                                                            ScriptCallbackTransaction& transaction,
                                                            RandomStream& random) override;
  [[nodiscard]] Result<void, std::string> on_actor_killed(const ScriptModule& module,
                                                          const combat::ActorKilled& event,
                                                          ScriptCallbackTransaction& transaction,
                                                          RandomStream& random) override;
  void set_tick(Tick tick) { tick_ = tick; }
  void set_world_instance(WorldInstanceId value) { world_instance_ = value; }
  void set_inventory(const InventoryRuntime* value) { inventory_ = value; }
  void set_quests(const QuestRuntime* value) { quests_ = value; }
  [[nodiscard]] const std::vector<std::string>& calls() const noexcept { return calls_; }

private:
  struct Installed {
    godot::Ref<godot::RefCounted> instance;
    bool placement{false};
    bool ability{false};
    bool entity_placed{false};
    bool damage_applied{false};
    bool actor_killed{false};
  };
  [[nodiscard]] Installed* find(const ScriptModule& module);
  [[nodiscard]] Result<void, std::string>
  invoke(const ScriptModule& module, const godot::StringName& callback, const godot::Array& args,
         ScriptCallbackTransaction& transaction, RandomStream& random);

  std::map<std::pair<ScriptScope, ContentId>, Installed> installed_;
  std::vector<std::string> calls_;
  Tick tick_{0};
  WorldInstanceId world_instance_{0};
  const InventoryRuntime* inventory_{nullptr};
  const QuestRuntime* quests_{nullptr};
};

} // namespace dross::godot_adapter

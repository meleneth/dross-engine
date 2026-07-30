#include "dross_script_runtime.hpp"

#include <dross/generated/godot_api.hpp>

#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>

#include <limits>
#include <string_view>
#include <utility>

namespace dross::godot_adapter {
namespace {

std::string utf8(const godot::String& value) {
  const auto converted = value.utf8();
  return {converted.get_data(), static_cast<std::size_t>(converted.length())};
}

std::optional<ScriptStateKey> state_key(const godot::String& value) {
  auto parsed = ScriptStateKey::parse(utf8(value));
  return parsed ? std::optional<ScriptStateKey>{std::move(*parsed)} : std::nullopt;
}

} // namespace

void DrossScriptModuleDefinition::_bind_methods() {
  godot::ClassDB::bind_method(godot::D_METHOD("set_module_id", "value"),
                              &DrossScriptModuleDefinition::set_module_id);
  godot::ClassDB::bind_method(godot::D_METHOD("get_module_id"),
                              &DrossScriptModuleDefinition::get_module_id);
  godot::ClassDB::bind_method(godot::D_METHOD("set_scope_kind", "value"),
                              &DrossScriptModuleDefinition::set_scope_kind);
  godot::ClassDB::bind_method(godot::D_METHOD("get_scope_kind"),
                              &DrossScriptModuleDefinition::get_scope_kind);
  godot::ClassDB::bind_method(godot::D_METHOD("set_entity_sequence", "value"),
                              &DrossScriptModuleDefinition::set_entity_sequence);
  godot::ClassDB::bind_method(godot::D_METHOD("get_entity_sequence"),
                              &DrossScriptModuleDefinition::get_entity_sequence);
  godot::ClassDB::bind_method(godot::D_METHOD("set_state_schema_version", "value"),
                              &DrossScriptModuleDefinition::set_state_schema_version);
  godot::ClassDB::bind_method(godot::D_METHOD("get_state_schema_version"),
                              &DrossScriptModuleDefinition::get_state_schema_version);
  godot::ClassDB::bind_method(godot::D_METHOD("set_script", "value"),
                              &DrossScriptModuleDefinition::set_script);
  godot::ClassDB::bind_method(godot::D_METHOD("get_script"),
                              &DrossScriptModuleDefinition::get_script);
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::STRING, "module_id"), "set_module_id",
               "get_module_id");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "scope_kind"), "set_scope_kind",
               "get_scope_kind");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "entity_sequence"), "set_entity_sequence",
               "get_entity_sequence");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "state_schema_version"),
               "set_state_schema_version", "get_state_schema_version");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::OBJECT, "script",
                                   godot::PROPERTY_HINT_RESOURCE_TYPE, "Script"),
               "set_script", "get_script");
}

bool DrossScriptStateApi::get_bool(const godot::String& key, const bool fallback) const {
  const auto parsed = state_key(key);
  if (!transaction_ || !parsed) {
    return fallback;
  }
  const auto* value = transaction_->state(*parsed);
  return value && std::holds_alternative<bool>(*value) ? std::get<bool>(*value) : fallback;
}

void DrossScriptStateApi::set_bool(const godot::String& key, const bool value) {
  const auto parsed = state_key(key);
  if (transaction_ && parsed) {
    transaction_->set_state(*parsed, value);
  }
}

std::int64_t DrossScriptStateApi::get_int(const godot::String& key,
                                          const std::int64_t fallback) const {
  const auto parsed = state_key(key);
  if (!transaction_ || !parsed) {
    return fallback;
  }
  const auto* value = transaction_->state(*parsed);
  return value && std::holds_alternative<std::int64_t>(*value) ? std::get<std::int64_t>(*value)
                                                               : fallback;
}

void DrossScriptStateApi::set_int(const godot::String& key, const std::int64_t value) {
  const auto parsed = state_key(key);
  if (transaction_ && parsed) {
    transaction_->set_state(*parsed, value);
  }
}

void DrossScriptStateApi::_bind_methods() {
  godot::ClassDB::bind_method(godot::D_METHOD("get_bool", "key", "fallback"),
                              &DrossScriptStateApi::get_bool);
  godot::ClassDB::bind_method(godot::D_METHOD("set_bool", "key", "value"),
                              &DrossScriptStateApi::set_bool);
  godot::ClassDB::bind_method(godot::D_METHOD("get_int", "key", "fallback"),
                              &DrossScriptStateApi::get_int);
  godot::ClassDB::bind_method(godot::D_METHOD("set_int", "key", "value"),
                              &DrossScriptStateApi::set_int);
}

std::int64_t DrossRandomApi::below(const std::int64_t upper_exclusive) {
  if (!random_ || upper_exclusive <= 0) {
    return -1;
  }
  return static_cast<std::int64_t>(
      random_->bounded_u64(static_cast<std::uint64_t>(upper_exclusive)).value());
}

void DrossRandomApi::_bind_methods() {
  godot::ClassDB::bind_method(godot::D_METHOD("below", "upper_exclusive"), &DrossRandomApi::below);
}

void DrossCommandApi::request_combat() {
  if (transaction_) {
    transaction_->request_combat();
  }
}

bool DrossCommandApi::grant_item(const std::int64_t owner_lineage,
                                 const std::int64_t owner_sequence, const godot::String& item,
                                 const std::int64_t count) {
  auto parsed = ContentId::parse(utf8(item));
  if (!transaction_ || !parsed || owner_lineage < 0 || owner_sequence <= 0 || count <= 0 ||
      count > std::numeric_limits<std::uint32_t>::max()) {
    return false;
  }
  transaction_->submit(inventory::GrantItem{
      .owner = EntityRef{world_instance_, EntityId{static_cast<std::uint64_t>(owner_lineage),
                                                   static_cast<std::uint64_t>(owner_sequence)}},
      .item = *std::move(parsed),
      .count = static_cast<std::uint32_t>(count),
  });
  return true;
}

void DrossCommandApi::_bind_methods() {
  godot::ClassDB::bind_method(godot::D_METHOD("request_combat"), &DrossCommandApi::request_combat);
  godot::ClassDB::bind_method(
      godot::D_METHOD("grant_item", "owner_lineage", "owner_sequence", "item", "count"),
      &DrossCommandApi::grant_item);
}

bool DrossQueryApi::is_owner(const std::int64_t lineage, const std::int64_t sequence) const {
  return scope_ && lineage >= 0 && sequence >= 0 && scope_->entity &&
         *scope_->entity ==
             EntityId{static_cast<std::uint64_t>(lineage), static_cast<std::uint64_t>(sequence)};
}

std::int64_t DrossQueryApi::inventory_count(const std::int64_t owner_lineage,
                                            const std::int64_t owner_sequence,
                                            const godot::String& item) const {
  auto parsed = ContentId::parse(utf8(item));
  if (!inventory_ || !parsed || owner_lineage < 0 || owner_sequence <= 0) {
    return 0;
  }
  return static_cast<std::int64_t>(inventory_->count(
      EntityRef{world_instance_, EntityId{static_cast<std::uint64_t>(owner_lineage),
                                          static_cast<std::uint64_t>(owner_sequence)}},
      *parsed));
}

bool DrossQueryApi::has_item(const std::int64_t owner_lineage, const std::int64_t owner_sequence,
                             const godot::String& item, const std::int64_t count) const {
  auto parsed = ContentId::parse(utf8(item));
  if (!inventory_ || !parsed || owner_lineage < 0 || owner_sequence <= 0 || count <= 0 ||
      count > std::numeric_limits<std::uint32_t>::max()) {
    return false;
  }
  return inventory_->has(
      EntityRef{world_instance_, EntityId{static_cast<std::uint64_t>(owner_lineage),
                                          static_cast<std::uint64_t>(owner_sequence)}},
      *parsed, static_cast<std::uint32_t>(count));
}

godot::String DrossQueryApi::quest_status(const godot::String& quest) const {
  const auto parsed = ContentId::parse(utf8(quest));
  if (!quests_ || !parsed) {
    return {};
  }
  switch (quests_->status(*parsed)) {
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

godot::String DrossQueryApi::quest_stage(const godot::String& quest) const {
  const auto parsed = ContentId::parse(utf8(quest));
  if (!quests_ || !parsed) {
    return {};
  }
  const auto stage = quests_->stage(*parsed);
  return stage ? godot::String{stage->canonical().data()} : godot::String{};
}

void DrossQueryApi::_bind_methods() {
  godot::ClassDB::bind_method(godot::D_METHOD("is_owner", "lineage", "sequence"),
                              &DrossQueryApi::is_owner);
  godot::ClassDB::bind_method(
      godot::D_METHOD("inventory_count", "owner_lineage", "owner_sequence", "item"),
      &DrossQueryApi::inventory_count);
  godot::ClassDB::bind_method(
      godot::D_METHOD("has_item", "owner_lineage", "owner_sequence", "item", "count"),
      &DrossQueryApi::has_item);
  godot::ClassDB::bind_method(godot::D_METHOD("quest_status", "quest"),
                              &DrossQueryApi::quest_status);
  godot::ClassDB::bind_method(godot::D_METHOD("quest_stage", "quest"), &DrossQueryApi::quest_stage);
}

DrossScriptContext::DrossScriptContext() {
  state_.instantiate();
  random_.instantiate();
  commands_.instantiate();
  query_.instantiate();
}

std::int64_t DrossScriptContext::get_owner_lineage() const {
  return module_ && module_->scope.entity
             ? static_cast<std::int64_t>(module_->scope.entity->lineage())
             : -1;
}

std::int64_t DrossScriptContext::get_owner_sequence() const {
  return module_ && module_->scope.entity
             ? static_cast<std::int64_t>(module_->scope.entity->sequence())
             : -1;
}

void DrossScriptContext::attach(const ScriptModule& module, ScriptCallbackTransaction& transaction,
                                RandomStream& random, const Tick tick,
                                const WorldInstanceId world_instance,
                                const InventoryRuntime* inventory, const QuestRuntime* quests) {
  module_ = &module;
  tick_ = static_cast<std::int64_t>(tick.value());
  state_->attach(&transaction);
  random_->attach(&random);
  commands_->attach(&transaction, world_instance);
  query_->attach(&module.scope, inventory, quests, world_instance);
}

void DrossScriptContext::detach() {
  state_->attach(nullptr);
  random_->attach(nullptr);
  commands_->attach(nullptr, WorldInstanceId{0});
  query_->attach(nullptr, nullptr, nullptr, WorldInstanceId{0});
  module_ = nullptr;
}

void DrossScriptContext::_bind_methods() {
  godot::ClassDB::bind_method(godot::D_METHOD("get_state"), &DrossScriptContext::get_state);
  godot::ClassDB::bind_method(godot::D_METHOD("get_random"), &DrossScriptContext::get_random);
  godot::ClassDB::bind_method(godot::D_METHOD("get_commands"), &DrossScriptContext::get_commands);
  godot::ClassDB::bind_method(godot::D_METHOD("get_query"), &DrossScriptContext::get_query);
  godot::ClassDB::bind_method(godot::D_METHOD("get_tick"), &DrossScriptContext::get_tick);
  godot::ClassDB::bind_method(godot::D_METHOD("get_owner_lineage"),
                              &DrossScriptContext::get_owner_lineage);
  godot::ClassDB::bind_method(godot::D_METHOD("get_owner_sequence"),
                              &DrossScriptContext::get_owner_sequence);
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::OBJECT, "state"), "", "get_state");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::OBJECT, "random"), "", "get_random");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::OBJECT, "commands"), "", "get_commands");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::OBJECT, "query"), "", "get_query");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "tick"), "", "get_tick");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "owner_lineage"), "", "get_owner_lineage");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "owner_sequence"), "",
               "get_owner_sequence");
}

void DrossCallbackLogger::_log_error(const godot::String& function, const godot::String& file,
                                     const std::int32_t line, const godot::String& code,
                                     const godot::String& rationale, const bool,
                                     const std::int32_t error_type,
                                     const godot::TypedArray<godot::Ref<godot::ScriptBacktrace>>&) {
  if (error_type != godot::Logger::ERROR_TYPE_SCRIPT) {
    return;
  }
  faulted_ = true;
  message_ = utf8(file) + ":" + std::to_string(line) + " " + utf8(function) + " " + utf8(code) +
             " " + utf8(rationale);
}

Result<ScriptModule, std::string>
GodotScriptRuntime::add_definition(const godot::Ref<DrossScriptModuleDefinition>& definition) {
  if (definition.is_null() || definition->get_script().is_null() ||
      !definition->get_script()->can_instantiate()) {
    return tl::unexpected{std::string{"script definition is not instantiable"}};
  }
  auto module_id = ContentId::parse(utf8(definition->get_module_id()));
  if (!module_id || definition->get_state_schema_version() <= 0 ||
      definition->get_state_schema_version() > std::numeric_limits<std::uint32_t>::max()) {
    return tl::unexpected{std::string{"invalid script definition identity or schema"}};
  }
  const auto region = ContentId::parse("demo:region").value();
  std::optional<ScriptScope> scope;
  if (definition->get_scope_kind() == 0) {
    scope = ScriptScope::for_region(region);
  } else if (definition->get_scope_kind() == 1 && definition->get_entity_sequence() > 0) {
    scope = ScriptScope::for_entity(
        region, EntityId{7, static_cast<std::uint64_t>(definition->get_entity_sequence())});
  } else {
    return tl::unexpected{std::string{"invalid script scope"}};
  }
  ScriptModule module{.module_id = std::move(*module_id),
                      .scope = std::move(*scope),
                      .state_schema_version =
                          static_cast<std::uint32_t>(definition->get_state_schema_version())};
  godot::Ref<godot::RefCounted> instance;
  instance.instantiate();
  instance->set_script(definition->get_script());
  installed_.insert_or_assign(std::pair{module.scope, module.module_id},
                              Installed{.instance = std::move(instance)});
  return module;
}

GodotScriptRuntime::Installed* GodotScriptRuntime::find(const ScriptModule& module) {
  const auto found = installed_.find(std::pair{module.scope, module.module_id});
  return found == installed_.end() ? nullptr : &found->second;
}

bool GodotScriptRuntime::discover_callbacks(const ScriptModule& module) {
  auto* installed = find(module);
  if (!installed) {
    return false;
  }
  installed->placement = installed->instance->has_method("contribute_placement_rules");
  installed->ability = installed->instance->has_method("contribute_ability_rules");
  installed->entity_placed = installed->instance->has_method("on_entity_placed");
  installed->damage_applied = installed->instance->has_method("on_damage_applied");
  installed->actor_killed = installed->instance->has_method("on_actor_killed");
  return installed->placement || installed->ability || installed->entity_placed ||
         installed->damage_applied || installed->actor_killed;
}

Result<void, std::string> GodotScriptRuntime::invoke(const ScriptModule& module,
                                                     const godot::StringName& callback,
                                                     const godot::Array& args,
                                                     ScriptCallbackTransaction& transaction,
                                                     RandomStream& random) {
  auto* installed = find(module);
  if (!installed) {
    return tl::unexpected{std::string{"script instance missing"}};
  }
  godot::Ref<DrossScriptContext> context;
  context.instantiate();
  context->attach(module, transaction, random, tick_, world_instance_, inventory_, quests_);
  auto call_args = args;
  call_args.push_back(context);
  godot::Ref<DrossCallbackLogger> logger;
  logger.instantiate();
  godot::OS::get_singleton()->add_logger(logger);
  installed->instance->callv(callback, call_args);
  godot::OS::get_singleton()->remove_logger(logger);
  context->detach();
  calls_.push_back(std::string{module.module_id.canonical()} + ":" + utf8(godot::String{callback}));
  if (logger->faulted()) {
    return tl::unexpected{logger->message()};
  }
  return {};
}

Result<void, std::string>
GodotScriptRuntime::contribute_placement(const ScriptModule& module, const placement::PlaceEntity&,
                                         ScriptCallbackTransaction& transaction,
                                         RandomStream& random) {
  auto* installed = find(module);
  if (!installed || !installed->placement) {
    return {};
  }
  godot::Ref<generated::godot_api::DrossPlacementRuleQuery> query;
  query.instantiate();
  godot::Array args;
  args.push_back(query);
  auto result = invoke(module, "contribute_placement_rules", args, transaction, random);
  if (result && !query->is_accepted()) {
    transaction.add_rule(ScriptRuleContribution{.accepted = false, .reason = query->core_reason()});
  }
  return result;
}

Result<void, std::string>
GodotScriptRuntime::contribute_ability(const ScriptModule& module, const combat::PerformAbility&,
                                       ScriptCallbackTransaction& transaction,
                                       RandomStream& random) {
  auto* installed = find(module);
  if (!installed || !installed->ability) {
    return {};
  }
  godot::Ref<generated::godot_api::DrossAbilityRuleQuery> query;
  query.instantiate();
  godot::Array args;
  args.push_back(query);
  auto result = invoke(module, "contribute_ability_rules", args, transaction, random);
  if (result && !query->is_accepted()) {
    transaction.add_rule(ScriptRuleContribution{.accepted = false, .reason = query->core_reason()});
  }
  return result;
}

Result<void, std::string>
GodotScriptRuntime::on_entity_placed(const ScriptModule& module,
                                     const placement::EntityPlaced& event,
                                     ScriptCallbackTransaction& transaction, RandomStream& random) {
  auto* installed = find(module);
  if (!installed || !installed->entity_placed) {
    return {};
  }
  godot::Array args;
  args.push_back(generated::godot_api::DrossEntityPlacedEvent::from_core(event));
  return invoke(module, "on_entity_placed", args, transaction, random);
}

Result<void, std::string> GodotScriptRuntime::on_damage_applied(
    const ScriptModule& module, const combat::DamageApplied& event,
    ScriptCallbackTransaction& transaction, RandomStream& random) {
  auto* installed = find(module);
  if (!installed || !installed->damage_applied) {
    return {};
  }
  godot::Array args;
  args.push_back(generated::godot_api::DrossDamageAppliedEvent::from_core(event));
  return invoke(module, "on_damage_applied", args, transaction, random);
}

Result<void, std::string>
GodotScriptRuntime::on_actor_killed(const ScriptModule& module, const combat::ActorKilled& event,
                                    ScriptCallbackTransaction& transaction, RandomStream& random) {
  auto* installed = find(module);
  if (!installed || !installed->actor_killed) {
    return {};
  }
  godot::Array args;
  args.push_back(generated::godot_api::DrossActorKilledEvent::from_core(event));
  return invoke(module, "on_actor_killed", args, transaction, random);
}

} // namespace dross::godot_adapter

#include <dross/runtime/script_runtime.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

namespace dross {

ScriptScope ScriptScope::for_region(ContentId region) {
  return ScriptScope{.kind = ScriptScopeKind::region, .region = std::move(region), .entity = {}};
}

ScriptScope ScriptScope::for_entity(ContentId region, const EntityId entity) {
  return ScriptScope{
      .kind = ScriptScopeKind::entity, .region = std::move(region), .entity = entity};
}

Result<ScriptStateKey, ScriptStateKeyError> ScriptStateKey::parse(std::string value) {
  if (value.empty()) {
    return tl::unexpected{ScriptStateKeyError::empty};
  }
  if (value.size() > 64) {
    return tl::unexpected{ScriptStateKeyError::too_long};
  }
  if (!std::ranges::all_of(value,
                           [](const unsigned char character) {
                             return std::islower(character) != 0 || std::isdigit(character) != 0 ||
                                    character == '_';
                           }) ||
      std::islower(static_cast<unsigned char>(value.front())) == 0) {
    return tl::unexpected{ScriptStateKeyError::invalid_character};
  }
  return ScriptStateKey{std::move(value)};
}

const ScriptStateValue* ScriptStateBag::find(const ScriptStateAddress& address) const {
  const auto found = values_.find(address);
  return found == values_.end() ? nullptr : &found->second;
}

void ScriptStateBag::apply(const std::vector<ScriptStateWrite>& writes) {
  for (const auto& write : writes) {
    values_.insert_or_assign(write.address, write.value);
  }
}

ScriptCallbackTransaction::ScriptCallbackTransaction(const ScriptModule& module,
                                                     const ScriptStateBag& state)
    : module_{&module}, state_{&state} {}

void ScriptCallbackTransaction::add_rule(ScriptRuleContribution contribution) {
  rules_.push_back(std::move(contribution));
}

void ScriptCallbackTransaction::submit(PlaceEntityEnvelope command) {
  commands_.push_back(std::move(command));
}

void ScriptCallbackTransaction::set_state(ScriptStateKey key, ScriptStateValue value) {
  state_writes_.push_back(ScriptStateWrite{
      .address =
          ScriptStateAddress{
              .module_id = module_->module_id,
              .scope = module_->scope,
              .key = std::move(key),
          },
      .value = std::move(value),
  });
}

const ScriptStateValue* ScriptCallbackTransaction::state(const ScriptStateKey& key) const {
  for (auto write = state_writes_.rbegin(); write != state_writes_.rend(); ++write) {
    if (write->address.key == key) {
      return &write->value;
    }
  }
  return state_->find(ScriptStateAddress{
      .module_id = module_->module_id,
      .scope = module_->scope,
      .key = key,
  });
}

Result<void, ScriptModuleError> TypedScriptRuntime::install(ScriptModule module) {
  if (module.state_schema_version == 0) {
    return tl::unexpected{ScriptModuleError::state_schema_zero};
  }
  const bool valid_scope =
      (module.scope.kind == ScriptScopeKind::region && !module.scope.entity.has_value()) ||
      (module.scope.kind == ScriptScopeKind::entity && module.scope.entity.has_value());
  if (!valid_scope) {
    return tl::unexpected{ScriptModuleError::scope_identity_mismatch};
  }
  if (std::ranges::any_of(modules_, [&module](const ScriptModule& installed) {
        return installed.module_id == module.module_id && installed.scope == module.scope;
      })) {
    return tl::unexpected{ScriptModuleError::duplicate_module_in_scope};
  }
  if (!port_->discover_callbacks(module)) {
    return tl::unexpected{ScriptModuleError::scope_identity_mismatch};
  }
  modules_.push_back(std::move(module));
  sort_modules();
  return {};
}

ScriptRuleResult TypedScriptRuntime::contribute_placement(const placement::PlaceEntity& query,
                                                          const Tick tick) {
  ScriptRuleResult result{.accepted = true, .reason = {}, .deferred_commands = {}, .fault = {}};
  std::vector<ScriptStateWrite> pending_state;
  std::vector<PlaceEntityEnvelope> pending_commands;
  for (const auto& module : modules_) {
    ScriptCallbackTransaction transaction{module, state_};
    auto callback = port_->contribute_placement(module, query, transaction, stream_for(module));
    if (!callback) {
      result.accepted = false;
      result.fault = ScriptCallbackError{.module_id = module.module_id,
                                         .scope = module.scope,
                                         .callback = "contribute_placement",
                                         .tick = tick,
                                         .message = callback.error()};
      return result;
    }
    for (const auto& contribution : transaction.rules()) {
      if (!contribution.accepted) {
        result.accepted = false;
        result.reason = contribution.reason;
      }
    }
    pending_state.insert(pending_state.end(), transaction.state_writes().begin(),
                         transaction.state_writes().end());
    pending_commands.insert(pending_commands.end(), transaction.commands().begin(),
                            transaction.commands().end());
    if (!result.accepted) {
      return result;
    }
  }
  state_.apply(pending_state);
  result.deferred_commands = std::move(pending_commands);
  return result;
}

ScriptEventResult TypedScriptRuntime::on_entity_placed(const placement::EntityPlaced& event,
                                                       const Tick tick) {
  ScriptEventResult result;
  for (const auto& module : modules_) {
    ScriptCallbackTransaction transaction{module, state_};
    auto callback = port_->on_entity_placed(module, event, transaction, stream_for(module));
    if (!callback) {
      world_faulted_ = true;
      result.fault = ScriptCallbackError{.module_id = module.module_id,
                                         .scope = module.scope,
                                         .callback = "on_entity_placed",
                                         .tick = tick,
                                         .message = callback.error()};
      return result;
    }
    state_.apply(transaction.state_writes());
    result.deferred_commands.insert(result.deferred_commands.end(), transaction.commands().begin(),
                                    transaction.commands().end());
  }
  return result;
}

RandomStream& TypedScriptRuntime::stream_for(const ScriptModule& module) {
  return random_->stream(
      script_child_stream_id(module.module_id, module.scope.entity.value_or(EntityId{0, 0})));
}

void TypedScriptRuntime::sort_modules() {
  std::ranges::sort(modules_, [](const ScriptModule& left, const ScriptModule& right) {
    if (left.scope.kind != right.scope.kind) {
      return left.scope.kind < right.scope.kind;
    }
    if (left.scope.entity != right.scope.entity) {
      return left.scope.entity < right.scope.entity;
    }
    return left.module_id < right.module_id;
  });
}

} // namespace dross

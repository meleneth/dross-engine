#include <dross/runtime/script_runtime.hpp>

#include <dross/foundation/byte_codec.hpp>

#include <algorithm>
#include <bit>
#include <cctype>
#include <ranges>
#include <string>
#include <utility>

namespace dross {
namespace {

constexpr std::uint32_t script_state_magic = 0x53535244U;
constexpr std::uint16_t script_state_version = 1;
constexpr std::size_t maximum_script_state_key_length = 64;

enum class DurableValueTag : std::uint8_t {
  boolean = 1,
  integer = 2,
  content_id = 3,
  entity_id = 4,
};

Result<ScriptStateValue, ScriptStateDecodeError> decode_durable_value(ByteReader& reader) {
  const auto tag = reader.read_u16();
  if (!tag) {
    return tl::unexpected{ScriptStateDecodeError::invalid_value};
  }
  switch (static_cast<DurableValueTag>(*tag)) {
  case DurableValueTag::boolean: {
    const auto decoded = reader.read_u16();
    if (!decoded || *decoded > 1U) {
      return tl::unexpected{ScriptStateDecodeError::invalid_value};
    }
    return ScriptStateValue{*decoded == 1U};
  }
  case DurableValueTag::integer: {
    const auto decoded = reader.read_u64();
    if (!decoded) {
      return tl::unexpected{ScriptStateDecodeError::invalid_value};
    }
    return ScriptStateValue{std::bit_cast<std::int64_t>(*decoded)};
  }
  case DurableValueTag::content_id: {
    auto decoded = reader.read_content_id();
    if (!decoded) {
      return tl::unexpected{ScriptStateDecodeError::invalid_value};
    }
    return ScriptStateValue{*std::move(decoded)};
  }
  case DurableValueTag::entity_id: {
    const auto decoded = reader.read_entity_id();
    if (!decoded) {
      return tl::unexpected{ScriptStateDecodeError::invalid_value};
    }
    return ScriptStateValue{*decoded};
  }
  default:
    return tl::unexpected{ScriptStateDecodeError::invalid_value};
  }
}

Result<ScriptStateWrite, ScriptStateDecodeError> decode_script_state_write(ByteReader& reader) {
  auto module = reader.read_content_id();
  const auto kind = reader.read_u16();
  auto region = reader.read_content_id();
  const auto has_entity = reader.read_u16();
  if (!module || !kind || !region || !has_entity || *kind > 1U || *has_entity > 1U) {
    return tl::unexpected{ScriptStateDecodeError::invalid_scope};
  }
  std::optional<EntityId> entity;
  if (*has_entity == 1U) {
    const auto decoded = reader.read_entity_id();
    if (!decoded) {
      return tl::unexpected{ScriptStateDecodeError::invalid_scope};
    }
    entity = *decoded;
  }
  const auto scope_kind = static_cast<ScriptScopeKind>(*kind);
  if ((scope_kind == ScriptScopeKind::region && entity) ||
      (scope_kind == ScriptScopeKind::entity && !entity)) {
    return tl::unexpected{ScriptStateDecodeError::invalid_scope};
  }
  auto key_text = reader.read_string();
  if (!key_text) {
    return tl::unexpected{ScriptStateDecodeError::invalid_key};
  }
  auto key = ScriptStateKey::parse(std::move(*key_text));
  if (!key) {
    return tl::unexpected{ScriptStateDecodeError::invalid_key};
  }
  auto value = decode_durable_value(reader);
  if (!value) {
    return tl::unexpected{value.error()};
  }
  return ScriptStateWrite{
      .address =
          {
              .module_id = *std::move(module),
              .scope =
                  {
                      .kind = scope_kind,
                      .region = *std::move(region),
                      .entity = entity,
                  },
              .key = *std::move(key),
          },
      .value = *std::move(value),
  };
}

} // namespace

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
  if (value.size() > maximum_script_state_key_length) {
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

std::vector<std::byte> encode_script_state(const ScriptStateBag& state) {
  ByteWriter writer;
  writer.write_u32(script_state_magic);
  writer.write_u16(script_state_version);
  writer.write_u32(static_cast<std::uint32_t>(state.values().size()));
  for (const auto& [address, value] : state.values()) {
    writer.write(address.module_id);
    writer.write_u16(static_cast<std::uint16_t>(address.scope.kind));
    writer.write(address.scope.region);
    writer.write_u16(address.scope.entity.has_value() ? 1U : 0U);
    if (address.scope.entity) {
      writer.write(*address.scope.entity);
    }
    writer.write_string(address.key.value());
    std::visit(
        [&writer](const auto& item) {
          using Value = std::decay_t<decltype(item)>;
          if constexpr (std::is_same_v<Value, bool>) {
            writer.write_u16(static_cast<std::uint16_t>(DurableValueTag::boolean));
            writer.write_u16(item ? 1U : 0U);
          } else if constexpr (std::is_same_v<Value, std::int64_t>) {
            writer.write_u16(static_cast<std::uint16_t>(DurableValueTag::integer));
            writer.write_u64(std::bit_cast<std::uint64_t>(item));
          } else if constexpr (std::is_same_v<Value, ContentId>) {
            writer.write_u16(static_cast<std::uint16_t>(DurableValueTag::content_id));
            writer.write(item);
          } else {
            writer.write_u16(static_cast<std::uint16_t>(DurableValueTag::entity_id));
            writer.write(item);
          }
        },
        value);
  }
  return {writer.bytes().begin(), writer.bytes().end()};
}

Result<ScriptStateBag, ScriptStateDecodeError>
decode_script_state(const std::span<const std::byte> bytes) {
  ByteReader reader{bytes};
  const auto magic = reader.read_u32();
  const auto version = reader.read_u16();
  const auto count = reader.read_u32();
  if (!magic || !version || !count || *magic != script_state_magic ||
      *version != script_state_version) {
    return tl::unexpected{ScriptStateDecodeError::invalid_format};
  }

  ScriptStateBag state;
  std::vector<ScriptStateWrite> writes;
  writes.reserve(*count);
  for (std::uint32_t index = 0; index < *count; ++index) {
    auto write = decode_script_state_write(reader);
    if (!write) {
      return tl::unexpected{write.error()};
    }
    writes.push_back(*std::move(write));
  }
  if (reader.remaining() != 0) {
    return tl::unexpected{ScriptStateDecodeError::invalid_format};
  }
  state.apply(writes);
  return state;
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

void ScriptCallbackTransaction::request_combat() {
  mode_commands_.push_back(ScriptModeCommand::request_combat);
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
  for (const auto& state_write : std::ranges::reverse_view(state_writes_)) {
    if (state_write.address.key == key) {
      return &state_write.value;
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
  ScriptRuleResult result{.accepted = true,
                          .reason = {},
                          .deferred_commands = {},
                          .deferred_mode_commands = {},
                          .fault = {}};
  std::vector<ScriptStateWrite> pending_state;
  std::vector<PlaceEntityEnvelope> pending_commands;
  std::vector<ScriptModeCommand> pending_mode_commands;
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
    pending_mode_commands.insert(pending_mode_commands.end(), transaction.mode_commands().begin(),
                                 transaction.mode_commands().end());
    if (!result.accepted) {
      return result;
    }
  }
  state_.apply(pending_state);
  result.deferred_commands = std::move(pending_commands);
  result.deferred_mode_commands = std::move(pending_mode_commands);
  return result;
}

ScriptRuleResult TypedScriptRuntime::contribute_ability(const combat::PerformAbility& query,
                                                        const Tick tick) {
  ScriptRuleResult result{.accepted = true,
                          .reason = {},
                          .deferred_commands = {},
                          .deferred_mode_commands = {},
                          .fault = {}};
  std::vector<ScriptStateWrite> pending_state;
  std::vector<PlaceEntityEnvelope> pending_commands;
  std::vector<ScriptModeCommand> pending_mode_commands;
  for (const auto& module : modules_) {
    ScriptCallbackTransaction transaction{module, state_};
    auto callback = port_->contribute_ability(module, query, transaction, stream_for(module));
    if (!callback) {
      result.accepted = false;
      result.fault = ScriptCallbackError{.module_id = module.module_id,
                                         .scope = module.scope,
                                         .callback = "contribute_ability",
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
    pending_mode_commands.insert(pending_mode_commands.end(), transaction.mode_commands().begin(),
                                 transaction.mode_commands().end());
    if (!result.accepted) {
      return result;
    }
  }
  state_.apply(pending_state);
  result.deferred_commands = std::move(pending_commands);
  result.deferred_mode_commands = std::move(pending_mode_commands);
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
    result.deferred_mode_commands.insert(result.deferred_mode_commands.end(),
                                         transaction.mode_commands().begin(),
                                         transaction.mode_commands().end());
  }
  return result;
}

ScriptEventResult TypedScriptRuntime::on_damage_applied(const combat::DamageApplied& event,
                                                        const Tick tick) {
  ScriptEventResult result;
  for (const auto& module : modules_) {
    ScriptCallbackTransaction transaction{module, state_};
    auto callback = port_->on_damage_applied(module, event, transaction, stream_for(module));
    if (!callback) {
      world_faulted_ = true;
      result.fault = ScriptCallbackError{.module_id = module.module_id,
                                         .scope = module.scope,
                                         .callback = "on_damage_applied",
                                         .tick = tick,
                                         .message = callback.error()};
      return result;
    }
    state_.apply(transaction.state_writes());
  }
  return result;
}

ScriptEventResult TypedScriptRuntime::on_actor_killed(const combat::ActorKilled& event,
                                                      const Tick tick) {
  ScriptEventResult result;
  for (const auto& module : modules_) {
    ScriptCallbackTransaction transaction{module, state_};
    auto callback = port_->on_actor_killed(module, event, transaction, stream_for(module));
    if (!callback) {
      world_faulted_ = true;
      result.fault = ScriptCallbackError{.module_id = module.module_id,
                                         .scope = module.scope,
                                         .callback = "on_actor_killed",
                                         .tick = tick,
                                         .message = callback.error()};
      return result;
    }
    state_.apply(transaction.state_writes());
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

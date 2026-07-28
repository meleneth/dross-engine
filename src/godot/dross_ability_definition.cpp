#include "dross_ability_definition.hpp"

#include <godot_cpp/core/class_db.hpp>

#include <limits>
#include <string_view>

namespace dross::godot_adapter {
namespace {

std::optional<ContentId> parse_id(const godot::String& value) {
  const auto utf8 = value.utf8();
  auto parsed =
      ContentId::parse(std::string_view{utf8.get_data(), static_cast<std::size_t>(utf8.length())});
  return parsed ? std::optional<ContentId>{std::move(*parsed)} : std::nullopt;
}

} // namespace

void DrossAbilityDefinition::set_ability_id(const godot::String& value) { ability_id_ = value; }
godot::String DrossAbilityDefinition::get_ability_id() const { return ability_id_; }
void DrossAbilityDefinition::set_range(const std::int64_t value) { range_ = value; }
std::int64_t DrossAbilityDefinition::get_range() const noexcept { return range_; }
void DrossAbilityDefinition::set_action_point_cost(const std::int64_t value) {
  action_point_cost_ = value;
}
std::int64_t DrossAbilityDefinition::get_action_point_cost() const noexcept {
  return action_point_cost_;
}
void DrossAbilityDefinition::set_damage(const std::int64_t value) { damage_ = value; }
std::int64_t DrossAbilityDefinition::get_damage() const noexcept { return damage_; }
void DrossAbilityDefinition::set_presentation_cue(const godot::String& value) {
  presentation_cue_ = value;
}
godot::String DrossAbilityDefinition::get_presentation_cue() const { return presentation_cue_; }

std::optional<AbilityDefinition> DrossAbilityDefinition::compile_core() const {
  auto ability = parse_id(ability_id_);
  auto cue = parse_id(presentation_cue_);
  constexpr auto maximum = static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max());
  if (!ability || !cue || range_ < 0 || range_ > maximum || action_point_cost_ < 0 ||
      action_point_cost_ > maximum || damage_ <= 0 ||
      damage_ > std::numeric_limits<std::int32_t>::max()) {
    return std::nullopt;
  }
  return AbilityDefinition{
      .id = std::move(*ability),
      .range = static_cast<std::uint32_t>(range_),
      .action_point_cost = static_cast<std::uint32_t>(action_point_cost_),
      .damage = HitPoints{static_cast<std::int32_t>(damage_)},
      .presentation_cue = std::move(*cue),
  };
}

bool DrossAbilityDefinition::is_valid() const { return compile_core().has_value(); }

void DrossAbilityDefinition::_bind_methods() {
  godot::ClassDB::bind_method(godot::D_METHOD("set_ability_id", "value"),
                              &DrossAbilityDefinition::set_ability_id);
  godot::ClassDB::bind_method(godot::D_METHOD("get_ability_id"),
                              &DrossAbilityDefinition::get_ability_id);
  godot::ClassDB::bind_method(godot::D_METHOD("set_range", "value"),
                              &DrossAbilityDefinition::set_range);
  godot::ClassDB::bind_method(godot::D_METHOD("get_range"), &DrossAbilityDefinition::get_range);
  godot::ClassDB::bind_method(godot::D_METHOD("set_action_point_cost", "value"),
                              &DrossAbilityDefinition::set_action_point_cost);
  godot::ClassDB::bind_method(godot::D_METHOD("get_action_point_cost"),
                              &DrossAbilityDefinition::get_action_point_cost);
  godot::ClassDB::bind_method(godot::D_METHOD("set_damage", "value"),
                              &DrossAbilityDefinition::set_damage);
  godot::ClassDB::bind_method(godot::D_METHOD("get_damage"), &DrossAbilityDefinition::get_damage);
  godot::ClassDB::bind_method(godot::D_METHOD("set_presentation_cue", "value"),
                              &DrossAbilityDefinition::set_presentation_cue);
  godot::ClassDB::bind_method(godot::D_METHOD("get_presentation_cue"),
                              &DrossAbilityDefinition::get_presentation_cue);
  godot::ClassDB::bind_method(godot::D_METHOD("is_valid"), &DrossAbilityDefinition::is_valid);
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::STRING, "ability_id"), "set_ability_id",
               "get_ability_id");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "range"), "set_range", "get_range");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "action_point_cost"),
               "set_action_point_cost", "get_action_point_cost");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "damage"), "set_damage", "get_damage");
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::STRING, "presentation_cue"),
               "set_presentation_cue", "get_presentation_cue");
}

} // namespace dross::godot_adapter

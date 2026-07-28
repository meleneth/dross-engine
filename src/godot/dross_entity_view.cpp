#include "dross_entity_view.hpp"

#include <godot_cpp/core/class_db.hpp>

#include <algorithm>

namespace dross::godot_adapter {

void DrossEntityView::set_entity_sequence(const std::int64_t value) {
  entity_sequence_ = std::max<std::int64_t>(value, 0);
}

std::int64_t DrossEntityView::get_entity_sequence() const noexcept { return entity_sequence_; }

void DrossEntityView::apply_presentation_snapshot(const godot::Vector3& from,
                                                  const godot::Vector3& to, const double alpha) {
  const auto frame_alpha = static_cast<godot::real_t>(std::clamp(alpha, 0.0, 1.0));
  set_position(from.lerp(to, frame_alpha));
}

void DrossEntityView::_bind_methods() {
  godot::ClassDB::bind_method(godot::D_METHOD("set_entity_sequence", "value"),
                              &DrossEntityView::set_entity_sequence);
  godot::ClassDB::bind_method(godot::D_METHOD("get_entity_sequence"),
                              &DrossEntityView::get_entity_sequence);
  godot::ClassDB::bind_method(godot::D_METHOD("apply_presentation_snapshot", "from", "to", "alpha"),
                              &DrossEntityView::apply_presentation_snapshot);
  ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "entity_sequence"), "set_entity_sequence",
               "get_entity_sequence");
}

bool DrossViewRegistry::register_view(DrossEntityView* view) {
  if (view == nullptr || view->get_entity_sequence() <= 0) {
    return false;
  }
  const auto sequence = view->get_entity_sequence();
  const auto found = views_.find(sequence);
  if (found != views_.end() && godot::ObjectDB::get_instance(found->second) != nullptr) {
    return false;
  }
  views_[sequence] = view->get_instance_id();
  return true;
}

bool DrossViewRegistry::unregister_view(const std::int64_t entity_sequence) {
  return views_.erase(entity_sequence) == 1;
}

DrossEntityView* DrossViewRegistry::find_view(const std::int64_t entity_sequence) const {
  const auto found = views_.find(entity_sequence);
  return found == views_.end() ? nullptr
                               : godot::Object::cast_to<DrossEntityView>(
                                     godot::ObjectDB::get_instance(found->second));
}

std::int64_t DrossViewRegistry::get_view_count() const noexcept {
  return static_cast<std::int64_t>(views_.size());
}

void DrossViewRegistry::_bind_methods() {
  godot::ClassDB::bind_method(godot::D_METHOD("register_view", "view"),
                              &DrossViewRegistry::register_view);
  godot::ClassDB::bind_method(godot::D_METHOD("unregister_view", "entity_sequence"),
                              &DrossViewRegistry::unregister_view);
  godot::ClassDB::bind_method(godot::D_METHOD("find_view", "entity_sequence"),
                              &DrossViewRegistry::find_view);
  godot::ClassDB::bind_method(godot::D_METHOD("get_view_count"),
                              &DrossViewRegistry::get_view_count);
}

} // namespace dross::godot_adapter

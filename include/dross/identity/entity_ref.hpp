#pragma once

#include <dross/identity/ids.hpp>

namespace dross {

class EntityRef {
public:
  constexpr EntityRef(const WorldInstanceId world_instance, const EntityId id) noexcept
      : world_instance_{world_instance}, id_{id} {}

  [[nodiscard]] constexpr WorldInstanceId world_instance() const noexcept {
    return world_instance_;
  }
  [[nodiscard]] constexpr EntityId id() const noexcept { return id_; }
  [[nodiscard]] constexpr auto operator<=>(const EntityRef&) const = default;

private:
  WorldInstanceId world_instance_;
  EntityId id_;
};

} // namespace dross

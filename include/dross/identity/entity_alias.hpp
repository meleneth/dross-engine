#pragma once

#include <dross/identity/content_id.hpp>

#include <utility>

namespace dross {

class EntityAlias {
public:
  explicit EntityAlias(ContentId value) : value_{std::move(value)} {}

  [[nodiscard]] const ContentId& content_id() const noexcept { return value_; }
  [[nodiscard]] auto operator<=>(const EntityAlias&) const = default;

private:
  ContentId value_;
};

} // namespace dross

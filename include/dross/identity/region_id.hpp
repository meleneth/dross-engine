#pragma once

#include <dross/identity/content_id.hpp>

#include <utility>

namespace dross {

class RegionId {
public:
  explicit RegionId(ContentId value) : value_{std::move(value)} {}

  [[nodiscard]] const ContentId& content_id() const noexcept { return value_; }
  [[nodiscard]] auto operator<=>(const RegionId&) const = default;

private:
  ContentId value_;
};

} // namespace dross

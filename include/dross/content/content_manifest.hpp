#pragma once

#include <dross/foundation/result.hpp>
#include <dross/foundation/version.hpp>
#include <dross/identity/content_id.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace dross {

using ContentHash = std::array<std::uint8_t, 32>;

struct ContentPackageRecord {
  ContentId package_id;
  SemanticVersion version;
  std::vector<ContentId> dependencies;
  ContentHash content_hash;

  [[nodiscard]] bool operator==(const ContentPackageRecord&) const = default;
};

using ContentManifest = std::vector<ContentPackageRecord>;

[[nodiscard]] ContentManifest engine_content_manifest();

enum class ContentManifestError : std::uint8_t {
  missing_package,
  unexpected_package,
  version_mismatch,
  dependency_order_mismatch,
  content_hash_mismatch,
};

[[nodiscard]] Result<void, ContentManifestError>
validate_content_manifest(const ContentManifest& saved, const ContentManifest& required);

} // namespace dross

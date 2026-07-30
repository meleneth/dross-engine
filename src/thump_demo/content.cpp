#include "content.hpp"

#include <algorithm>
#include <cstddef>

namespace thump_demo {
namespace {

dross::ContentHash release_hash(const char* value) {
  const auto stable = dross::ContentId::parse(value).value().stable_hash();
  dross::ContentHash result{};
  std::ranges::transform(stable, result.begin(),
                         [](const std::byte byte) { return std::to_integer<std::uint8_t>(byte); });
  return result;
}

} // namespace

const dross::ContentId& package_id() {
  static const auto value = dross::ContentId::parse("thump_demo:package").value();
  return value;
}

const dross::ContentId& room_id() {
  static const auto value = dross::ContentId::parse("thump_demo:room").value();
  return value;
}

const dross::ContentId& thump_ability_id() {
  static const auto value = dross::ContentId::parse("thump_demo:thump").value();
  return value;
}

dross::ContentManifest content_manifest() {
  auto manifest = dross::engine_content_manifest();
  manifest.push_back(dross::ContentPackageRecord{
      .package_id = package_id(),
      .version = {.major = 1, .minor = 0, .patch = 0},
      .dependencies = {dross::ContentId::parse("dross:base").value()},
      .content_hash = release_hash("thump_demo:package_content_v1"),
  });
  return manifest;
}

} // namespace thump_demo

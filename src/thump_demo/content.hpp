#pragma once

#include <dross/content/content_manifest.hpp>
#include <dross/identity/content_id.hpp>

namespace thump_demo {

[[nodiscard]] const dross::ContentId& package_id();
[[nodiscard]] const dross::ContentId& room_id();
[[nodiscard]] const dross::ContentId& thump_ability_id();
[[nodiscard]] dross::ContentManifest content_manifest();

} // namespace thump_demo

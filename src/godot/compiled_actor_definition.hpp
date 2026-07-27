#pragma once

#include <dross/hex/footprint.hpp>
#include <dross/identity/content_id.hpp>

namespace dross::godot_adapter {

struct CompiledActorDefinition {
  ContentId id;
  FootprintDefinition footprint;
};

} // namespace dross::godot_adapter

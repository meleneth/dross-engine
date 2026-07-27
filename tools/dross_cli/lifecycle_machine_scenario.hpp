#pragma once

#include <dross/runtime/replay.hpp>

#include <string>

[[nodiscard]] int run_lifecycle_machine_scenario(const std::string& record_path);
[[nodiscard]] int verify_lifecycle_replay(const dross::ReplayLog& recorded);

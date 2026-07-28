#pragma once

#include <dross/runtime/replay.hpp>

#include <cstdint>
#include <string>

[[nodiscard]] int run_exploration_movement_scenario(std::uint64_t seed,
                                                    const std::string& record_path);
[[nodiscard]] int verify_exploration_movement_replay(const dross::ReplayLog& recorded);

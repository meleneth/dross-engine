#pragma once

#include <cstdint>
#include <string>

[[nodiscard]] int run_command_event_kernel_scenario(std::uint64_t seed,
                                                    const std::string& record_path);
[[nodiscard]] int run_replay_verification(const std::string& path);

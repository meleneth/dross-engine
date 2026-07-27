#pragma once

#include <cstdint>
#include <string>

[[nodiscard]] int run_persistence_scenario(std::uint64_t seed, const std::string& save_path);
[[nodiscard]] int inspect_save(const std::string& save_path);
struct ResumeSaveArguments {
  std::string save_path;
  std::string commands_path;
};
[[nodiscard]] int resume_save(const ResumeSaveArguments& arguments);

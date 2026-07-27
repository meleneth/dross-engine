cmake_minimum_required(VERSION 3.25)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "In-source builds are not supported. Use a CMake preset.")
endif()

project(
  dross_engine
  VERSION 0.1.0
  DESCRIPTION "Godot-adapted deterministic tactics engine"
  LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CTest)
include(Dependencies)
include(DrossCompilerWarnings)
include(DrossSanitizers)
include(DrossStaticAnalysis)

add_library(dross_core STATIC
  src/core/foundation/byte_codec.cpp
  src/core/foundation/version.cpp
  src/core/hex/footprint.cpp
  src/core/hex/compiled_hex_map.cpp
  src/core/hex/hex_coord.cpp
  src/core/hex/hex_topology.cpp
  src/core/hex/occupancy.cpp
  src/core/hex/path_planner.cpp
  src/core/hex/traversal.cpp
  src/core/identity/content_id.cpp
  src/core/identity/entity_id_allocator.cpp
  src/core/identity/ids.cpp
  src/core/persistence/save_container.cpp
  src/core/random/random_hub.cpp
  src/core/runtime/command_event_kernel.cpp
  src/core/runtime/fixed_tick_runtime.cpp
  src/core/runtime/machine_trace.cpp
  src/core/runtime/replay.cpp
  src/core/runtime/simulation_mode.cpp
  src/core/runtime/world_lifecycle.cpp
  src/core/world/world_storage.cpp
)
add_library(dross::core ALIAS dross_core)

target_include_directories(dross_core
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/generated/include>"
    "$<INSTALL_INTERFACE:include>"
)
target_include_directories(dross_core SYSTEM PRIVATE
  "${pcg_cpp_SOURCE_DIR}/include"
  "${sml_SOURCE_DIR}/include")
target_compile_features(dross_core PUBLIC cxx_std_20)
target_link_libraries(dross_core
  PUBLIC tl::expected
  PRIVATE blake3::blake3 EnTT::EnTT eventpp::eventpp)
dross_target_warnings(dross_core)
dross_enable_sanitizers(dross_core)

add_executable(dross_headless
  tools/dross_cli/command_event_kernel_scenario.cpp
  tools/dross_cli/hex_pathing_scenario.cpp
  tools/dross_cli/lifecycle_machine_scenario.cpp
  tools/dross_cli/main.cpp
)
target_link_libraries(dross_headless PRIVATE dross::core)
target_compile_features(dross_headless PRIVATE cxx_std_20)
dross_target_warnings(dross_headless)
dross_enable_sanitizers(dross_headless)

add_executable(dross_tests
  generated/tests/generated_schema_compile.cpp
  tests/unit/foundation/version_test.cpp
  tests/unit/foundation/byte_codec_test.cpp
  tests/unit/foundation/quantity_test.cpp
  tests/unit/hex/hex_coord_test.cpp
  tests/unit/hex/hex_map_test.cpp
  tests/unit/hex/path_planner_test.cpp
  tests/unit/hex/hex_topology_test.cpp
  tests/unit/identity/content_id_test.cpp
  tests/unit/identity/identity_test.cpp
  tests/unit/persistence/save_container_test.cpp
  tests/unit/random/random_hub_test.cpp
  tests/unit/runtime/command_event_kernel_test.cpp
  tests/unit/runtime/fixed_tick_runtime_test.cpp
  tests/unit/runtime/replay_test.cpp
  tests/unit/runtime/simulation_mode_test.cpp
  tests/unit/runtime/world_lifecycle_test.cpp
  tests/unit/world/world_storage_test.cpp
)
target_link_libraries(dross_tests PRIVATE dross::core Catch2::Catch2WithMain)
target_compile_features(dross_tests PRIVATE cxx_std_20)
dross_target_warnings(dross_tests)
dross_enable_sanitizers(dross_tests)

include(Catch)
catch_discover_tests(dross_tests DISCOVERY_MODE PRE_TEST)

add_test(NAME headless.version COMMAND dross_headless version)
set_tests_properties(
  headless.version
  PROPERTIES PASS_REGULAR_EXPRESSION "^dross-engine 0.1.0 phase-06")
add_test(NAME headless.validate_id COMMAND dross_headless validate-id dross:thump)
set_tests_properties(
  headless.validate_id
  PROPERTIES PASS_REGULAR_EXPRESSION "^dross:thump")
add_test(NAME headless.reject_invalid_id COMMAND dross_headless validate-id Dross:thump)
set_tests_properties(headless.reject_invalid_id PROPERTIES WILL_FAIL TRUE)
add_test(NAME headless.identity_lifecycle
         COMMAND dross_headless scenario identity-lifecycle)
set_tests_properties(
  headless.identity_lifecycle
  PROPERTIES PASS_REGULAR_EXPRESSION
             "^identity-lifecycle world=42 alive=\\[entity:17:2,entity:17:40\\] named=entity:17:40 next=3")
add_test(NAME headless.hex_pathing COMMAND dross_headless scenario hex-pathing)
set_tests_properties(
  headless.hex_pathing
  PROPERTIES PASS_REGULAR_EXPRESSION
             "^hex-pathing single=\\[0,0,east>0,1,east\\] cost=1 asymmetric=\\[0,0,east>0,0,southeast>0,1,southeast\\] cost=2 rotations=1 blocked=\\[occupied,blocked_edge\\] revision=1")
add_test(NAME headless.command_event_kernel
         COMMAND dross_headless scenario command-event-kernel)
set_tests_properties(
  headless.command_event_kernel
  PROPERTIES PASS_REGULAR_EXPRESSION
             "^command-event-kernel seed=12345 randomized=rejected occupancy=2 checkpoints=2")
add_test(NAME headless.lifecycle_machines
         COMMAND dross_headless scenario lifecycle-machines)
set_tests_properties(
  headless.lifecycle_machines
  PROPERTIES PASS_REGULAR_EXPRESSION
             "^lifecycle-machines checkpoints=8 trace=8 final=faulted")

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(
  NAME architecture.no_godot_in_core
  COMMAND Python3::Interpreter
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/architecture/no_godot_in_core.py"
          "${CMAKE_CURRENT_SOURCE_DIR}")
add_test(
  NAME architecture.no_entt_in_public_api
  COMMAND Python3::Interpreter
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/architecture/no_entt_in_public_api.py"
          "${CMAKE_CURRENT_SOURCE_DIR}")
add_test(
  NAME architecture.no_forbidden_random_api
  COMMAND Python3::Interpreter
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/architecture/no_forbidden_random_api.py"
          "${CMAKE_CURRENT_SOURCE_DIR}")

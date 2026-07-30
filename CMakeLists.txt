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
  src/core/hex/grid_bake.cpp
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
  src/core/runtime/combat_runtime.cpp
  src/core/runtime/door_runtime.cpp
  src/core/runtime/fixed_tick_runtime.cpp
  src/core/runtime/machine_trace.cpp
  src/core/runtime/movement_runtime.cpp
  src/core/runtime/replay.cpp
  src/core/runtime/script_runtime.cpp
  src/core/runtime/simulation_mode.cpp
  src/core/runtime/world_lifecycle.cpp
  src/core/world/world_storage.cpp
)
add_library(dross::core ALIAS dross_core)
set_target_properties(dross_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

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

if(DROSS_BUILD_GODOT)
  add_library(dross_godot SHARED
    generated/src/godot_api.cpp
    src/godot/dross_ability_definition.cpp
    src/godot/dross_door_definition.cpp
    src/godot/dross_actor_definition.cpp
    src/godot/dross_entity_view.cpp
    src/godot/dross_footprint_definition.cpp
    src/godot/dross_grid_resources.cpp
    src/godot/dross_hex_grid_region3d.cpp
    src/godot/dross_hex_grid_overlay3d.cpp
    src/godot/dross_script_runtime.cpp
    src/godot/dross_validation_error.cpp
    src/godot/dross_world_host.cpp
    src/godot/register_types.cpp
  )
  target_link_libraries(dross_godot PRIVATE dross::core godot::cpp)
  target_compile_features(dross_godot PRIVATE cxx_std_20)
  target_include_directories(dross_godot PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/godot")
  set_target_properties(
    dross_godot
    PROPERTIES
      CXX_VISIBILITY_PRESET hidden
      VISIBILITY_INLINES_HIDDEN YES
      LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/godot/bin"
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/godot/bin")
  dross_target_warnings(dross_godot)
endif()

add_executable(dross_headless
  tools/dross_cli/command_event_kernel_scenario.cpp
  tools/dross_cli/exploration_movement_scenario.cpp
  tools/dross_cli/hex_pathing_scenario.cpp
  tools/dross_cli/lifecycle_machine_scenario.cpp
  tools/dross_cli/main.cpp
  tools/dross_cli/persistence_scenario.cpp
  tools/dross_cli/thump_scenario.cpp
)
target_link_libraries(dross_headless PRIVATE dross::core)
target_compile_features(dross_headless PRIVATE cxx_std_20)
dross_target_warnings(dross_headless)
dross_enable_sanitizers(dross_headless)

add_executable(dross_benchmarks
  tools/dross_benchmark/main.cpp
)
target_link_libraries(dross_benchmarks PRIVATE dross::core)
target_compile_features(dross_benchmarks PRIVATE cxx_std_20)
dross_target_warnings(dross_benchmarks)
dross_enable_sanitizers(dross_benchmarks)

add_executable(dross_tests
  generated/tests/generated_schema_compile.cpp
  tests/unit/foundation/version_test.cpp
  tests/unit/foundation/byte_codec_test.cpp
  tests/unit/foundation/quantity_test.cpp
  tests/unit/hex/hex_coord_test.cpp
  tests/unit/hex/grid_bake_test.cpp
  tests/unit/hex/hex_map_test.cpp
  tests/unit/hex/path_planner_test.cpp
  tests/unit/hex/hex_topology_test.cpp
  tests/unit/identity/content_id_test.cpp
  tests/unit/identity/identity_test.cpp
  tests/unit/persistence/save_container_test.cpp
  tests/unit/random/random_hub_test.cpp
  tests/unit/runtime/command_event_kernel_test.cpp
  tests/unit/runtime/combat_runtime_test.cpp
  tests/unit/runtime/door_runtime_test.cpp
  tests/unit/runtime/fixed_tick_runtime_test.cpp
  tests/unit/runtime/replay_test.cpp
  tests/unit/runtime/movement_runtime_test.cpp
  tests/unit/runtime/script_runtime_test.cpp
  tests/unit/runtime/simulation_mode_test.cpp
  tests/unit/runtime/world_lifecycle_test.cpp
  tests/unit/world/world_storage_test.cpp
)
target_link_libraries(dross_tests PRIVATE dross::core Catch2::Catch2WithMain)
target_compile_features(dross_tests PRIVATE cxx_std_20)
target_compile_definitions(
  dross_tests PRIVATE DROSS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
dross_target_warnings(dross_tests)
dross_enable_sanitizers(dross_tests)

include(Catch)
catch_discover_tests(dross_tests DISCOVERY_MODE PRE_TEST)

add_test(NAME headless.version COMMAND dross_headless version)
set_tests_properties(
  headless.version
  PROPERTIES PASS_REGULAR_EXPRESSION "^dross-engine 0.1.0 phase-14")
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
add_test(NAME headless.grid_bake COMMAND dross_headless scenario grid-bake)
set_tests_properties(
  headless.grid_bake
  PROPERTIES PASS_REGULAR_EXPRESSION "^grid-bake cells=2 edges=1 manual=1")
add_test(NAME headless.exploration_movement
         COMMAND dross_headless scenario exploration-movement --seed 12345)
set_tests_properties(
  headless.exploration_movement
  PROPERTIES PASS_REGULAR_EXPRESSION
             "^exploration-movement seed=12345 final=3 entered=3 checkpoints=6")
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
add_test(
  NAME headless.replay_fixture
  COMMAND dross_headless replay --verify-checkpoints
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/thump-v1.dross-replay")
set_tests_properties(
  headless.replay_fixture
  PROPERTIES PASS_REGULAR_EXPRESSION "^replay verified checkpoints=2")
add_test(
  NAME headless.compare_runs
  COMMAND dross_headless compare-runs
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/thump-v1.dross-replay"
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/thump-v1.dross-replay")
set_tests_properties(
  headless.compare_runs
  PROPERTIES PASS_REGULAR_EXPRESSION "^runs match checkpoints=2")
add_test(
  NAME headless.thump_save_checkpoints
  COMMAND dross_headless scenario thump-on-field-mouse --seed 12345
          --save-checkpoints "${CMAKE_CURRENT_BINARY_DIR}/thump-saves")
set_tests_properties(
  headless.thump_save_checkpoints
  PROPERTIES
    FIXTURES_SETUP thump_save_checkpoints
    PASS_REGULAR_EXPRESSION
      "^thump-on-field-mouse seed=12345 .* checkpoints=2 exploration_resumed=match combat_resumed=match")
add_test(
  NAME headless.inspect_thump_exploration_save
  COMMAND dross_headless inspect-save
          "${CMAKE_CURRENT_BINARY_DIR}/thump-saves/exploration-tick-0.dross-save")
set_tests_properties(
  headless.inspect_thump_exploration_save
  PROPERTIES
    FIXTURES_REQUIRED thump_save_checkpoints
    PASS_REGULAR_EXPRESSION "^save version=1 tick=0 components=4 streams=0")
add_test(
  NAME headless.inspect_thump_combat_save
  COMMAND dross_headless inspect-save
          "${CMAKE_CURRENT_BINARY_DIR}/thump-saves/combat-tick-0.dross-save")
set_tests_properties(
  headless.inspect_thump_combat_save
  PROPERTIES
    FIXTURES_REQUIRED thump_save_checkpoints
    PASS_REGULAR_EXPRESSION "^save version=1 tick=0 components=4 streams=1")
add_test(
  NAME headless.inspect_thump_completed_save
  COMMAND dross_headless inspect-save
          "${CMAKE_CURRENT_BINARY_DIR}/thump-saves/completed-tick-1.dross-save")
set_tests_properties(
  headless.inspect_thump_completed_save
  PROPERTIES
    FIXTURES_REQUIRED thump_save_checkpoints
    PASS_REGULAR_EXPRESSION "^save version=1 tick=1 components=4 streams=1")
add_test(
  NAME headless.persistence_foundation
  COMMAND dross_headless scenario persistence-foundation --seed 12345
          --save "${CMAKE_CURRENT_BINARY_DIR}/foundation.dross-save")
set_tests_properties(
  headless.persistence_foundation
  PROPERTIES
    FIXTURES_SETUP persistence_save
    PASS_REGULAR_EXPRESSION "^persistence-foundation seed=12345 .* match=yes")
add_test(
  NAME headless.inspect_save
  COMMAND dross_headless inspect-save
          "${CMAKE_CURRENT_BINARY_DIR}/foundation.dross-save")
set_tests_properties(
  headless.inspect_save
  PROPERTIES
    FIXTURES_REQUIRED persistence_save
    PASS_REGULAR_EXPRESSION "^save version=1 tick=1 components=3 streams=1")
add_test(
  NAME headless.resume_save
  COMMAND dross_headless resume
          "${CMAKE_CURRENT_BINARY_DIR}/foundation.dross-save"
          --commands
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/foundation-tail.dross-commands")
set_tests_properties(
  headless.resume_save
  PROPERTIES
    FIXTURES_REQUIRED persistence_save
    PASS_REGULAR_EXPRESSION "^resume final=[0-9a-f]+ events=1")

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
add_test(
  NAME architecture.no_forbidden_gdscript_random
  COMMAND Python3::Interpreter
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/architecture/no_forbidden_gdscript_random.py"
          "${CMAKE_CURRENT_SOURCE_DIR}")

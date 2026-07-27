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
  src/core/identity/content_id.cpp
  src/core/identity/ids.cpp
)
add_library(dross::core ALIAS dross_core)

target_include_directories(dross_core
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>"
)
target_compile_features(dross_core PUBLIC cxx_std_20)
target_link_libraries(dross_core PUBLIC tl::expected PRIVATE blake3::blake3)
dross_target_warnings(dross_core)
dross_enable_sanitizers(dross_core)

add_executable(dross_tests
  tests/unit/foundation/version_test.cpp
  tests/unit/foundation/byte_codec_test.cpp
  tests/unit/foundation/quantity_test.cpp
  tests/unit/identity/content_id_test.cpp
  tests/unit/identity/identity_test.cpp
)
target_link_libraries(dross_tests PRIVATE dross::core Catch2::Catch2WithMain)
target_compile_features(dross_tests PRIVATE cxx_std_20)
dross_target_warnings(dross_tests)
dross_enable_sanitizers(dross_tests)

include(Catch)
catch_discover_tests(dross_tests)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(
  NAME architecture.no_godot_in_core
  COMMAND Python3::Interpreter
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/architecture/no_godot_in_core.py"
          "${CMAKE_CURRENT_SOURCE_DIR}")

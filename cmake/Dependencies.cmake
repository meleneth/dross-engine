include("${CMAKE_CURRENT_LIST_DIR}/CPM.cmake")

option(DROSS_BUILD_GODOT "Build the Godot GDExtension adapter" ON)

set(CATCH_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
CPMAddPackage(
  NAME Catch2
  GITHUB_REPOSITORY catchorg/Catch2
  GIT_TAG v3.8.1
  SYSTEM YES
  OPTIONS
    "CATCH_BUILD_TESTING OFF"
    "CATCH_INSTALL_DOCS OFF"
    "CATCH_INSTALL_EXTRAS OFF")

if(Catch2_ADDED)
  list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")
endif()

CPMAddPackage(NAME EnTT GITHUB_REPOSITORY skypjack/entt GIT_TAG v3.15.0 SYSTEM YES)
CPMAddPackage(NAME sml GITHUB_REPOSITORY boost-ext/sml GIT_TAG v1.1.12 SYSTEM YES)
CPMAddPackage(NAME eventpp GITHUB_REPOSITORY wqking/eventpp GIT_TAG v0.1.3 SYSTEM YES)
CPMAddPackage(NAME pcg_cpp GITHUB_REPOSITORY imneme/pcg-cpp GIT_TAG v0.98.1 SYSTEM YES)
CPMAddPackage(
  NAME tl_expected
  GITHUB_REPOSITORY TartanLlama/expected
  GIT_TAG v1.1.0
  SYSTEM YES
  OPTIONS
    "EXPECTED_BUILD_TESTS OFF")
CPMAddPackage(NAME blake3 GITHUB_REPOSITORY BLAKE3-team/BLAKE3 GIT_TAG 1.8.2 SYSTEM YES)
if(blake3_ADDED)
  add_library(dross_blake3 STATIC
    "${blake3_SOURCE_DIR}/c/blake3.c"
    "${blake3_SOURCE_DIR}/c/blake3_dispatch.c"
    "${blake3_SOURCE_DIR}/c/blake3_portable.c")
  target_include_directories(dross_blake3 SYSTEM PUBLIC "${blake3_SOURCE_DIR}/c")
  target_compile_definitions(dross_blake3 PRIVATE
    BLAKE3_NO_AVX2
    BLAKE3_NO_AVX512
    BLAKE3_NO_SSE2
    BLAKE3_NO_SSE41)
  add_library(blake3::blake3 ALIAS dross_blake3)
endif()
if(DROSS_BUILD_GODOT)
  set(GODOTCPP_USE_HOT_RELOAD ON CACHE BOOL "" FORCE)
  set(GODOTCPP_TARGET "template_debug" CACHE STRING "" FORCE)
  CPMAddPackage(
    NAME godot_cpp
    GITHUB_REPOSITORY godotengine/godot-cpp
    GIT_TAG 357ad8694d49e56d38b487cdf59def8ab3037c83
    SYSTEM YES
    OPTIONS
      "GODOTCPP_USE_HOT_RELOAD ON"
      "GODOTCPP_TARGET template_debug")
endif()

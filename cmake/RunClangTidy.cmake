if(NOT CLANG_TIDY_EXE)
  message(FATAL_ERROR "clang-tidy was not found")
endif()

if(NOT EXISTS "${DROSS_BINARY_DIR}/compile_commands.json")
  message(FATAL_ERROR "compile_commands.json is required for clang-tidy")
endif()

execute_process(
  COMMAND "${CLANG_TIDY_EXE}" -p "${DROSS_BINARY_DIR}"
          "${DROSS_SOURCE_DIR}/src/core/foundation/version.cpp"
          "${DROSS_SOURCE_DIR}/tests/unit/foundation/version_test.cpp"
  RESULT_VARIABLE result)

if(NOT result EQUAL 0)
  message(FATAL_ERROR "clang-tidy failed")
endif()

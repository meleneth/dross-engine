if(NOT CLANG_TIDY_EXE)
  message(FATAL_ERROR "clang-tidy was not found")
endif()

if(NOT EXISTS "${DROSS_BINARY_DIR}/compile_commands.json")
  message(FATAL_ERROR "compile_commands.json is required for clang-tidy")
endif()

file(GLOB_RECURSE DROSS_TIDY_FILES
  "${DROSS_SOURCE_DIR}/src/core/*.cpp"
  "${DROSS_SOURCE_DIR}/tools/*.cpp")

execute_process(
  COMMAND "${CLANG_TIDY_EXE}"
          "--config-file=${DROSS_SOURCE_DIR}/.clang-tidy"
          -p "${DROSS_BINARY_DIR}" ${DROSS_TIDY_FILES}
  RESULT_VARIABLE result)

if(NOT result EQUAL 0)
  message(FATAL_ERROR "clang-tidy failed")
endif()

if(NOT CLANG_FORMAT_EXE)
  message(FATAL_ERROR "clang-format was not found")
endif()

if(NOT DROSS_FORMAT_FILES)
  message(STATUS "No files to format")
  return()
endif()

if(DROSS_FORMAT_MODE STREQUAL "fix")
  execute_process(COMMAND "${CLANG_FORMAT_EXE}" -i ${DROSS_FORMAT_FILES}
                  RESULT_VARIABLE result)
else()
  execute_process(COMMAND "${CLANG_FORMAT_EXE}" --dry-run --Werror ${DROSS_FORMAT_FILES}
                  RESULT_VARIABLE result)
endif()

if(NOT result EQUAL 0)
  message(FATAL_ERROR "clang-format failed")
endif()

# Formatting.cmake - Code formatting with clang-format
#
# Usage:
#   cmake --build . --target format       # Format all source files
#   cmake --build . --target format-check # Check formatting (CI mode)

find_program(CLANG_FORMAT_EXE NAMES clang-format clang-format-18 clang-format-17 clang-format-16 clang-format-15)

if(CLANG_FORMAT_EXE)
    message(STATUS "Found clang-format: ${CLANG_FORMAT_EXE}")

    # Get version
    execute_process(
        COMMAND ${CLANG_FORMAT_EXE} --version
        OUTPUT_VARIABLE CLANG_FORMAT_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message(STATUS "  ${CLANG_FORMAT_VERSION}")

    # Collect all source files
    file(GLOB_RECURSE ALL_SOURCE_FILES
        ${CMAKE_SOURCE_DIR}/src/*.cpp
        ${CMAKE_SOURCE_DIR}/src/*.h
        ${CMAKE_SOURCE_DIR}/src/*.mm
        ${CMAKE_SOURCE_DIR}/tests/*.cpp
        ${CMAKE_SOURCE_DIR}/tests/*.h
    )

    # Exclude generated files
    list(FILTER ALL_SOURCE_FILES EXCLUDE REGEX ".*/(moc_|ui_|qrc_).*")

    # Format target - applies formatting
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i -style=file ${ALL_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Formatting source files with clang-format"
        VERBATIM
    )

    # Format check target - verifies formatting without modifying (for CI)
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror -style=file ${ALL_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Checking source file formatting"
        VERBATIM
    )
else()
    message(STATUS "clang-format not found - formatting targets disabled")
    message(STATUS "  Install with: brew install clang-format")
endif()

# Formatting.cmake - Code formatting with clang-format
#
# Usage:
#   cmake --build . --target format       # Format all source files
#   cmake --build . --target format-check # Check formatting (CI mode)

# Homebrew installs LLVM keg-only (not linked into /opt/homebrew/bin).
# Provide explicit hints so find_program can locate it.
set(_LLVM_HOMEBREW_HINTS
    "/opt/homebrew/opt/llvm/bin"
    "/opt/homebrew/opt/llvm@22/bin"
    "/opt/homebrew/opt/llvm@21/bin"
    "/usr/local/opt/llvm/bin"
    "/usr/local/opt/llvm@22/bin"
    "/usr/local/opt/llvm@21/bin"
)

find_program(CLANG_FORMAT_EXE
    NAMES clang-format clang-format-22 clang-format-21 clang-format-18 clang-format-17 clang-format-16 clang-format-15
    HINTS ${_LLVM_HOMEBREW_HINTS}
)

if(CLANG_FORMAT_EXE)
    message(STATUS "Found clang-format: ${CLANG_FORMAT_EXE}")

    # Get version
    execute_process(
        COMMAND ${CLANG_FORMAT_EXE} --version
        OUTPUT_VARIABLE CLANG_FORMAT_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message(STATUS "  ${CLANG_FORMAT_VERSION}")

    # Collect all source files.
    # .mm (Objective-C++) files are excluded: the .clang-format config uses
    # Language: Cpp and clang-format 22+ rejects ObjC files with a Cpp-only
    # configuration, causing the format/format-check targets to fail.
    file(GLOB_RECURSE ALL_SOURCE_FILES
        ${CMAKE_SOURCE_DIR}/src/*.cpp
        ${CMAKE_SOURCE_DIR}/src/*.h
        ${CMAKE_SOURCE_DIR}/tests/*.cpp
        ${CMAKE_SOURCE_DIR}/tests/*.h
    )

    # Exclude generated files and Objective-C++ platform files
    list(FILTER ALL_SOURCE_FILES EXCLUDE REGEX ".*/(moc_|ui_|qrc_).*")
    list(FILTER ALL_SOURCE_FILES EXCLUDE REGEX ".*\\.mm$")

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

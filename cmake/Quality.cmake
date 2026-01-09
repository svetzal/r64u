# Quality.cmake - Master quality tooling module
#
# This module includes all quality-related CMake modules and provides
# a unified interface for code quality checks.
#
# Available targets:
#   format       - Format all source files with clang-format
#   format-check - Verify formatting (for CI)
#   tidy         - Run clang-tidy static analysis
#   cppcheck     - Run cppcheck static analysis
#   coverage     - Generate code coverage report
#   quality      - Run all quality checks (format-check + static analysis)
#
# Build options:
#   -DBUILD_COVERAGE=ON         - Build with coverage instrumentation
#   -DENABLE_CLANG_TIDY=ON      - Enable clang-tidy during compilation
#   -DSANITIZER=address         - Enable AddressSanitizer
#   -DSANITIZER=undefined       - Enable UndefinedBehaviorSanitizer
#   -DSANITIZER="address;undefined" - Enable multiple sanitizers

# Include individual modules
include(${CMAKE_CURRENT_LIST_DIR}/Coverage.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/Sanitizers.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/StaticAnalysis.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/Formatting.cmake)

# Print summary
message(STATUS "")
message(STATUS "Quality Tooling Summary:")
message(STATUS "  Code formatting:    ${CLANG_FORMAT_EXE}")
message(STATUS "  Static analysis:    ${CLANG_TIDY_EXE}")
message(STATUS "  Additional checks:  ${CPPCHECK_EXE}")
message(STATUS "  Coverage:           ${BUILD_COVERAGE}")
message(STATUS "  Sanitizers:         ${SANITIZER}")
message(STATUS "")

# Create unified quality target
add_custom_target(quality
    COMMENT "Running all quality checks"
)

# Add dependencies based on available tools
if(CLANG_FORMAT_EXE)
    add_dependencies(quality format-check)
endif()

if(CPPCHECK_EXE)
    add_dependencies(quality cppcheck)
endif()

# Note: clang-tidy is typically run during build with ENABLE_CLANG_TIDY
# rather than as a separate target for better incremental analysis

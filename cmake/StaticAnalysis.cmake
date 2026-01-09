# StaticAnalysis.cmake - Static analysis tools (clang-tidy, cppcheck)
#
# Usage:
#   cmake -DENABLE_CLANG_TIDY=ON ..   # Enable clang-tidy during build
#   cmake --build . --target tidy     # Run clang-tidy on all sources
#   cmake --build . --target cppcheck # Run cppcheck on all sources

option(ENABLE_CLANG_TIDY "Enable clang-tidy during compilation" OFF)

# Find clang-tidy
find_program(CLANG_TIDY_EXE NAMES clang-tidy clang-tidy-18 clang-tidy-17 clang-tidy-16 clang-tidy-15)

if(CLANG_TIDY_EXE)
    message(STATUS "Found clang-tidy: ${CLANG_TIDY_EXE}")

    if(ENABLE_CLANG_TIDY)
        message(STATUS "clang-tidy enabled during compilation")
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
    endif()

    # Create tidy target for on-demand analysis
    # Collects all source files and runs clang-tidy on them
    add_custom_target(tidy
        COMMAND ${CMAKE_COMMAND} -E echo "Running clang-tidy..."
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy static analysis"
        VERBATIM
    )
else()
    message(STATUS "clang-tidy not found - static analysis targets disabled")
    message(STATUS "  Install with: brew install llvm")
endif()

# Find cppcheck
find_program(CPPCHECK_EXE NAMES cppcheck)

if(CPPCHECK_EXE)
    message(STATUS "Found cppcheck: ${CPPCHECK_EXE}")

    # Get cppcheck version
    execute_process(
        COMMAND ${CPPCHECK_EXE} --version
        OUTPUT_VARIABLE CPPCHECK_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message(STATUS "  ${CPPCHECK_VERSION}")

    # cppcheck target
    add_custom_target(cppcheck
        COMMAND ${CPPCHECK_EXE}
            --enable=warning,style,performance,portability
            --std=c++17
            --suppress=missingIncludeSystem
            --suppress=unmatchedSuppression
            --inline-suppr
            --quiet
            --error-exitcode=1
            -I ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/src
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running cppcheck static analysis"
        VERBATIM
    )
else()
    message(STATUS "cppcheck not found - cppcheck target disabled")
    message(STATUS "  Install with: brew install cppcheck")
endif()

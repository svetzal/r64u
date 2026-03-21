# StaticAnalysis.cmake - Static analysis tools (clang-tidy, cppcheck)
#
# Usage:
#   cmake -DENABLE_CLANG_TIDY=ON ..   # Enable clang-tidy during build
#   cmake --build . --target tidy     # Run clang-tidy on all sources
#   cmake --build . --target cppcheck # Run cppcheck on all sources

option(ENABLE_CLANG_TIDY "Enable clang-tidy during compilation" OFF)

# Find clang-tidy
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

find_program(CLANG_TIDY_EXE
    NAMES clang-tidy clang-tidy-22 clang-tidy-21 clang-tidy-18 clang-tidy-17 clang-tidy-16 clang-tidy-15
    HINTS ${_LLVM_HOMEBREW_HINTS}
)

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
            # Qt macro false-positives: cppcheck cannot parse moc-generated
            # macros (slots, signals, Q_ENUM, Q_UNUSED, Q_OBJECT, etc.) and
            # falsely reports them as unknown macros.
            --suppress=unknownMacro
            # Qt emit false-positives: "emit signal(...)" is misidentified as
            # a local variable shadowing the signal function declaration.
            --suppress=shadowFunction
            # Qt parent-pointer constructor pattern: QObject-derived classes
            # conventionally accept a nullable parent pointer as their sole
            # argument; marking every such constructor explicit would break
            # the Qt ownership model.
            --suppress=noExplicitConstructor
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

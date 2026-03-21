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

    # Collect only .cpp translation units for clang-tidy.
    # Header files are analysed through their corresponding .cpp files via the
    # compile_commands.json; passing .h files directly as translation units
    # causes false "file not found" errors because they have no standalone entry
    # in compile_commands.json.
    file(GLOB_RECURSE TIDY_SOURCE_FILES
        ${CMAKE_SOURCE_DIR}/src/*.cpp
        ${CMAKE_SOURCE_DIR}/tests/*.cpp
    )
    list(FILTER TIDY_SOURCE_FILES EXCLUDE REGEX ".*/(moc_|ui_|qrc_).*")
    list(FILTER TIDY_SOURCE_FILES EXCLUDE REGEX ".*\\.mm$")
    # Exclude integration tests — they require real hardware and are only compiled
    # with BUILD_INTEGRATION_TESTS=ON, so they have no compile_commands.json
    # entries and their generated .moc files do not exist by default.
    list(FILTER TIDY_SOURCE_FILES EXCLUDE REGEX ".*/tests/integration/.*")

    # On Apple platforms, Homebrew LLVM's clang-tidy does not automatically
    # know the Xcode SDK path.  Inject it so system headers (like <limits>)
    # resolve correctly.
    set(_TIDY_EXTRA_ARGS "")
    if(APPLE)
        execute_process(
            COMMAND xcrun --show-sdk-path
            OUTPUT_VARIABLE _APPLE_SDK_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(_APPLE_SDK_PATH)
            list(APPEND _TIDY_EXTRA_ARGS "--extra-arg=-isysroot${_APPLE_SDK_PATH}")
        endif()
    endif()

    # Create tidy target for on-demand analysis
    add_custom_target(tidy
        COMMAND ${CLANG_TIDY_EXE}
            -p ${CMAKE_BINARY_DIR}
            ${_TIDY_EXTRA_ARGS}
            ${TIDY_SOURCE_FILES}
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

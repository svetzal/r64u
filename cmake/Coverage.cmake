# Coverage.cmake - Code coverage support for Apple Clang
#
# Usage:
#   cmake -DBUILD_COVERAGE=ON ..
#   cmake --build .
#   ctest
#   cmake --build . --target coverage
#
# Requires: Xcode command line tools (provides llvm-profdata, llvm-cov)

option(BUILD_COVERAGE "Build with code coverage instrumentation" OFF)

if(BUILD_COVERAGE)
    message(STATUS "Code coverage enabled")

    # Check for required tools
    find_program(LLVM_PROFDATA xcrun ARGS llvm-profdata)
    find_program(LLVM_COV xcrun ARGS llvm-cov)

    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
        message(FATAL_ERROR "Coverage requires Clang compiler")
    endif()

    # Add coverage flags
    set(COVERAGE_FLAGS "-fprofile-instr-generate -fcoverage-mapping")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COVERAGE_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COVERAGE_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${COVERAGE_FLAGS}")

    # Create coverage directory
    set(COVERAGE_DIR "${CMAKE_BINARY_DIR}/coverage")
    file(MAKE_DIRECTORY ${COVERAGE_DIR})

    # Function to add coverage target for a test executable
    function(add_coverage_target TARGET_NAME TEST_EXECUTABLE)
        set(PROFRAW_FILE "${COVERAGE_DIR}/${TARGET_NAME}.profraw")
        set(PROFDATA_FILE "${COVERAGE_DIR}/${TARGET_NAME}.profdata")
        set(HTML_DIR "${COVERAGE_DIR}/${TARGET_NAME}_html")

        add_custom_target(coverage_${TARGET_NAME}
            COMMAND ${CMAKE_COMMAND} -E env LLVM_PROFILE_FILE=${PROFRAW_FILE} $<TARGET_FILE:${TEST_EXECUTABLE}>
            COMMAND xcrun llvm-profdata merge -sparse ${PROFRAW_FILE} -o ${PROFDATA_FILE}
            COMMAND xcrun llvm-cov show $<TARGET_FILE:${TEST_EXECUTABLE}>
                -instr-profile=${PROFDATA_FILE}
                -format=html
                -output-dir=${HTML_DIR}
                -show-line-counts-or-regions
                -show-instantiations=false
                "-ignore-filename-regex=.*/(tests|moc_|ui_|qrc_).*"
            COMMAND xcrun llvm-cov report $<TARGET_FILE:${TEST_EXECUTABLE}>
                -instr-profile=${PROFDATA_FILE}
                "-ignore-filename-regex=.*/(tests|moc_|ui_|qrc_).*"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS ${TEST_EXECUTABLE}
            COMMENT "Generating coverage report for ${TARGET_NAME}"
            VERBATIM
        )
    endfunction()

    # Main coverage target that runs all tests and generates combined report
    add_custom_target(coverage
        COMMAND ${CMAKE_COMMAND} -E echo "Running tests and generating coverage..."
        COMMENT "Coverage report will be in ${COVERAGE_DIR}"
    )
endif()

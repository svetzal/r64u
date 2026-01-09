# Sanitizers.cmake - Runtime sanitizer support
#
# Usage:
#   cmake -DSANITIZER=address ..    # AddressSanitizer (memory errors)
#   cmake -DSANITIZER=undefined ..  # UndefinedBehaviorSanitizer
#   cmake -DSANITIZER=thread ..     # ThreadSanitizer (data races)
#   cmake -DSANITIZER=memory ..     # MemorySanitizer (uninitialized reads, Linux only)
#
# Multiple sanitizers can be combined: -DSANITIZER="address;undefined"

set(SANITIZER "" CACHE STRING "Enable runtime sanitizers (address, undefined, thread, memory)")

if(SANITIZER)
    message(STATUS "Sanitizers enabled: ${SANITIZER}")

    set(SANITIZER_FLAGS "")

    foreach(san ${SANITIZER})
        if(san STREQUAL "address")
            list(APPEND SANITIZER_FLAGS "-fsanitize=address")
            list(APPEND SANITIZER_FLAGS "-fno-omit-frame-pointer")
            message(STATUS "  - AddressSanitizer: Detects memory errors, buffer overflows, use-after-free")
        elseif(san STREQUAL "undefined")
            list(APPEND SANITIZER_FLAGS "-fsanitize=undefined")
            list(APPEND SANITIZER_FLAGS "-fno-omit-frame-pointer")
            message(STATUS "  - UndefinedBehaviorSanitizer: Detects undefined behavior")
        elseif(san STREQUAL "thread")
            list(APPEND SANITIZER_FLAGS "-fsanitize=thread")
            message(STATUS "  - ThreadSanitizer: Detects data races")
        elseif(san STREQUAL "memory")
            if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
                message(WARNING "MemorySanitizer is only supported on Linux")
            else()
                list(APPEND SANITIZER_FLAGS "-fsanitize=memory")
                list(APPEND SANITIZER_FLAGS "-fno-omit-frame-pointer")
                message(STATUS "  - MemorySanitizer: Detects uninitialized memory reads")
            endif()
        else()
            message(FATAL_ERROR "Unknown sanitizer: ${san}")
        endif()
    endforeach()

    string(REPLACE ";" " " SANITIZER_FLAGS_STR "${SANITIZER_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS_STR}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SANITIZER_FLAGS_STR}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SANITIZER_FLAGS_STR}")
endif()

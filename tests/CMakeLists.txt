# Test configuration
enable_testing()

find_package(Qt6 REQUIRED COMPONENTS Test)

# Mock FTP client - placed at services/c64uftpclient.h to shadow the real one
set(MOCK_SOURCES
    services/c64uftpclient.cpp
)

set(MOCK_HEADERS
    services/c64uftpclient.h
)

# TransferQueue tests
# The mock is at tests/services/c64uftpclient.h which shadows src/services/c64uftpclient.h
add_executable(test_transferqueue
    test_transferqueue.cpp
    ${MOCK_SOURCES}
    ${MOCK_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/models/transferqueue.cpp
)

target_include_directories(test_transferqueue PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}  # tests/ dir - so services/c64uftpclient.h shadows src/services/
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_transferqueue PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

# Don't run tests automatically during build
add_test(NAME TransferQueueTest COMMAND test_transferqueue)

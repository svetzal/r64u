# Test configuration
enable_testing()

find_package(Qt6 REQUIRED COMPONENTS Test)

# Mock FTP client - implements IFtpClient for runtime injection
set(MOCK_FTP_SOURCES
    mocks/mockftpclient.cpp
)

set(MOCK_FTP_HEADERS
    mocks/mockftpclient.h
)

# Legacy mock that shadows C64UFtpClient at link time (for ConfigFileLoader tests)
set(LEGACY_MOCK_SOURCES
    services/c64uftpclient.cpp
)

set(LEGACY_MOCK_HEADERS
    services/c64uftpclient.h
)

# TransferQueue tests - uses new MockFtpClient via interface injection
add_executable(test_transferqueue
    test_transferqueue.cpp
    ${MOCK_FTP_SOURCES}
    ${MOCK_FTP_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferqueue.cpp
)

target_include_directories(test_transferqueue PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}  # tests/ dir - for mocks/mockftpclient.h
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_transferqueue PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

# Register test with CTest
add_test(NAME TransferQueueTest COMMAND test_transferqueue)

# Add coverage target if coverage is enabled
if(BUILD_COVERAGE)
    add_coverage_target(transferqueue test_transferqueue)
    add_dependencies(coverage coverage_transferqueue)
endif()

# ConfigurationModel tests
add_executable(test_configurationmodel
    test_configurationmodel.cpp
    ${CMAKE_SOURCE_DIR}/src/models/configurationmodel.cpp
)

target_include_directories(test_configurationmodel PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_configurationmodel PRIVATE
    Qt6::Core
    Qt6::Test
)

# Register test with CTest
add_test(NAME ConfigurationModelTest COMMAND test_configurationmodel)

# Add coverage target if coverage is enabled
if(BUILD_COVERAGE)
    add_coverage_target(configurationmodel test_configurationmodel)
    add_dependencies(coverage coverage_configurationmodel)
endif()

# PetsciiConverter tests
add_executable(test_petsciiconverter
    test_petsciiconverter.cpp
    ${CMAKE_SOURCE_DIR}/src/services/petsciiconverter.cpp
)

target_include_directories(test_petsciiconverter PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_petsciiconverter PRIVATE
    Qt6::Core
    Qt6::Test
)

# Register test with CTest
add_test(NAME PetsciiConverterTest COMMAND test_petsciiconverter)

# Add coverage target if coverage is enabled
if(BUILD_COVERAGE)
    add_coverage_target(petsciiconverter test_petsciiconverter)
    add_dependencies(coverage coverage_petsciiconverter)
endif()

# SidFileParser tests
add_executable(test_sidfileparser
    test_sidfileparser.cpp
    ${CMAKE_SOURCE_DIR}/src/services/sidfileparser.cpp
)

target_include_directories(test_sidfileparser PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_sidfileparser PRIVATE
    Qt6::Core
    Qt6::Test
)

# Register test with CTest
add_test(NAME SidFileParserTest COMMAND test_sidfileparser)

# Add coverage target if coverage is enabled
if(BUILD_COVERAGE)
    add_coverage_target(sidfileparser test_sidfileparser)
    add_dependencies(coverage coverage_sidfileparser)
endif()

# ConfigFileLoader tests - uses legacy mock that shadows C64UFtpClient
add_executable(test_configfileloader
    test_configfileloader.cpp
    ${CMAKE_SOURCE_DIR}/src/services/configfileloader.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${LEGACY_MOCK_SOURCES}
    services/c64urestclient.cpp
)

target_include_directories(test_configfileloader BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/services  # Mock services directory
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(test_configfileloader PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_configfileloader PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

# Register test with CTest
add_test(NAME ConfigFileLoaderTest COMMAND test_configfileloader)

# Add coverage target if coverage is enabled
if(BUILD_COVERAGE)
    add_coverage_target(configfileloader test_configfileloader)
    add_dependencies(coverage coverage_configfileloader)
endif()

# DiskImageReader tests
add_executable(test_diskimagereader
    test_diskimagereader.cpp
    ${CMAKE_SOURCE_DIR}/src/services/diskimagereader.cpp
    ${CMAKE_SOURCE_DIR}/src/services/petsciiconverter.cpp
)

target_include_directories(test_diskimagereader PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_diskimagereader PRIVATE
    Qt6::Core
    Qt6::Test
)

# Register test with CTest
add_test(NAME DiskImageReaderTest COMMAND test_diskimagereader)

# Add coverage target if coverage is enabled
if(BUILD_COVERAGE)
    add_coverage_target(diskimagereader test_diskimagereader)
    add_dependencies(coverage coverage_diskimagereader)
endif()

# LocalFileProxyModel tests
add_executable(test_localfileproxymodel
    test_localfileproxymodel.cpp
    ${CMAKE_SOURCE_DIR}/src/models/localfileproxymodel.cpp
)

target_include_directories(test_localfileproxymodel PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_localfileproxymodel PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Widgets
)

# Register test with CTest
add_test(NAME LocalFileProxyModelTest COMMAND test_localfileproxymodel)

# Add coverage target if coverage is enabled
if(BUILD_COVERAGE)
    add_coverage_target(localfileproxymodel test_localfileproxymodel)
    add_dependencies(coverage coverage_localfileproxymodel)
endif()

# VideoStreamReceiver tests
add_executable(test_videostreamreceiver
    test_videostreamreceiver.cpp
    ${CMAKE_SOURCE_DIR}/src/services/videostreamreceiver.cpp
)

target_include_directories(test_videostreamreceiver PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_videostreamreceiver PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

# Register test with CTest
add_test(NAME VideoStreamReceiverTest COMMAND test_videostreamreceiver)

# Add coverage target if coverage is enabled
if(BUILD_COVERAGE)
    add_coverage_target(videostreamreceiver test_videostreamreceiver)
    add_dependencies(coverage coverage_videostreamreceiver)
endif()

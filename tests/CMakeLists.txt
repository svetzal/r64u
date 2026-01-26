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

# RemoteFileModel tests - uses MockFtpClient via interface injection
add_executable(test_remotefilemodel
    test_remotefilemodel.cpp
    ${MOCK_FTP_SOURCES}
    ${MOCK_FTP_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/models/remotefilemodel.cpp
)

target_include_directories(test_remotefilemodel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}  # tests/ dir - for mocks/mockftpclient.h
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_remotefilemodel PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Widgets
)

# Register test with CTest
add_test(NAME RemoteFileModelTest COMMAND test_remotefilemodel)

# Add coverage target if coverage is enabled
if(BUILD_COVERAGE)
    add_coverage_target(remotefilemodel test_remotefilemodel)
    add_dependencies(coverage coverage_remotefilemodel)
endif()

# DeviceConnection tests - tests state machine using real clients with signal injection
add_executable(test_deviceconnection
    test_deviceconnection.cpp
    ${CMAKE_SOURCE_DIR}/src/services/deviceconnection.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64urestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64uftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
)

target_include_directories(test_deviceconnection PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_deviceconnection PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

# Register test with CTest
add_test(NAME DeviceConnectionTest COMMAND test_deviceconnection)

# Add coverage target if coverage is enabled
if(BUILD_COVERAGE)
    add_coverage_target(deviceconnection test_deviceconnection)
    add_dependencies(coverage coverage_deviceconnection)
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

# FilePreviewService tests - uses MockFtpClient via interface injection
add_executable(test_filepreviewservice
    test_filepreviewservice.cpp
    ${MOCK_FTP_SOURCES}
    ${MOCK_FTP_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/filepreviewservice.cpp
)

target_include_directories(test_filepreviewservice PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}  # tests/ dir - for mocks/mockftpclient.h
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_filepreviewservice PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

# Register test with CTest
add_test(NAME FilePreviewServiceTest COMMAND test_filepreviewservice)

# Add coverage target if coverage is enabled
if(BUILD_COVERAGE)
    add_coverage_target(filepreviewservice test_filepreviewservice)
    add_dependencies(coverage coverage_filepreviewservice)
endif()

# TransferService tests - uses MockFtpClient via interface injection
add_executable(test_transferservice
    test_transferservice.cpp
    ${MOCK_FTP_SOURCES}
    ${MOCK_FTP_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/deviceconnection.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64urestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64uftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/transferservice.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferqueue.cpp
)

target_include_directories(test_transferservice PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}  # tests/ dir - for mocks/mockftpclient.h
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_transferservice PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

# Register test with CTest
add_test(NAME TransferServiceTest COMMAND test_transferservice)

# Add coverage target if coverage is enabled
if(BUILD_COVERAGE)
    add_coverage_target(transferservice test_transferservice)
    add_dependencies(coverage coverage_transferservice)
endif()

# ErrorHandler tests
add_executable(test_errorhandler
    test_errorhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/services/errorhandler.cpp
)

target_include_directories(test_errorhandler PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_errorhandler PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Widgets
)

# Register test with CTest
add_test(NAME ErrorHandlerTest COMMAND test_errorhandler)

# Add coverage target if coverage is enabled
if(BUILD_COVERAGE)
    add_coverage_target(errorhandler test_errorhandler)
    add_dependencies(coverage coverage_errorhandler)
endif()

# StatusMessageService tests
add_executable(test_statusmessageservice
    test_statusmessageservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/statusmessageservice.cpp
)

target_include_directories(test_statusmessageservice PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_statusmessageservice PRIVATE
    Qt6::Core
    Qt6::Test
)

# Register test with CTest
add_test(NAME StatusMessageServiceTest COMMAND test_statusmessageservice)

# Add coverage target if coverage is enabled
if(BUILD_COVERAGE)
    add_coverage_target(statusmessageservice test_statusmessageservice)
    add_dependencies(coverage coverage_statusmessageservice)
endif()

# C64UFtpClient protocol tests - tests the real FTP client implementation
add_executable(test_c64uftpclient_protocol
    test_c64uftpclient_protocol.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64uftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
)

target_include_directories(test_c64uftpclient_protocol BEFORE PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_c64uftpclient_protocol PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

# Register test with CTest
add_test(NAME C64UFtpClientProtocolTest COMMAND test_c64uftpclient_protocol)

# Add coverage target if coverage is enabled
if(BUILD_COVERAGE)
    add_coverage_target(c64uftpclient_protocol test_c64uftpclient_protocol)
    add_dependencies(coverage coverage_c64uftpclient_protocol)
endif()

# FilePreview strategy tests
add_executable(test_filepreviewstrategies
    test_filepreviewstrategies.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filepreview/filepreviewfactory.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filepreview/textfilepreview.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filepreview/diskimagepreview.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filepreview/sidfilepreview.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filepreview/defaultfilepreview.cpp
    ${CMAKE_SOURCE_DIR}/src/services/diskimagereader.cpp
    ${CMAKE_SOURCE_DIR}/src/services/sidfileparser.cpp
    ${CMAKE_SOURCE_DIR}/src/services/petsciiconverter.cpp
)

target_include_directories(test_filepreviewstrategies BEFORE PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_filepreviewstrategies PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Widgets
)

# Register test with CTest
add_test(NAME FilePreviewStrategiesTest COMMAND test_filepreviewstrategies)

# Add coverage target if coverage is enabled
if(BUILD_COVERAGE)
    add_coverage_target(filepreviewstrategies test_filepreviewstrategies)
    add_dependencies(coverage coverage_filepreviewstrategies)
endif()

# Real Device Download integration test (optional - requires actual hardware)
# Build only when BUILD_INTEGRATION_TESTS is ON
option(BUILD_INTEGRATION_TESTS "Build integration tests that require real hardware" OFF)

if(BUILD_INTEGRATION_TESTS)
    add_executable(test_realdevice_download
        integration/test_realdevice_download.cpp
        ${CMAKE_SOURCE_DIR}/src/services/c64uftpclient.cpp
        ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
        ${CMAKE_SOURCE_DIR}/src/models/transferqueue.cpp
    )

    target_include_directories(test_realdevice_download PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )

    target_link_libraries(test_realdevice_download PRIVATE
        Qt6::Core
        Qt6::Test
        Qt6::Network
    )

    # Note: Not registered with CTest as it requires hardware
    # Run manually with:
    # DEVICE_HOST=192.168.1.137 TEST_FOLDERS="/SD/folder1,/SD/folder2" ./test_realdevice_download
endif()

# FileBrowserSync integration tests
add_executable(test_filebrowsersync
    test_filebrowsersync.cpp
    ${MOCK_FTP_SOURCES}
    ${MOCK_FTP_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/models/remotefilemodel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/pathnavigationwidget.cpp
)

target_include_directories(test_filebrowsersync PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}  # tests/ dir - for mocks/mockftpclient.h
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_filebrowsersync PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Widgets
)

# Register test with CTest
add_test(NAME FileBrowserSyncTest COMMAND test_filebrowsersync)

# Add coverage target if coverage is enabled
if(BUILD_COVERAGE)
    add_coverage_target(filebrowsersync test_filebrowsersync)
    add_dependencies(coverage coverage_filebrowsersync)
endif()

# RollingStats tests - header-only utility
add_executable(test_rollingstats
    test_rollingstats.cpp
)

target_include_directories(test_rollingstats PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_rollingstats PRIVATE
    Qt6::Core
    Qt6::Test
)

# Register test with CTest
add_test(NAME RollingStatsTest COMMAND test_rollingstats)

# Add coverage target if coverage is enabled
if(BUILD_COVERAGE)
    add_coverage_target(rollingstats test_rollingstats)
    add_dependencies(coverage coverage_rollingstats)
endif()

# StreamingDiagnostics tests
add_executable(test_streamingdiagnostics
    test_streamingdiagnostics.cpp
    ${CMAKE_SOURCE_DIR}/src/services/streamingdiagnostics.cpp
    ${CMAKE_SOURCE_DIR}/src/services/videostreamreceiver.cpp
    ${CMAKE_SOURCE_DIR}/src/services/audiostreamreceiver.cpp
)

target_include_directories(test_streamingdiagnostics PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_streamingdiagnostics PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Gui
    Qt6::Network
)

# Register test with CTest
add_test(NAME StreamingDiagnosticsTest COMMAND test_streamingdiagnostics)

# Add coverage target if coverage is enabled
if(BUILD_COVERAGE)
    add_coverage_target(streamingdiagnostics test_streamingdiagnostics)
    add_dependencies(coverage coverage_streamingdiagnostics)
endif()

# HVSCMetadataService tests
add_executable(test_hvscmetadataservice
    test_hvscmetadataservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/hvscmetadataservice.cpp
)

target_include_directories(test_hvscmetadataservice PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_hvscmetadataservice PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

# Register test with CTest
add_test(NAME HVSCMetadataServiceTest COMMAND test_hvscmetadataservice)

# Add coverage target if coverage is enabled
if(BUILD_COVERAGE)
    add_coverage_target(hvscmetadataservice test_hvscmetadataservice)
    add_dependencies(coverage coverage_hvscmetadataservice)
endif()

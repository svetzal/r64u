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

# Mock local file system - implements ILocalFileSystem for runtime injection
set(MOCK_LOCAL_FS_SOURCES
    mocks/mocklocalfilesystem.cpp
)

set(MOCK_LOCAL_FS_HEADERS
    mocks/mocklocalfilesystem.h
)

# Mock file downloader - implements IFileDownloader for runtime injection
set(MOCK_FILE_DOWNLOADER_SOURCES
    mocks/mockfiledownloader.cpp
)

set(MOCK_FILE_DOWNLOADER_HEADERS
    mocks/mockfiledownloader.h
)

# Streaming interface mock sources (Q_OBJECT mocks need .cpp for MOC)
set(MOCK_STREAMING_SOURCES
    mocks/mockstreamcontrolclient.cpp
    mocks/mockvideostreamreceiver.cpp
    mocks/mockaudiostreamreceiver.cpp
    mocks/mockaudioplaybackservice.cpp
)

set(MOCK_STREAMING_HEADERS
    mocks/mockstreamcontrolclient.h
    mocks/mockvideostreamreceiver.h
    mocks/mockaudiostreamreceiver.h
    mocks/mockaudioplaybackservice.h
    mocks/mocknetworkinterfaceprovider.h
)

# Legacy mock that shadows C64UFtpClient at link time (for ConfigFileLoader tests)
set(LEGACY_MOCK_SOURCES
    services/c64uftpclient.cpp
)

set(LEGACY_MOCK_HEADERS
    services/c64uftpclient.h
)

# TransferCore tests - pure transfer logic, no I/O dependencies
add_executable(test_transfercore
    test_transfercore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/transfercore.cpp
)

target_include_directories(test_transfercore PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_transfercore PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME TransferCoreTest COMMAND test_transfercore)

if(BUILD_COVERAGE)
    add_coverage_target(transfercore test_transfercore)
    add_dependencies(coverage coverage_transfercore)
endif()

# BatchManager tests - isolated unit tests for batch lifecycle logic (no I/O, no FTP)
add_executable(test_batchmanager
    test_batchmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/services/transfercore.cpp
    ${CMAKE_SOURCE_DIR}/src/models/batchmanager.cpp
)

target_include_directories(test_batchmanager PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_batchmanager PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME BatchManagerTest COMMAND test_batchmanager)

if(BUILD_COVERAGE)
    add_coverage_target(batchmanager test_batchmanager)
    add_dependencies(coverage coverage_batchmanager)
endif()

# TransferQueue tests - uses MockFtpClient and LocalFileSystem (production) via interface injection
add_executable(test_transferqueue
    test_transferqueue.cpp
    ${MOCK_FTP_SOURCES}
    ${MOCK_FTP_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ilocalfilesystem.cpp
    ${CMAKE_SOURCE_DIR}/src/services/transfercore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/localfilesystem.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferftphandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/batchmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfertimeoutmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfereventprocessor.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/recursivescancoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/remotedirectorycreator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/folderoperationcoordinator.cpp
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

# Coordinator tests - isolated unit tests using MockLocalFileSystem (no real disk I/O)
add_executable(test_coordinators
    test_coordinators.cpp
    ${MOCK_FTP_SOURCES}
    ${MOCK_FTP_HEADERS}
    ${MOCK_LOCAL_FS_SOURCES}
    ${MOCK_LOCAL_FS_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ilocalfilesystem.cpp
    ${CMAKE_SOURCE_DIR}/src/services/transfercore.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/recursivescancoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/remotedirectorycreator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/folderoperationcoordinator.cpp
)

target_include_directories(test_coordinators PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}  # tests/ dir - for mocks/
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_coordinators PRIVATE
    Qt6::Core
    Qt6::Test
)

# Register test with CTest
add_test(NAME CoordinatorsTest COMMAND test_coordinators)

# Add coverage target if coverage is enabled
if(BUILD_COVERAGE)
    add_coverage_target(coordinators test_coordinators)
    add_dependencies(coverage coverage_coordinators)
endif()

# RemoteFileModel tests - uses MockFtpClient via interface injection
add_executable(test_remotefilemodel
    test_remotefilemodel.cpp
    ${MOCK_FTP_SOURCES}
    ${MOCK_FTP_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/filetypecore.cpp
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
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64urestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/restresponsecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64uftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftpcore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftpcommandqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftptransferstate.cpp
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
    ${CMAKE_SOURCE_DIR}/src/services/filetypecore.cpp
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

# ConfigFileLoader tests - uses IFtpClient/IRestClient interfaces
add_executable(test_configfileloader
    test_configfileloader.cpp
    ${CMAKE_SOURCE_DIR}/src/services/configfileloader.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
)

target_include_directories(test_configfileloader PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
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
    ${CMAKE_SOURCE_DIR}/src/services/filetypecore.cpp
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
    ${CMAKE_SOURCE_DIR}/src/services/filetypecore.cpp
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
    ${CMAKE_SOURCE_DIR}/src/services/ivideostreamreceiver.cpp
    ${CMAKE_SOURCE_DIR}/src/services/videostreamreceiver.cpp
    ${CMAKE_SOURCE_DIR}/src/services/videostreamcore.cpp
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
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/deviceconnection.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64urestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/restresponsecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64uftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftpcore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftpcommandqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftptransferstate.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ilocalfilesystem.cpp
    ${CMAKE_SOURCE_DIR}/src/services/transfercore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/localfilesystem.cpp
    ${CMAKE_SOURCE_DIR}/src/services/transferservice.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferftphandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/batchmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfertimeoutmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfereventprocessor.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/recursivescancoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/remotedirectorycreator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/folderoperationcoordinator.cpp
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
    ${CMAKE_SOURCE_DIR}/src/services/ftpcore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftpcommandqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftptransferstate.cpp
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
    ${CMAKE_SOURCE_DIR}/src/ui/filepreview/c64previewbase.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filepreview/filepreviewfactory.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filepreview/textfilepreview.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filepreview/diskimagepreview.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filepreview/sidfilepreview.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filepreview/defaultfilepreview.cpp
    ${CMAKE_SOURCE_DIR}/src/services/diskimagereader.cpp
    ${CMAKE_SOURCE_DIR}/src/services/sidfileparser.cpp
    ${CMAKE_SOURCE_DIR}/src/services/petsciiconverter.cpp
    ${CMAKE_SOURCE_DIR}/src/services/fileactioncore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/filetypecore.cpp
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

# FtpResponseParser tests - pure FTP response parsing functions (in ftpcore)
add_executable(test_ftpresponseparser
    test_ftpresponseparser.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftpcore.cpp
)
target_include_directories(test_ftpresponseparser PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_ftpresponseparser PRIVATE Qt6::Core Qt6::Test)
add_test(NAME FtpResponseParserTest COMMAND test_ftpresponseparser)
if(BUILD_COVERAGE)
    add_coverage_target(ftpresponseparser test_ftpresponseparser)
    add_dependencies(coverage coverage_ftpresponseparser)
endif()

# FtpCommandQueue tests
add_executable(test_ftpcommandqueue
    test_ftpcommandqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftpcommandqueue.cpp
)
target_include_directories(test_ftpcommandqueue PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_ftpcommandqueue PRIVATE Qt6::Core Qt6::Test)
add_test(NAME FtpCommandQueueTest COMMAND test_ftpcommandqueue)
if(BUILD_COVERAGE)
    add_coverage_target(ftpcommandqueue test_ftpcommandqueue)
    add_dependencies(coverage coverage_ftpcommandqueue)
endif()

# FtpTransferState tests
add_executable(test_ftptransferstate
    test_ftptransferstate.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftptransferstate.cpp
)
target_include_directories(test_ftptransferstate PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_ftptransferstate PRIVATE Qt6::Core Qt6::Test)
add_test(NAME FtpTransferStateTest COMMAND test_ftptransferstate)
if(BUILD_COVERAGE)
    add_coverage_target(ftptransferstate test_ftptransferstate)
    add_dependencies(coverage coverage_ftptransferstate)
endif()

# Real Device Download integration test (optional - requires actual hardware)
# Build only when BUILD_INTEGRATION_TESTS is ON
option(BUILD_INTEGRATION_TESTS "Build integration tests that require real hardware" OFF)

if(BUILD_INTEGRATION_TESTS)
    add_executable(test_realdevice_download
        integration/test_realdevice_download.cpp
        ${CMAKE_SOURCE_DIR}/src/services/c64uftpclient.cpp
        ${CMAKE_SOURCE_DIR}/src/services/ftpcore.cpp
        ${CMAKE_SOURCE_DIR}/src/services/ftpcommandqueue.cpp
        ${CMAKE_SOURCE_DIR}/src/services/ftptransferstate.cpp
        ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
        ${CMAKE_SOURCE_DIR}/src/services/transfercore.cpp
        ${CMAKE_SOURCE_DIR}/src/models/transferqueue.cpp
        ${CMAKE_SOURCE_DIR}/src/models/transferftphandler.cpp
        ${CMAKE_SOURCE_DIR}/src/models/batchmanager.cpp
        ${CMAKE_SOURCE_DIR}/src/models/transfertimeoutmanager.cpp
        ${CMAKE_SOURCE_DIR}/src/models/transfereventprocessor.cpp
        ${CMAKE_SOURCE_DIR}/src/ftp/recursivescancoordinator.cpp
        ${CMAKE_SOURCE_DIR}/src/ftp/remotedirectorycreator.cpp
        ${CMAKE_SOURCE_DIR}/src/ftp/folderoperationcoordinator.cpp
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
    ${CMAKE_SOURCE_DIR}/src/services/filetypecore.cpp
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

# KeyboardInputService tests
add_executable(test_keyboardinputservice
    test_keyboardinputservice.cpp
    mocks/mockrestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/keyboardinputservice.cpp
)

target_include_directories(test_keyboardinputservice PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}  # tests/ dir - for mocks/
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_keyboardinputservice PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
)

add_test(NAME KeyboardInputServiceTest COMMAND test_keyboardinputservice)

if(BUILD_COVERAGE)
    add_coverage_target(keyboardinputservice test_keyboardinputservice)
    add_dependencies(coverage coverage_keyboardinputservice)
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
    ${CMAKE_SOURCE_DIR}/src/services/ivideostreamreceiver.cpp
    ${CMAKE_SOURCE_DIR}/src/services/videostreamreceiver.cpp
    ${CMAKE_SOURCE_DIR}/src/services/videostreamcore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iaudiostreamreceiver.cpp
    ${CMAKE_SOURCE_DIR}/src/services/audiostreamreceiver.cpp
    ${CMAKE_SOURCE_DIR}/src/services/audiostreamcore.cpp
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
    ${MOCK_FILE_DOWNLOADER_SOURCES}
    ${MOCK_FILE_DOWNLOADER_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/services/ifiledownloader.cpp
    ${CMAKE_SOURCE_DIR}/src/services/hvscmetadataservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/hvscparser.cpp
)

target_include_directories(test_hvscmetadataservice PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}  # tests/ dir - for mocks/
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

# HVSCParser tests - pure parsing functions, no I/O dependencies
add_executable(test_hvscparser
    test_hvscparser.cpp
    ${CMAKE_SOURCE_DIR}/src/services/hvscparser.cpp
)

target_include_directories(test_hvscparser PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_hvscparser PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME HVSCParserTest COMMAND test_hvscparser)

if(BUILD_COVERAGE)
    add_coverage_target(hvscparser test_hvscparser)
    add_dependencies(coverage coverage_hvscparser)
endif()

# SonglengthsParser tests - pure parsing functions, no I/O dependencies
add_executable(test_songlengthsparser
    test_songlengthsparser.cpp
    ${CMAKE_SOURCE_DIR}/src/services/songlengthsparser.cpp
)

target_include_directories(test_songlengthsparser PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_songlengthsparser PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME SonglengthsParserTest COMMAND test_songlengthsparser)

if(BUILD_COVERAGE)
    add_coverage_target(songlengthsparser test_songlengthsparser)
    add_dependencies(coverage coverage_songlengthsparser)
endif()

# StreamingManager tests - coordination logic using mock streaming services
add_executable(test_streamingmanager
    test_streamingmanager.cpp
    ${MOCK_FTP_SOURCES}
    ${MOCK_FTP_HEADERS}
    ${MOCK_STREAMING_SOURCES}
    ${MOCK_STREAMING_HEADERS}
    mocks/mockrestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/istreamcontrolclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ivideostreamreceiver.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iaudiostreamreceiver.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iaudioplaybackservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/deviceconnection.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64urestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/restresponsecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64uftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftpcore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftpcommandqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftptransferstate.cpp
    ${CMAKE_SOURCE_DIR}/src/services/keyboardinputservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/streamingmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/services/streamcontrolclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/streamprotocolcore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/videostreamreceiver.cpp
    ${CMAKE_SOURCE_DIR}/src/services/videostreamcore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/audiostreamreceiver.cpp
    ${CMAKE_SOURCE_DIR}/src/services/audiostreamcore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/audioplaybackservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/streamingdiagnostics.cpp
)

target_include_directories(test_streamingmanager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}  # tests/ dir - for mocks/
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_streamingmanager PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    Qt6::Network
    Qt6::Multimedia
)

add_test(NAME StreamingManagerTest COMMAND test_streamingmanager)

if(BUILD_COVERAGE)
    add_coverage_target(streamingmanager test_streamingmanager)
    add_dependencies(coverage coverage_streamingmanager)
endif()

# AviCore tests - pure AVI RIFF container format logic, no I/O dependencies
add_executable(test_avicore
    test_avicore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/avicore.cpp
)

target_include_directories(test_avicore PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_avicore PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME AviCoreTest COMMAND test_avicore)

if(BUILD_COVERAGE)
    add_coverage_target(avicore test_avicore)
    add_dependencies(coverage coverage_avicore)
endif()

# AudioStreamCore tests - pure audio packet parsing and rate calculation, no I/O dependencies
add_executable(test_audiostreamcore
    test_audiostreamcore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/audiostreamcore.cpp
)

target_include_directories(test_audiostreamcore PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_audiostreamcore PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME AudioStreamCoreTest COMMAND test_audiostreamcore)

if(BUILD_COVERAGE)
    add_coverage_target(audiostreamcore test_audiostreamcore)
    add_dependencies(coverage coverage_audiostreamcore)
endif()

# VideoStreamCore tests - pure video packet parsing and format detection, no I/O dependencies
add_executable(test_videostreamcore
    test_videostreamcore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/videostreamcore.cpp
)

target_include_directories(test_videostreamcore PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_videostreamcore PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME VideoStreamCoreTest COMMAND test_videostreamcore)

if(BUILD_COVERAGE)
    add_coverage_target(videostreamcore test_videostreamcore)
    add_dependencies(coverage coverage_videostreamcore)
endif()

# RestResponseCore tests - pure REST JSON response parsing, no I/O dependencies
add_executable(test_restresponsecore
    test_restresponsecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/restresponsecore.cpp
)

target_include_directories(test_restresponsecore PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_restresponsecore PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME RestResponseCoreTest COMMAND test_restresponsecore)

if(BUILD_COVERAGE)
    add_coverage_target(restresponsecore test_restresponsecore)
    add_dependencies(coverage coverage_restresponsecore)
endif()

# StreamProtocolCore tests - pure stream control protocol serialisation, no I/O dependencies
add_executable(test_streamprotocolcore
    test_streamprotocolcore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/streamprotocolcore.cpp
)

target_include_directories(test_streamprotocolcore PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_streamprotocolcore PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME StreamProtocolCoreTest COMMAND test_streamprotocolcore)

if(BUILD_COVERAGE)
    add_coverage_target(streamprotocolcore test_streamprotocolcore)
    add_dependencies(coverage coverage_streamprotocolcore)
endif()

# FtpCore tests - pure FTP protocol parsing, no I/O dependencies
add_executable(test_ftpcore
    test_ftpcore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftpcore.cpp
)

target_include_directories(test_ftpcore PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_ftpcore PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME FtpCoreTest COMMAND test_ftpcore)

if(BUILD_COVERAGE)
    add_coverage_target(ftpcore test_ftpcore)
    add_dependencies(coverage coverage_ftpcore)
endif()

# FavoritesCore tests - pure favorites logic, no I/O dependencies
add_executable(test_favoritescore
    test_favoritescore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/favoritescore.cpp
)

target_include_directories(test_favoritescore PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_favoritescore PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME FavoritesCoreTest COMMAND test_favoritescore)

if(BUILD_COVERAGE)
    add_coverage_target(favoritescore test_favoritescore)
    add_dependencies(coverage coverage_favoritescore)
endif()

# FileActionCore tests - pure preview type detection and routing, no I/O dependencies
add_executable(test_fileactioncore
    test_fileactioncore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/fileactioncore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/filetypecore.cpp
)

target_include_directories(test_fileactioncore PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_fileactioncore PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME FileActionCoreTest COMMAND test_fileactioncore)

if(BUILD_COVERAGE)
    add_coverage_target(fileactioncore test_fileactioncore)
    add_dependencies(coverage coverage_fileactioncore)
endif()

# DiskBootSequenceCore tests - pure C64 boot sequence state machine, no I/O dependencies
add_executable(test_diskbootsequencecore
    test_diskbootsequencecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/diskbootsequencecore.cpp
)

target_include_directories(test_diskbootsequencecore PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_diskbootsequencecore PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME DiskBootSequenceCoreTest COMMAND test_diskbootsequencecore)

if(BUILD_COVERAGE)
    add_coverage_target(diskbootsequencecore test_diskbootsequencecore)
    add_dependencies(coverage coverage_diskbootsequencecore)
endif()

# FileMetadataCore tests - pure SID/disk metadata formatting, no I/O dependencies
add_executable(test_filemetadatacore
    test_filemetadatacore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/filemetadatacore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/sidfileparser.cpp
    ${CMAKE_SOURCE_DIR}/src/services/filetypecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/gamebase64service.cpp
    ${CMAKE_SOURCE_DIR}/src/services/hvscmetadataservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/hvscparser.cpp
    ${CMAKE_SOURCE_DIR}/src/services/songlengthsdatabase.cpp
    ${CMAKE_SOURCE_DIR}/src/services/songlengthsparser.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ifiledownloader.cpp
    ${MOCK_FILE_DOWNLOADER_SOURCES}
    ${MOCK_FILE_DOWNLOADER_HEADERS}
)

target_include_directories(test_filemetadatacore PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}  # tests/ dir - for mocks/
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_filemetadatacore PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Sql
    ZLIB::ZLIB
)

add_test(NAME FileMetadataCoreTest COMMAND test_filemetadatacore)

if(BUILD_COVERAGE)
    add_coverage_target(filemetadatacore test_filemetadatacore)
    add_dependencies(coverage coverage_filemetadatacore)
endif()

# FileTypeCore tests - pure file type detection and capabilities, no I/O dependencies
add_executable(test_filetypecore
    test_filetypecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/filetypecore.cpp
)

target_include_directories(test_filetypecore PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_filetypecore PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME FileTypeCoreTest COMMAND test_filetypecore)

if(BUILD_COVERAGE)
    add_coverage_target(filetypecore test_filetypecore)
    add_dependencies(coverage coverage_filetypecore)
endif()

# PlaylistCore tests - pure playlist logic, no I/O dependencies
add_executable(test_playlistcore
    test_playlistcore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/playlistcore.cpp
)

target_include_directories(test_playlistcore PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_playlistcore PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME PlaylistCoreTest COMMAND test_playlistcore)

if(BUILD_COVERAGE)
    add_coverage_target(playlistcore test_playlistcore)
    add_dependencies(coverage coverage_playlistcore)
endif()

# GameBase64Service tests
add_executable(test_gamebase64service
    test_gamebase64service.cpp
    ${MOCK_FILE_DOWNLOADER_SOURCES}
    ${MOCK_FILE_DOWNLOADER_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/services/ifiledownloader.cpp
    ${CMAKE_SOURCE_DIR}/src/services/gamebase64service.cpp
)

target_include_directories(test_gamebase64service PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}  # tests/ dir - for mocks/
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_gamebase64service PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Sql
    ZLIB::ZLIB
)

# Register test with CTest
add_test(NAME GameBase64ServiceTest COMMAND test_gamebase64service)

# Add coverage target if coverage is enabled
if(BUILD_COVERAGE)
    add_coverage_target(gamebase64service test_gamebase64service)
    add_dependencies(coverage coverage_gamebase64service)
endif()

# SonglengthsDatabase tests
add_executable(test_songlengthsdatabase
    test_songlengthsdatabase.cpp
    ${MOCK_FILE_DOWNLOADER_SOURCES}
    ${MOCK_FILE_DOWNLOADER_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/services/ifiledownloader.cpp
    ${CMAKE_SOURCE_DIR}/src/services/songlengthsdatabase.cpp
    ${CMAKE_SOURCE_DIR}/src/services/songlengthsparser.cpp
)

target_include_directories(test_songlengthsdatabase PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}  # tests/ dir - for mocks/
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_songlengthsdatabase PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

# Register test with CTest
add_test(NAME SonglengthsDatabaseTest COMMAND test_songlengthsdatabase)

# Add coverage target if coverage is enabled
if(BUILD_COVERAGE)
    add_coverage_target(songlengthsdatabase test_songlengthsdatabase)
    add_dependencies(coverage coverage_songlengthsdatabase)
endif()

# FavoritesManager tests - imperative shell around favorites:: core, QSettings I/O
add_executable(test_favoritesmanager
    test_favoritesmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/services/favoritesmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/services/favoritescore.cpp
)

target_include_directories(test_favoritesmanager PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_favoritesmanager PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME FavoritesManagerTest COMMAND test_favoritesmanager)

if(BUILD_COVERAGE)
    add_coverage_target(favoritesmanager test_favoritesmanager)
    add_dependencies(coverage coverage_favoritesmanager)
endif()

# DiskBootSequenceService tests - imperative shell around diskboot:: state machine
add_executable(test_diskbootsequenceservice
    test_diskbootsequenceservice.cpp
    mocks/mockrestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/diskbootsequenceservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/diskbootsequencecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
)

target_include_directories(test_diskbootsequenceservice PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}  # tests/ dir - for mocks/
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_diskbootsequenceservice PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

add_test(NAME DiskBootSequenceServiceTest COMMAND test_diskbootsequenceservice)

if(BUILD_COVERAGE)
    add_coverage_target(diskbootsequenceservice test_diskbootsequenceservice)
    add_dependencies(coverage coverage_diskbootsequenceservice)
endif()

# HttpFileDownloader tests - thin HTTP downloader shell, uses local QTcpServer
add_executable(test_httpfiledownloader
    test_httpfiledownloader.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ifiledownloader.cpp
    ${CMAKE_SOURCE_DIR}/src/services/httpfiledownloader.cpp
)

target_include_directories(test_httpfiledownloader PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_httpfiledownloader PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

add_test(NAME HttpFileDownloaderTest COMMAND test_httpfiledownloader)

if(BUILD_COVERAGE)
    add_coverage_target(httpfiledownloader test_httpfiledownloader)
    add_dependencies(coverage coverage_httpfiledownloader)
endif()

# StreamControlClient tests - TCP stream control client
add_executable(test_streamcontrolclient
    test_streamcontrolclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/istreamcontrolclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/streamcontrolclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/streamprotocolcore.cpp
)

target_include_directories(test_streamcontrolclient PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_streamcontrolclient PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

add_test(NAME StreamControlClientTest COMMAND test_streamcontrolclient)

if(BUILD_COVERAGE)
    add_coverage_target(streamcontrolclient test_streamcontrolclient)
    add_dependencies(coverage coverage_streamcontrolclient)
endif()

# AudioStreamReceiver tests - UDP audio receiver with jitter buffer
add_executable(test_audiostreamreceiver
    test_audiostreamreceiver.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iaudiostreamreceiver.cpp
    ${CMAKE_SOURCE_DIR}/src/services/audiostreamreceiver.cpp
    ${CMAKE_SOURCE_DIR}/src/services/audiostreamcore.cpp
)

target_include_directories(test_audiostreamreceiver PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_audiostreamreceiver PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

add_test(NAME AudioStreamReceiverTest COMMAND test_audiostreamreceiver)

if(BUILD_COVERAGE)
    add_coverage_target(audiostreamreceiver test_audiostreamreceiver)
    add_dependencies(coverage coverage_audiostreamreceiver)
endif()

# AudioPlaybackService tests - Qt Multimedia audio playback service
add_executable(test_audioplaybackservice
    test_audioplaybackservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iaudioplaybackservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/audioplaybackservice.cpp
)

target_include_directories(test_audioplaybackservice PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_audioplaybackservice PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Multimedia
)

add_test(NAME AudioPlaybackServiceTest COMMAND test_audioplaybackservice)

if(BUILD_COVERAGE)
    add_coverage_target(audioplaybackservice test_audioplaybackservice)
    add_dependencies(coverage coverage_audioplaybackservice)
endif()

# VideoRecordingService tests - AVI video recording service
add_executable(test_videorecordingservice
    test_videorecordingservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/videorecordingservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/avicore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/vic2frameconverter.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ivideostreamreceiver.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iaudiostreamreceiver.cpp
)

target_include_directories(test_videorecordingservice PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_videorecordingservice PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Test
)

add_test(NAME VideoRecordingServiceTest COMMAND test_videorecordingservice)

if(BUILD_COVERAGE)
    add_coverage_target(videorecordingservice test_videorecordingservice)
    add_dependencies(coverage coverage_videorecordingservice)
endif()

# FileBrowserCore tests - pure file browser action routing, no I/O dependencies
add_executable(test_filebrowsercore
    test_filebrowsercore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/filebrowsercore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/filetypecore.cpp
)
target_include_directories(test_filebrowsercore PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_filebrowsercore PRIVATE Qt6::Core Qt6::Test)
add_test(NAME FileBrowserCoreTest COMMAND test_filebrowsercore)
if(BUILD_COVERAGE)
    add_coverage_target(filebrowsercore test_filebrowsercore)
    add_dependencies(coverage coverage_filebrowsercore)
endif()

# FavoritesUICore tests - pure favorites display logic, no I/O dependencies
add_executable(test_favoritesuicore
    test_favoritesuicore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/favoritesuicore.cpp
)
target_include_directories(test_favoritesuicore PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_favoritesuicore PRIVATE Qt6::Core Qt6::Test)
add_test(NAME FavoritesUICoreTest COMMAND test_favoritesuicore)
if(BUILD_COVERAGE)
    add_coverage_target(favoritesuicore test_favoritesuicore)
    add_dependencies(coverage coverage_favoritesuicore)
endif()

# SystemCommandController tests
add_executable(test_systemcommandcontroller
    test_systemcommandcontroller.cpp
    mocks/mockrestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/systemcommandcontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/services/statusmessageservice.cpp
)
target_include_directories(test_systemcommandcontroller PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_systemcommandcontroller PRIVATE
    Qt6::Core
    Qt6::Test
)
add_test(NAME SystemCommandControllerTest COMMAND test_systemcommandcontroller)
if(BUILD_COVERAGE)
    add_coverage_target(systemcommandcontroller test_systemcommandcontroller)
    add_dependencies(coverage coverage_systemcommandcontroller)
endif()

# TransferTimeoutManager tests - isolated unit tests with configurable timeout
add_executable(test_transfertimeoutmanager
    test_transfertimeoutmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfertimeoutmanager.cpp
)

target_include_directories(test_transfertimeoutmanager PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_transfertimeoutmanager PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME TransferTimeoutManagerTest COMMAND test_transfertimeoutmanager)

if(BUILD_COVERAGE)
    add_coverage_target(transfertimeoutmanager test_transfertimeoutmanager)
    add_dependencies(coverage coverage_transfertimeoutmanager)
endif()

# TransferEventProcessor tests - isolated unit tests for deferred event queue
add_executable(test_transfereventprocessor
    test_transfereventprocessor.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfereventprocessor.cpp
)

target_include_directories(test_transfereventprocessor PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_transfereventprocessor PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME TransferEventProcessorTest COMMAND test_transfereventprocessor)

if(BUILD_COVERAGE)
    add_coverage_target(transfereventprocessor test_transfereventprocessor)
    add_dependencies(coverage coverage_transfereventprocessor)
endif()

# TransferFtpHandler tests - FTP event translation layer
add_executable(test_transferftphandler
    test_transferftphandler.cpp
    ${MOCK_FTP_SOURCES}
    ${MOCK_FTP_HEADERS}
    ${MOCK_LOCAL_FS_SOURCES}
    ${MOCK_LOCAL_FS_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ilocalfilesystem.cpp
    ${CMAKE_SOURCE_DIR}/src/services/transfercore.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferftphandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfertimeoutmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/recursivescancoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/remotedirectorycreator.cpp
)

target_include_directories(test_transferftphandler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_transferftphandler PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

add_test(NAME TransferFtpHandlerTest COMMAND test_transferftphandler)

if(BUILD_COVERAGE)
    add_coverage_target(transferftphandler test_transferftphandler)
    add_dependencies(coverage coverage_transferftphandler)
endif()

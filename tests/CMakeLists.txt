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

# Mock local file system - implements ILocalFileSystemService for runtime injection
set(MOCK_LOCAL_FS_SOURCES
    mocks/mocklocalfilesystemservice.cpp
)

set(MOCK_LOCAL_FS_HEADERS
    mocks/mocklocalfilesystemservice.h
)

# Mock file downloader - implements IFileDownloaderService for runtime injection
set(MOCK_FILE_DOWNLOADER_SOURCES
    mocks/mockfiledownloaderservice.cpp
)

set(MOCK_FILE_DOWNLOADER_HEADERS
    mocks/mockfiledownloaderservice.h
)

# Streaming interface mock sources (Q_OBJECT mocks need .cpp for MOC)
set(MOCK_STREAMING_SOURCES
    mocks/mockstreamcontrolservice.cpp
    mocks/mockvideostreamreceiverservice.cpp
    mocks/mockaudiostreamreceiverservice.cpp
    mocks/mockaudioplaybackservice.cpp
)

set(MOCK_STREAMING_HEADERS
    mocks/mockstreamcontrolservice.h
    mocks/mockvideostreamreceiverservice.h
    mocks/mockaudiostreamreceiverservice.h
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

# All transfer core implementation files (split from transfercore.cpp for cohesion)
set(TRANSFER_CORE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/core/transfercore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/transferconfirmcore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/transferlistingcore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/transferdecisioncore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/transferftpcore.cpp
)

# TransferCore tests - pure transfer logic, no I/O dependencies
add_executable(test_transfercore
    test_transfercore.cpp
    ${TRANSFER_CORE_SOURCES}
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
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${TRANSFER_CORE_SOURCES}
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

# TransferQueue tests - uses MockFtpClient and LocalFileSystemService (production) via interface injection
add_executable(test_transferqueue
    test_transferqueue.cpp
    ${MOCK_FTP_SOURCES}
    ${MOCK_FTP_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ilocalfilesystemservice.cpp
    ${TRANSFER_CORE_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/services/localfilesystemservice.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfermanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferwiring.cpp
    ${CMAKE_SOURCE_DIR}/src/models/singlefileenqueuehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferdispatchhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferdeletehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferftphandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/batchmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfertimeoutmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfereventhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/recursivescancoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/remotedirectorycoordinator.cpp
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

# TransferManager tests - uses MockFtpClient, tests orchestration logic without QAbstractListModel
add_executable(test_transfermanager
    test_transfermanager.cpp
    ${MOCK_FTP_SOURCES}
    ${MOCK_FTP_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ilocalfilesystemservice.cpp
    ${TRANSFER_CORE_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/services/localfilesystemservice.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfermanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferwiring.cpp
    ${CMAKE_SOURCE_DIR}/src/models/singlefileenqueuehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferdispatchhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferdeletehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferftphandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/batchmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfertimeoutmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfereventhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/recursivescancoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/remotedirectorycoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/folderoperationcoordinator.cpp
)

target_include_directories(test_transfermanager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_transfermanager PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

add_test(NAME TransferManagerTest COMMAND test_transfermanager)

if(BUILD_COVERAGE)
    add_coverage_target(transfermanager test_transfermanager)
    add_dependencies(coverage coverage_transfermanager)
endif()

# Coordinator tests - isolated unit tests using MockLocalFileSystemService (no real disk I/O)
add_executable(test_coordinators
    test_coordinators.cpp
    ${MOCK_FTP_SOURCES}
    ${MOCK_FTP_HEADERS}
    ${MOCK_LOCAL_FS_SOURCES}
    ${MOCK_LOCAL_FS_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ilocalfilesystemservice.cpp
    ${TRANSFER_CORE_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/ftp/recursivescancoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/remotedirectorycoordinator.cpp
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

# TransferDecisionCore tests - pure processNext/delete decision and FTP error handling, no I/O
add_executable(test_transferdecisioncore
    test_transferdecisioncore.cpp
    ${TRANSFER_CORE_SOURCES}
)
target_include_directories(test_transferdecisioncore PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_transferdecisioncore PRIVATE
    Qt6::Core
    Qt6::Test
)
add_test(NAME TransferDecisionCoreTest COMMAND test_transferdecisioncore)
if(BUILD_COVERAGE)
    add_coverage_target(transferdecisioncore test_transferdecisioncore)
    add_dependencies(coverage coverage_transferdecisioncore)
endif()

# TransferConfirmCore tests - pure overwrite/folder confirmation logic, no I/O
add_executable(test_transferconfirmcore
    test_transferconfirmcore.cpp
    ${TRANSFER_CORE_SOURCES}
)
target_include_directories(test_transferconfirmcore PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_transferconfirmcore PRIVATE
    Qt6::Core
    Qt6::Test
)
add_test(NAME TransferConfirmCoreTest COMMAND test_transferconfirmcore)
if(BUILD_COVERAGE)
    add_coverage_target(transferconfirmcore test_transferconfirmcore)
    add_dependencies(coverage coverage_transferconfirmcore)
endif()

# TransferListingCore tests - pure directory listing processing, no I/O
add_executable(test_transferlistingcore
    test_transferlistingcore.cpp
    ${TRANSFER_CORE_SOURCES}
)
target_include_directories(test_transferlistingcore PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_transferlistingcore PRIVATE
    Qt6::Core
    Qt6::Test
)
add_test(NAME TransferListingCoreTest COMMAND test_transferlistingcore)
if(BUILD_COVERAGE)
    add_coverage_target(transferlistingcore test_transferlistingcore)
    add_dependencies(coverage coverage_transferlistingcore)
endif()

# TransferFtpCore tests - pure FTP operation completion and enqueue helpers, no I/O
add_executable(test_transferftpcore
    test_transferftpcore.cpp
    ${TRANSFER_CORE_SOURCES}
)
target_include_directories(test_transferftpcore PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_transferftpcore PRIVATE
    Qt6::Core
    Qt6::Test
)
add_test(NAME TransferFtpCoreTest COMMAND test_transferftpcore)
if(BUILD_COVERAGE)
    add_coverage_target(transferftpcore test_transferftpcore)
    add_dependencies(coverage coverage_transferftpcore)
endif()

# RecursiveScanCoordinator tests - isolated unit tests using mock FTP and local FS
add_executable(test_recursivescancoordinator
    test_recursivescancoordinator.cpp
    ${MOCK_FTP_SOURCES}
    ${MOCK_FTP_HEADERS}
    ${MOCK_LOCAL_FS_SOURCES}
    ${MOCK_LOCAL_FS_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ilocalfilesystemservice.cpp
    ${TRANSFER_CORE_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/ftp/recursivescancoordinator.cpp
)
target_include_directories(test_recursivescancoordinator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_recursivescancoordinator PRIVATE
    Qt6::Core
    Qt6::Test
)
add_test(NAME RecursiveScanCoordinatorTest COMMAND test_recursivescancoordinator)
if(BUILD_COVERAGE)
    add_coverage_target(recursivescancoordinator test_recursivescancoordinator)
    add_dependencies(coverage coverage_recursivescancoordinator)
endif()

# RemoteDirectoryCoordinator tests - isolated unit tests using mock FTP and local FS
add_executable(test_remotedirectorycoordinator
    test_remotedirectorycoordinator.cpp
    ${MOCK_FTP_SOURCES}
    ${MOCK_FTP_HEADERS}
    ${MOCK_LOCAL_FS_SOURCES}
    ${MOCK_LOCAL_FS_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ilocalfilesystemservice.cpp
    ${TRANSFER_CORE_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/ftp/remotedirectorycoordinator.cpp
)
target_include_directories(test_remotedirectorycoordinator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_remotedirectorycoordinator PRIVATE
    Qt6::Core
    Qt6::Test
)
add_test(NAME RemoteDirectoryCoordinatorTest COMMAND test_remotedirectorycoordinator)
if(BUILD_COVERAGE)
    add_coverage_target(remotedirectorycoordinator test_remotedirectorycoordinator)
    add_dependencies(coverage coverage_remotedirectorycoordinator)
endif()

# FolderOperationCoordinator tests - isolated unit tests using mock FTP and local FS
add_executable(test_folderoperationcoordinator
    test_folderoperationcoordinator.cpp
    ${MOCK_FTP_SOURCES}
    ${MOCK_FTP_HEADERS}
    ${MOCK_LOCAL_FS_SOURCES}
    ${MOCK_LOCAL_FS_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ilocalfilesystemservice.cpp
    ${TRANSFER_CORE_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/ftp/recursivescancoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/remotedirectorycoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/folderoperationcoordinator.cpp
)
target_include_directories(test_folderoperationcoordinator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_folderoperationcoordinator PRIVATE
    Qt6::Core
    Qt6::Test
)
add_test(NAME FolderOperationCoordinatorTest COMMAND test_folderoperationcoordinator)
if(BUILD_COVERAGE)
    add_coverage_target(folderoperationcoordinator test_folderoperationcoordinator)
    add_dependencies(coverage coverage_folderoperationcoordinator)
endif()

# TransferDeleteHandler tests - delete orchestration using mock FTP and coordinator
add_executable(test_transferdeletehandler
    test_transferdeletehandler.cpp
    ${MOCK_FTP_SOURCES}
    ${MOCK_FTP_HEADERS}
    ${MOCK_LOCAL_FS_SOURCES}
    ${MOCK_LOCAL_FS_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ilocalfilesystemservice.cpp
    ${TRANSFER_CORE_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/ftp/recursivescancoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/remotedirectorycoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferdeletehandler.cpp
)
target_include_directories(test_transferdeletehandler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_transferdeletehandler PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)
add_test(NAME TransferDeleteHandlerTest COMMAND test_transferdeletehandler)
if(BUILD_COVERAGE)
    add_coverage_target(transferdeletehandler test_transferdeletehandler)
    add_dependencies(coverage coverage_transferdeletehandler)
endif()

# RemoteFileBrowserController tests - pure path construction and validation logic, no I/O
add_executable(test_remotefilebrowsercontroller
    test_remotefilebrowsercontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/remotefilebrowsercontroller.cpp
)
target_include_directories(test_remotefilebrowsercontroller PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_remotefilebrowsercontroller PRIVATE
    Qt6::Core
    Qt6::Test
)
add_test(NAME RemoteFileBrowserControllerTest COMMAND test_remotefilebrowsercontroller)
if(BUILD_COVERAGE)
    add_coverage_target(remotefilebrowsercontroller test_remotefilebrowsercontroller)
    add_dependencies(coverage coverage_remotefilebrowsercontroller)
endif()

# RemoteFileModel tests - uses MockFtpClient via interface injection
add_executable(test_remotefilemodel
    test_remotefilemodel.cpp
    ${MOCK_FTP_SOURCES}
    ${MOCK_FTP_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/filetypecore.cpp
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

# DeviceConnectionManager tests - tests state machine using real clients with signal injection
add_executable(test_deviceconnectionmanager
    test_deviceconnectionmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/deviceconnectionmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64urestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/restresponsecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64uftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ftpcore.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftpcommandqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftptransferstate.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftpresponsehandler.cpp
)

target_include_directories(test_deviceconnectionmanager PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_deviceconnectionmanager PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

# Register test with CTest
add_test(NAME DeviceConnectionManagerTest COMMAND test_deviceconnectionmanager)

# Add coverage target if coverage is enabled
if(BUILD_COVERAGE)
    add_coverage_target(deviceconnectionmanager test_deviceconnectionmanager)
    add_dependencies(coverage coverage_deviceconnectionmanager)
endif()

# ConfigurationModel tests
add_executable(test_configurationmodel
    test_configurationmodel.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
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
    ${CMAKE_SOURCE_DIR}/src/core/petsciiconverter.cpp
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
    ${CMAKE_SOURCE_DIR}/src/core/sidfileparser.cpp
    ${CMAKE_SOURCE_DIR}/src/core/filetypecore.cpp
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

# ConfigFileLoaderService tests - uses IFtpClient/IRestClient interfaces
add_executable(test_configfileloaderservice
    test_configfileloaderservice.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/configfileloaderservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
)

target_include_directories(test_configfileloaderservice PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_configfileloaderservice PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

# Register test with CTest
add_test(NAME ConfigFileLoaderServiceTest COMMAND test_configfileloaderservice)

# Add coverage target if coverage is enabled
if(BUILD_COVERAGE)
    add_coverage_target(configfileloaderservice test_configfileloaderservice)
    add_dependencies(coverage coverage_configfileloaderservice)
endif()

# DiskImageReader tests
add_executable(test_diskimagereader
    test_diskimagereader.cpp
    ${CMAKE_SOURCE_DIR}/src/core/diskimagereader.cpp
    ${CMAKE_SOURCE_DIR}/src/core/petsciiconverter.cpp
    ${CMAKE_SOURCE_DIR}/src/core/filetypecore.cpp
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
    ${CMAKE_SOURCE_DIR}/src/core/filetypecore.cpp
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

# VideoStreamReceiverService tests
add_executable(test_videostreamreceiverservice
    test_videostreamreceiverservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ivideostreamreceiverservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/udpstreamsocket.cpp
    ${CMAKE_SOURCE_DIR}/src/services/videostreamreceiverservice.cpp
    ${CMAKE_SOURCE_DIR}/src/core/videostreamcore.cpp
)

target_include_directories(test_videostreamreceiverservice PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_videostreamreceiverservice PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

# Register test with CTest
add_test(NAME VideoStreamReceiverServiceTest COMMAND test_videostreamreceiverservice)

# Add coverage target if coverage is enabled
if(BUILD_COVERAGE)
    add_coverage_target(videostreamreceiverservice test_videostreamreceiverservice)
    add_dependencies(coverage coverage_videostreamreceiverservice)
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
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/deviceconnectionmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64urestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/restresponsecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64uftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ftpcore.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftpcommandqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftptransferstate.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftpresponsehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ilocalfilesystemservice.cpp
    ${TRANSFER_CORE_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/services/localfilesystemservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/transferservice.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfermanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferwiring.cpp
    ${CMAKE_SOURCE_DIR}/src/models/singlefileenqueuehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferdispatchhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferdeletehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferftphandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/batchmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfertimeoutmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfereventhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/recursivescancoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/remotedirectorycoordinator.cpp
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
    stubs/errorsourceconnector_stub.cpp
    ${CMAKE_SOURCE_DIR}/src/services/errorhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
)

target_include_directories(test_errorhandler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_errorhandler PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Widgets
    Qt6::Network
)

# Register test with CTest
add_test(NAME ErrorHandlerTest COMMAND test_errorhandler)

# Add coverage target if coverage is enabled
if(BUILD_COVERAGE)
    add_coverage_target(errorhandler test_errorhandler)
    add_dependencies(coverage coverage_errorhandler)
endif()

# ErrorSourceConnector tests - verifies connectSources() signal routing for each source type
add_executable(test_errorsourceconnector
    test_errorsourceconnector.cpp
    stubs/errorsourceconnector_test_stubs.cpp
    ${MOCK_FTP_SOURCES}
    ${MOCK_FTP_HEADERS}
    ${MOCK_FILE_DOWNLOADER_SOURCES}
    ${MOCK_FILE_DOWNLOADER_HEADERS}
    mocks/mockrestclient.cpp
    mocks/mockrestclient.h
    ${CMAKE_SOURCE_DIR}/src/services/errorsourceconnector.cpp
    ${CMAKE_SOURCE_DIR}/src/services/errorhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ifiledownloaderservice.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/filepreviewservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/configfileloaderservice.cpp
    ${CMAKE_SOURCE_DIR}/src/core/filetypecore.cpp
    ${CMAKE_SOURCE_DIR}/src/models/remotefilemodel.cpp
    ${CMAKE_SOURCE_DIR}/src/services/cacheddownloadservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/songlengthsdatabaseservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/songlengthsparser.cpp
    ${CMAKE_SOURCE_DIR}/src/services/hvscmetadataservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/hvscparser.cpp
    ${CMAKE_SOURCE_DIR}/src/services/gamebase64service.cpp
    ${CMAKE_SOURCE_DIR}/src/services/remotefileoperationsservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/keyboardinputservice.cpp
    # Headers for AUTOMOC to generate signal implementations for stubbed classes
    ${CMAKE_SOURCE_DIR}/src/services/deviceconnectionmanager.h
    ${CMAKE_SOURCE_DIR}/src/services/transferservice.h
    ${CMAKE_SOURCE_DIR}/src/services/videorecordingservice.h
    ${CMAKE_SOURCE_DIR}/src/services/ivideostreamreceiverservice.h
    ${CMAKE_SOURCE_DIR}/src/services/iaudioplaybackservice.h
    ${CMAKE_SOURCE_DIR}/src/services/playlistservice.h
)

target_include_directories(test_errorsourceconnector PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_errorsourceconnector PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Widgets
    Qt6::Sql
    ZLIB::ZLIB
)

add_test(NAME ErrorSourceConnectorTest COMMAND test_errorsourceconnector)

if(BUILD_COVERAGE)
    add_coverage_target(errorsourceconnector test_errorsourceconnector)
    add_dependencies(coverage coverage_errorsourceconnector)
endif()

# RemoteFileOperationsService tests - signal emission for null FTP client guard clauses and happy path
add_executable(test_remotefileoperationsservice
    test_remotefileoperationsservice.cpp
    ${MOCK_FTP_SOURCES}
    ${MOCK_FTP_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/services/remotefileoperationsservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
)

target_include_directories(test_remotefileoperationsservice PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_remotefileoperationsservice PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME RemoteFileOperationsServiceTest COMMAND test_remotefileoperationsservice)

if(BUILD_COVERAGE)
    add_coverage_target(remotefileoperationsservice test_remotefileoperationsservice)
    add_dependencies(coverage coverage_remotefileoperationsservice)
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
    ${CMAKE_SOURCE_DIR}/src/core/ftpcore.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftpcommandqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftptransferstate.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftpresponsehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
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
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filepreview/c64previewbase.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filepreview/textfilepreview.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filepreview/diskimagepreview.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filepreview/sidfilepreview.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filepreview/defaultfilepreview.cpp
    ${CMAKE_SOURCE_DIR}/src/core/diskimagereader.cpp
    ${CMAKE_SOURCE_DIR}/src/core/sidfileparser.cpp
    ${CMAKE_SOURCE_DIR}/src/core/petsciiconverter.cpp
    ${CMAKE_SOURCE_DIR}/src/core/fileactioncore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/filetypecore.cpp
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
    ${CMAKE_SOURCE_DIR}/src/core/ftpcore.cpp
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
    ${CMAKE_SOURCE_DIR}/src/ftp/ftpcommandqueue.cpp
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
    ${CMAKE_SOURCE_DIR}/src/ftp/ftptransferstate.cpp
)
target_include_directories(test_ftptransferstate PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_ftptransferstate PRIVATE Qt6::Core Qt6::Test)
add_test(NAME FtpTransferStateTest COMMAND test_ftptransferstate)
if(BUILD_COVERAGE)
    add_coverage_target(ftptransferstate test_ftptransferstate)
    add_dependencies(coverage coverage_ftptransferstate)
endif()

# FtpResponseHandler tests - FTP response logic with context/action pattern
add_executable(test_ftpresponsehandler
    test_ftpresponsehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftpresponsehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftptransferstate.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftpcommandqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ftpcore.cpp
)
target_include_directories(test_ftpresponsehandler PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_ftpresponsehandler PRIVATE Qt6::Core Qt6::Test Qt6::Network)
add_test(NAME FtpResponseHandlerTest COMMAND test_ftpresponsehandler)
if(BUILD_COVERAGE)
    add_coverage_target(ftpresponsehandler test_ftpresponsehandler)
    add_dependencies(coverage coverage_ftpresponsehandler)
endif()

# Real Device Download integration test (optional - requires actual hardware)
# Build only when BUILD_INTEGRATION_TESTS is ON
option(BUILD_INTEGRATION_TESTS "Build integration tests that require real hardware" OFF)

if(BUILD_INTEGRATION_TESTS)
    add_executable(test_realdevice_download
        integration/test_realdevice_download.cpp
        ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
        ${CMAKE_SOURCE_DIR}/src/services/c64uftpclient.cpp
        ${CMAKE_SOURCE_DIR}/src/core/ftpcore.cpp
        ${CMAKE_SOURCE_DIR}/src/ftp/ftpcommandqueue.cpp
        ${CMAKE_SOURCE_DIR}/src/ftp/ftptransferstate.cpp
        ${CMAKE_SOURCE_DIR}/src/services/ftpresponsehandler.cpp
        ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
        ${TRANSFER_CORE_SOURCES}
        ${CMAKE_SOURCE_DIR}/src/models/transferqueue.cpp
        ${CMAKE_SOURCE_DIR}/src/models/transfermanager.cpp
        ${CMAKE_SOURCE_DIR}/src/models/transferwiring.cpp
        ${CMAKE_SOURCE_DIR}/src/models/singlefileenqueuehandler.cpp
        ${CMAKE_SOURCE_DIR}/src/models/transferdispatchhandler.cpp
        ${CMAKE_SOURCE_DIR}/src/models/transferdeletehandler.cpp
        ${CMAKE_SOURCE_DIR}/src/models/transferftphandler.cpp
        ${CMAKE_SOURCE_DIR}/src/models/batchmanager.cpp
        ${CMAKE_SOURCE_DIR}/src/models/transfertimeoutmanager.cpp
        ${CMAKE_SOURCE_DIR}/src/models/transfereventhandler.cpp
        ${CMAKE_SOURCE_DIR}/src/ftp/recursivescancoordinator.cpp
        ${CMAKE_SOURCE_DIR}/src/ftp/remotedirectorycoordinator.cpp
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
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/filetypecore.cpp
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
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
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

# StreamingDiagnosticsService tests
add_executable(test_streamingdiagnosticsservice
    test_streamingdiagnosticsservice.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/streamingdiagnosticsservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ivideostreamreceiverservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/udpstreamsocket.cpp
    ${CMAKE_SOURCE_DIR}/src/services/videostreamreceiverservice.cpp
    ${CMAKE_SOURCE_DIR}/src/core/videostreamcore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iaudiostreamreceiverservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/audiostreamreceiverservice.cpp
    ${CMAKE_SOURCE_DIR}/src/core/audiostreamcore.cpp
)

target_include_directories(test_streamingdiagnosticsservice PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_streamingdiagnosticsservice PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Gui
    Qt6::Network
)

# Register test with CTest
add_test(NAME StreamingDiagnosticsServiceTest COMMAND test_streamingdiagnosticsservice)

# Add coverage target if coverage is enabled
if(BUILD_COVERAGE)
    add_coverage_target(streamingdiagnosticsservice test_streamingdiagnosticsservice)
    add_dependencies(coverage coverage_streamingdiagnosticsservice)
endif()

# HVSCMetadataService tests
add_executable(test_hvscmetadataservice
    test_hvscmetadataservice.cpp
    ${MOCK_FILE_DOWNLOADER_SOURCES}
    ${MOCK_FILE_DOWNLOADER_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/services/ifiledownloaderservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/cacheddownloadservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/hvscmetadataservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/hvscparser.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
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

# StreamingService tests - coordination logic using mock streaming services
add_executable(test_streamingmanager
    test_streamingmanager.cpp
    ${MOCK_FTP_SOURCES}
    ${MOCK_FTP_HEADERS}
    ${MOCK_STREAMING_SOURCES}
    ${MOCK_STREAMING_HEADERS}
    mocks/mockrestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/istreamcontrolservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ivideostreamreceiverservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iaudiostreamreceiverservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iaudioplaybackservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/deviceconnectionmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64urestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/restresponsecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64uftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ftpcore.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftpcommandqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftptransferstate.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftpresponsehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/services/keyboardinputservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/streamingservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/streamcontrolservice.cpp
    ${CMAKE_SOURCE_DIR}/src/core/streamprotocolcore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/udpstreamsocket.cpp
    ${CMAKE_SOURCE_DIR}/src/services/videostreamreceiverservice.cpp
    ${CMAKE_SOURCE_DIR}/src/core/videostreamcore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/audiostreamreceiverservice.cpp
    ${CMAKE_SOURCE_DIR}/src/core/audiostreamcore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/audioplaybackservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/streamingdiagnosticsservice.cpp
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

add_test(NAME StreamingServiceTest COMMAND test_streamingmanager)

if(BUILD_COVERAGE)
    add_coverage_target(streamingmanager test_streamingmanager)
    add_dependencies(coverage coverage_streamingmanager)
endif()

# AviCore tests - pure AVI RIFF container format logic, no I/O dependencies
add_executable(test_avicore
    test_avicore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/avicore.cpp
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
    ${CMAKE_SOURCE_DIR}/src/core/audiostreamcore.cpp
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
    ${CMAKE_SOURCE_DIR}/src/core/videostreamcore.cpp
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
    ${CMAKE_SOURCE_DIR}/src/core/restresponsecore.cpp
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
    ${CMAKE_SOURCE_DIR}/src/core/streamprotocolcore.cpp
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
    ${CMAKE_SOURCE_DIR}/src/core/ftpcore.cpp
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
    ${CMAKE_SOURCE_DIR}/src/core/favoritescore.cpp
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
    ${CMAKE_SOURCE_DIR}/src/core/fileactioncore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/filetypecore.cpp
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
    ${CMAKE_SOURCE_DIR}/src/core/diskbootsequencecore.cpp
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
    ${CMAKE_SOURCE_DIR}/src/core/filemetadatacore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/sidfileparser.cpp
    ${CMAKE_SOURCE_DIR}/src/core/filetypecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/cacheddownloadservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/gamebase64service.cpp
    ${CMAKE_SOURCE_DIR}/src/services/hvscmetadataservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/hvscparser.cpp
    ${CMAKE_SOURCE_DIR}/src/services/songlengthsdatabaseservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/songlengthsparser.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ifiledownloaderservice.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
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
    ${CMAKE_SOURCE_DIR}/src/core/filetypecore.cpp
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
    ${CMAKE_SOURCE_DIR}/src/core/playlistcore.cpp
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

# PlaylistService tests - tests signal emissions and state orchestration with null device
add_executable(test_playlistservice
    test_playlistservice.cpp
    stubs/playlistservice_test_stubs.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ifiledownloaderservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/playlistservice.cpp
    ${CMAKE_SOURCE_DIR}/src/core/playlistcore.cpp
    # Headers listed here so AUTOMOC generates moc files needed by stubs
    ${CMAKE_SOURCE_DIR}/src/services/streamingservice.h
    ${CMAKE_SOURCE_DIR}/src/services/songlengthsdatabaseservice.h
)

target_include_directories(test_playlistservice PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_playlistservice PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME PlaylistServiceTest COMMAND test_playlistservice)

if(BUILD_COVERAGE)
    add_coverage_target(playlistservice test_playlistservice)
    add_dependencies(coverage coverage_playlistservice)
endif()

# Vic2FrameConverter tests - pure data conversion, no I/O dependencies
add_executable(test_vic2frameconverter
    test_vic2frameconverter.cpp
    ${CMAKE_SOURCE_DIR}/src/services/vic2frameconverter.cpp
)

target_include_directories(test_vic2frameconverter PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_vic2frameconverter PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
)

add_test(NAME Vic2FrameConverterTest COMMAND test_vic2frameconverter)

if(BUILD_COVERAGE)
    add_coverage_target(vic2frameconverter test_vic2frameconverter)
    add_dependencies(coverage coverage_vic2frameconverter)
endif()

# C64URestClient tests - tests setHost() normalization and password state
add_executable(test_c64urestclient
    test_c64urestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64urestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/restresponsecore.cpp
)

target_include_directories(test_c64urestclient PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_c64urestclient PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Test
)

add_test(NAME C64URestClientTest COMMAND test_c64urestclient)

if(BUILD_COVERAGE)
    add_coverage_target(c64urestclient test_c64urestclient)
    add_dependencies(coverage coverage_c64urestclient)
endif()

# GameBase64Service tests
add_executable(test_gamebase64service
    test_gamebase64service.cpp
    ${MOCK_FILE_DOWNLOADER_SOURCES}
    ${MOCK_FILE_DOWNLOADER_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/services/ifiledownloaderservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/cacheddownloadservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/gamebase64service.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
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

# CachedDownloadService tests - download-cache-parse lifecycle logic
add_executable(test_cacheddownloadservice
    test_cacheddownloadservice.cpp
    ${MOCK_FILE_DOWNLOADER_SOURCES}
    ${MOCK_FILE_DOWNLOADER_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/services/ifiledownloaderservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/cacheddownloadservice.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
)

target_include_directories(test_cacheddownloadservice PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}  # tests/ dir - for mocks/
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_cacheddownloadservice PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

# Register test with CTest
add_test(NAME CachedDownloadServiceTest COMMAND test_cacheddownloadservice)

# Add coverage target if coverage is enabled
if(BUILD_COVERAGE)
    add_coverage_target(cacheddownloadservice test_cacheddownloadservice)
    add_dependencies(coverage coverage_cacheddownloadservice)
endif()

# SonglengthsDatabaseService tests
add_executable(test_songlengthsdatabaseservice
    test_songlengthsdatabaseservice.cpp
    ${MOCK_FILE_DOWNLOADER_SOURCES}
    ${MOCK_FILE_DOWNLOADER_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/services/ifiledownloaderservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/cacheddownloadservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/songlengthsdatabaseservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/songlengthsparser.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
)

target_include_directories(test_songlengthsdatabaseservice PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}  # tests/ dir - for mocks/
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_songlengthsdatabaseservice PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

# Register test with CTest
add_test(NAME SonglengthsDatabaseServiceTest COMMAND test_songlengthsdatabaseservice)

# Add coverage target if coverage is enabled
if(BUILD_COVERAGE)
    add_coverage_target(songlengthsdatabaseservice test_songlengthsdatabaseservice)
    add_dependencies(coverage coverage_songlengthsdatabaseservice)
endif()

# FavoritesService tests - imperative shell around favorites:: core, QSettings I/O
add_executable(test_favoritesservice
    test_favoritesservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/favoritesservice.cpp
    ${CMAKE_SOURCE_DIR}/src/core/favoritescore.cpp
)

target_include_directories(test_favoritesservice PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_favoritesservice PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME FavoritesServiceTest COMMAND test_favoritesservice)

if(BUILD_COVERAGE)
    add_coverage_target(favoritesservice test_favoritesservice)
    add_dependencies(coverage coverage_favoritesservice)
endif()

# DiskBootSequenceService tests - imperative shell around diskboot:: state machine
add_executable(test_diskbootsequenceservice
    test_diskbootsequenceservice.cpp
    mocks/mockrestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/diskbootsequenceservice.cpp
    ${CMAKE_SOURCE_DIR}/src/core/diskbootsequencecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
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

# HttpFileDownloaderService tests - thin HTTP downloader shell, uses local QTcpServer
add_executable(test_httpfiledownloader
    test_httpfiledownloader.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ifiledownloaderservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/httpfiledownloaderservice.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
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

# StreamControlService tests - TCP stream control client
add_executable(test_streamcontrolservice
    test_streamcontrolservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/istreamcontrolservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/streamcontrolservice.cpp
    ${CMAKE_SOURCE_DIR}/src/core/streamprotocolcore.cpp
)

target_include_directories(test_streamcontrolservice PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_streamcontrolservice PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

add_test(NAME StreamControlServiceTest COMMAND test_streamcontrolservice)

if(BUILD_COVERAGE)
    add_coverage_target(streamcontrolservice test_streamcontrolservice)
    add_dependencies(coverage coverage_streamcontrolservice)
endif()

# UdpStreamSocket tests - shared UDP socket lifecycle helper
add_executable(test_udpstreamsocket
    test_udpstreamsocket.cpp
    ${CMAKE_SOURCE_DIR}/src/services/udpstreamsocket.cpp
)

target_include_directories(test_udpstreamsocket PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_udpstreamsocket PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

add_test(NAME UdpStreamSocketTest COMMAND test_udpstreamsocket)

if(BUILD_COVERAGE)
    add_coverage_target(udpstreamsocket test_udpstreamsocket)
    add_dependencies(coverage coverage_udpstreamsocket)
endif()

# AudioStreamReceiverService tests - UDP audio receiver with jitter buffer
add_executable(test_audiostreamreceiverservice
    test_audiostreamreceiverservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iaudiostreamreceiverservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/udpstreamsocket.cpp
    ${CMAKE_SOURCE_DIR}/src/services/audiostreamreceiverservice.cpp
    ${CMAKE_SOURCE_DIR}/src/core/audiostreamcore.cpp
)

target_include_directories(test_audiostreamreceiverservice PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_audiostreamreceiverservice PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

add_test(NAME AudioStreamReceiverServiceTest COMMAND test_audiostreamreceiverservice)

if(BUILD_COVERAGE)
    add_coverage_target(audiostreamreceiverservice test_audiostreamreceiverservice)
    add_dependencies(coverage coverage_audiostreamreceiverservice)
endif()

# AudioPlaybackService tests - Qt Multimedia audio playback service
add_executable(test_audioplaybackservice
    test_audioplaybackservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iaudioplaybackservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/audioplaybackservice.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
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
    ${CMAKE_SOURCE_DIR}/src/core/avicore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/vic2frameconverter.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ivideostreamreceiverservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iaudiostreamreceiverservice.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
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
    ${CMAKE_SOURCE_DIR}/src/core/filebrowsercore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/filetypecore.cpp
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
    ${CMAKE_SOURCE_DIR}/src/core/favoritesuicore.cpp
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

# TransferEventHandler tests - isolated unit tests for deferred event queue
add_executable(test_transfereventhandler
    test_transfereventhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfereventhandler.cpp
)

target_include_directories(test_transfereventhandler PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_transfereventhandler PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME TransferEventHandlerTest COMMAND test_transfereventhandler)

if(BUILD_COVERAGE)
    add_coverage_target(transfereventhandler test_transfereventhandler)
    add_dependencies(coverage coverage_transfereventhandler)
endif()

# TransferDispatchHandler tests - processNext dispatch loop
add_executable(test_transferdispatchhandler
    test_transferdispatchhandler.cpp
    ${MOCK_FTP_SOURCES}
    ${MOCK_FTP_HEADERS}
    ${MOCK_LOCAL_FS_SOURCES}
    ${MOCK_LOCAL_FS_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ilocalfilesystemservice.cpp
    ${TRANSFER_CORE_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/models/transferdispatchhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/recursivescancoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/remotedirectorycoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/folderoperationcoordinator.cpp
)

target_include_directories(test_transferdispatchhandler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_transferdispatchhandler PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

add_test(NAME TransferDispatchHandlerTest COMMAND test_transferdispatchhandler)

if(BUILD_COVERAGE)
    add_coverage_target(transferdispatchhandler test_transferdispatchhandler)
    add_dependencies(coverage coverage_transferdispatchhandler)
endif()

# SingleFileEnqueueHandler tests - single-file upload/download enqueue logic
add_executable(test_singlefileenqueuehandler
    test_singlefileenqueuehandler.cpp
    ${MOCK_LOCAL_FS_SOURCES}
    ${MOCK_LOCAL_FS_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ilocalfilesystemservice.cpp
    ${TRANSFER_CORE_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/models/singlefileenqueuehandler.cpp
)

target_include_directories(test_singlefileenqueuehandler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_singlefileenqueuehandler PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME SingleFileEnqueueHandlerTest COMMAND test_singlefileenqueuehandler)

if(BUILD_COVERAGE)
    add_coverage_target(singlefileenqueuehandler test_singlefileenqueuehandler)
    add_dependencies(coverage coverage_singlefileenqueuehandler)
endif()

# TransferFtpHandler tests - FTP event translation layer
add_executable(test_transferftphandler
    test_transferftphandler.cpp
    ${MOCK_FTP_SOURCES}
    ${MOCK_FTP_HEADERS}
    ${MOCK_LOCAL_FS_SOURCES}
    ${MOCK_LOCAL_FS_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ilocalfilesystemservice.cpp
    ${TRANSFER_CORE_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/models/transferftphandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfertimeoutmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/recursivescancoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/remotedirectorycoordinator.cpp
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

# ScreenshotService tests - captures QImage to temp directory
add_executable(test_screenshotservice
    test_screenshotservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/screenshotservice.cpp
)

target_include_directories(test_screenshotservice PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_screenshotservice PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
)

add_test(NAME ScreenshotServiceTest COMMAND test_screenshotservice)

if(BUILD_COVERAGE)
    add_coverage_target(screenshotservice test_screenshotservice)
    add_dependencies(coverage coverage_screenshotservice)
endif()

# FileActionController tests - dispatch logic, guard conditions, action state management
add_executable(test_fileactioncontroller
    test_fileactioncontroller.cpp
    stubs/playlistservice_test_stubs.cpp
    mocks/mockrestclient.cpp
    mocks/mockftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/deviceconnectionmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/services/deviceactionservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64urestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/restresponsecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64uftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ftpcore.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftpcommandqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftptransferstate.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftpresponsehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/core/filetypecore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/filebrowsercore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/playlistservice.cpp
    ${CMAKE_SOURCE_DIR}/src/core/playlistcore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/diskbootsequenceservice.cpp
    ${CMAKE_SOURCE_DIR}/src/core/diskbootsequencecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/configfileloaderservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/errorhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/services/statusmessageservice.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/fileactioncontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/models/remotefilemodel.cpp
    # Headers listed so AUTOMOC generates moc files for stubs
    ${CMAKE_SOURCE_DIR}/src/services/streamingservice.h
    ${CMAKE_SOURCE_DIR}/src/services/songlengthsdatabaseservice.h
)

target_include_directories(test_fileactioncontroller PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}  # tests/ dir - for mocks/
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_fileactioncontroller PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Widgets
)

add_test(NAME FileActionControllerTest COMMAND test_fileactioncontroller)

if(BUILD_COVERAGE)
    add_coverage_target(fileactioncontroller test_fileactioncontroller)
    add_dependencies(coverage coverage_fileactioncontroller)
endif()

# ExploreNavigationController tests - navigation logic with MockNavigationView
add_executable(test_explorenavigationcontroller
    test_explorenavigationcontroller.cpp
    stubs/explorenavigationcontroller_test_stubs.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/explorenavigationcontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    # Headers listed so AUTOMOC generates moc files for stubbed classes
    ${CMAKE_SOURCE_DIR}/src/models/remotefilemodel.h
    ${CMAKE_SOURCE_DIR}/src/services/deviceconnectionmanager.h
    ${CMAKE_SOURCE_DIR}/src/ui/explorefavoritescontroller.h
)

target_include_directories(test_explorenavigationcontroller PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_explorenavigationcontroller PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Widgets
)

add_test(NAME ExploreNavigationControllerTest COMMAND test_explorenavigationcontroller)

if(BUILD_COVERAGE)
    add_coverage_target(explorenavigationcontroller test_explorenavigationcontroller)
    add_dependencies(coverage coverage_explorenavigationcontroller)
endif()

# PreviewCoordinator tests - routing logic via IDetailsDisplay mock
add_executable(test_previewcoordinator
    test_previewcoordinator.cpp
    stubs/playlistservice_test_stubs.cpp
    ${MOCK_FTP_SOURCES}
    ${MOCK_FTP_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/filepreviewservice.cpp
    ${CMAKE_SOURCE_DIR}/src/core/fileactioncore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/filetypecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/playlistservice.cpp
    ${CMAKE_SOURCE_DIR}/src/core/playlistcore.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/previewcoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/models/remotefilemodel.cpp
    # Headers listed so AUTOMOC generates moc files for stubs
    ${CMAKE_SOURCE_DIR}/src/services/streamingservice.h
    ${CMAKE_SOURCE_DIR}/src/services/songlengthsdatabaseservice.h
)

target_include_directories(test_previewcoordinator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_previewcoordinator PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Test
    Qt6::Network
)

add_test(NAME PreviewCoordinatorTest COMMAND test_previewcoordinator)

if(BUILD_COVERAGE)
    add_coverage_target(previewcoordinator test_previewcoordinator)
    add_dependencies(coverage coverage_previewcoordinator)
endif()

# ConnectionUIController tests - verifies IConnectionStatusView injection and action management
add_executable(test_connectionuicontroller
    test_connectionuicontroller.cpp
    mocks/mockrestclient.cpp
    ${MOCK_FTP_SOURCES}
    ${MOCK_FTP_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/deviceconnectionmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64urestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/restresponsecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64uftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ftpcore.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftpcommandqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftptransferstate.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftpresponsehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/connectionuicontroller.cpp
)
target_include_directories(test_connectionuicontroller PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_connectionuicontroller PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Gui
)
add_test(NAME ConnectionUIControllerTest COMMAND test_connectionuicontroller)
if(BUILD_COVERAGE)
    add_coverage_target(connectionuicontroller test_connectionuicontroller)
    add_dependencies(coverage coverage_connectionuicontroller)
endif()

# DatabaseDownloadController tests - verifies IMessagePresenter injection (headless)
add_executable(test_databasedownloadcontroller
    test_databasedownloadcontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/databasedownloadcontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/databasedownloadview.cpp
    ${CMAKE_SOURCE_DIR}/src/services/cacheddownloadservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/songlengthsdatabaseservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/songlengthsparser.cpp
    ${CMAKE_SOURCE_DIR}/src/services/hvscmetadataservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/hvscparser.cpp
    ${CMAKE_SOURCE_DIR}/src/services/gamebase64service.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ifiledownloaderservice.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
)

target_include_directories(test_databasedownloadcontroller PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_databasedownloadcontroller PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    Qt6::Network
    Qt6::Sql
    ZLIB::ZLIB
)

add_test(NAME DatabaseDownloadControllerTest COMMAND test_databasedownloadcontroller)

if(BUILD_COVERAGE)
    add_coverage_target(databasedownloadcontroller test_databasedownloadcontroller)
    add_dependencies(coverage coverage_databasedownloadcontroller)
endif()

# PanelCoordinator tests - verifies IPanel signal routing, mode switching, and coordinator logic
add_executable(test_panelcoordinator
    test_panelcoordinator.cpp
    stubs/errorsourceconnector_stub.cpp
    stubs/panelcoordinator_test_stubs.cpp
    mocks/mockrestclient.cpp
    ${MOCK_FTP_SOURCES}
    ${MOCK_FTP_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/deviceconnectionmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64urestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/restresponsecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64uftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ftpcore.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftpcommandqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftptransferstate.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftpresponsehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/services/statusmessageservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/errorhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/core/filetypecore.cpp
    ${CMAKE_SOURCE_DIR}/src/models/remotefilemodel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/panelcoordinator.cpp
    # Header listed so AUTOMOC generates moc file for TransferService (Q_OBJECT vtable)
    ${CMAKE_SOURCE_DIR}/src/services/transferservice.h
)

target_include_directories(test_panelcoordinator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_panelcoordinator PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Widgets
)

add_test(NAME PanelCoordinatorTest COMMAND test_panelcoordinator)

if(BUILD_COVERAGE)
    add_coverage_target(panelcoordinator test_panelcoordinator)
    add_dependencies(coverage coverage_panelcoordinator)
endif()

# ExplorePanelCore tests - pure action enablement and drive display logic
add_executable(test_explorepanelcore
    test_explorepanelcore.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/explorepanelcore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/filetypecore.cpp
)

target_include_directories(test_explorepanelcore PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_explorepanelcore PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME ExplorePanelCoreTest COMMAND test_explorepanelcore)

if(BUILD_COVERAGE)
    add_coverage_target(explorepanelcore test_explorepanelcore)
    add_dependencies(coverage coverage_explorepanelcore)
endif()

# ExploreContextMenuController tests - action enablement state via prepareMenu()
add_executable(test_explorecontextmenucontroller
    test_explorecontextmenucontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/explorecontextmenucontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/explorepanelcore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/filetypecore.cpp
)

target_include_directories(test_explorecontextmenucontroller PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_explorecontextmenucontroller PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Test
)

add_test(NAME ExploreContextMenuControllerTest COMMAND test_explorecontextmenucontroller)

if(BUILD_COVERAGE)
    add_coverage_target(explorecontextmenucontroller test_explorecontextmenucontroller)
    add_dependencies(coverage coverage_explorecontextmenucontroller)
endif()

# ExploreFavoritesController tests - signal emissions and menu rebuilding
add_executable(test_explorefavoritescontroller
    test_explorefavoritescontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/explorefavoritescontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/services/favoritesservice.cpp
    ${CMAKE_SOURCE_DIR}/src/core/favoritescore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/favoritesuicore.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
)

target_include_directories(test_explorefavoritescontroller PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_explorefavoritescontroller PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Test
)

add_test(NAME ExploreFavoritesControllerTest COMMAND test_explorefavoritescontroller)

if(BUILD_COVERAGE)
    add_coverage_target(explorefavoritescontroller test_explorefavoritescontroller)
    add_dependencies(coverage coverage_explorefavoritescontroller)
endif()

# ConfigurationService tests - delegation and signal forwarding via DeviceConnectionManager/IRestClient
add_executable(test_configurationservice
    test_configurationservice.cpp
    mocks/mockrestclient.cpp
    mocks/mockftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/configurationservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/deviceconnectionmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64urestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/restresponsecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64uftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ftpcore.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftpcommandqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftptransferstate.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftpresponsehandler.cpp
)

target_include_directories(test_configurationservice PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_configurationservice PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

add_test(NAME ConfigurationServiceTest COMMAND test_configurationservice)

if(BUILD_COVERAGE)
    add_coverage_target(configurationservice test_configurationservice)
    add_dependencies(coverage coverage_configurationservice)
endif()

# DeviceActionService tests - guard clauses and operationNotAvailable signal
add_executable(test_deviceactionservice
    test_deviceactionservice.cpp
    mocks/mockrestclient.cpp
    mocks/mockftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/deviceactionservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/deviceconnectionmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64urestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/restresponsecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64uftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ftpcore.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftpcommandqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftptransferstate.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftpresponsehandler.cpp
)

target_include_directories(test_deviceactionservice PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_deviceactionservice PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

add_test(NAME DeviceActionServiceTest COMMAND test_deviceactionservice)

if(BUILD_COVERAGE)
    add_coverage_target(deviceactionservice test_deviceactionservice)
    add_dependencies(coverage coverage_deviceactionservice)
endif()

# ConfigPanel tests - verifies ConfigPanel wiring through ConfigurationService
add_executable(test_configpanel
    test_configpanel.cpp
    mocks/mockrestclient.cpp
    mocks/mockftpclient.cpp
    ${MOCK_FTP_SOURCES}
    ${MOCK_FTP_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/configurationservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/deviceconnectionmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64urestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/restresponsecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64uftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ftpcore.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftpcommandqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftptransferstate.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftpresponsehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/configurationmodel.cpp
    ${CMAKE_SOURCE_DIR}/src/services/errorhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/services/statusmessageservice.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/configpanel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/configitemspanel.cpp
)

target_include_directories(test_configpanel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_configpanel PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Widgets
)

add_test(NAME ConfigPanelTest COMMAND test_configpanel)

if(BUILD_COVERAGE)
    add_coverage_target(configpanel test_configpanel)
    add_dependencies(coverage coverage_configpanel)
endif()

# ServiceFactory tests - verifies construction and wiring of all application services
add_executable(test_servicefactory
    test_servicefactory.cpp
    stubs/errorsourceconnector_stub.cpp
    stubs/streamingservice_stub.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/servicefactory.cpp
    ${CMAKE_SOURCE_DIR}/src/services/configurationservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/deviceactionservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/remotefileoperationsservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/systemcommandcontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/services/deviceconnectionmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64urestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/restresponsecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64uftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ftpcore.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftpcommandqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftptransferstate.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftpresponsehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/services/configfileloaderservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/filepreviewservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/transferservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/errorhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/services/statusmessageservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/favoritesservice.cpp
    ${CMAKE_SOURCE_DIR}/src/core/favoritescore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/playlistservice.cpp
    ${CMAKE_SOURCE_DIR}/src/core/playlistcore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ifiledownloaderservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/httpfiledownloaderservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/cacheddownloadservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/songlengthsdatabaseservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/songlengthsparser.cpp
    ${CMAKE_SOURCE_DIR}/src/services/hvscmetadataservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/hvscparser.cpp
    ${CMAKE_SOURCE_DIR}/src/services/gamebase64service.cpp
    ${CMAKE_SOURCE_DIR}/src/core/filetypecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ilocalfilesystemservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/localfilesystemservice.cpp
    ${TRANSFER_CORE_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/models/remotefilemodel.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfermanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferwiring.cpp
    ${CMAKE_SOURCE_DIR}/src/models/singlefileenqueuehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferdispatchhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferdeletehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferftphandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/batchmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfertimeoutmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfereventhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/recursivescancoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/remotedirectorycoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/folderoperationcoordinator.cpp
    # Headers listed so AUTOMOC generates moc files for stubs
    ${CMAKE_SOURCE_DIR}/src/services/streamingservice.h
    ${CMAKE_SOURCE_DIR}/src/services/songlengthsdatabaseservice.h
)

target_include_directories(test_servicefactory PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_servicefactory PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Widgets
    Qt6::Sql
    ZLIB::ZLIB
)

add_test(NAME ServiceFactoryTest COMMAND test_servicefactory)

if(BUILD_COVERAGE)
    add_coverage_target(servicefactory test_servicefactory)
    add_dependencies(coverage coverage_servicefactory)
endif()

# LocalFileOperationsService tests - uses QTemporaryDir for real filesystem isolation
add_executable(test_localfileoperationsservice
    test_localfileoperationsservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/localfileoperationsservice.cpp
)

target_include_directories(test_localfileoperationsservice PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_localfileoperationsservice PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME LocalFileOperationsServiceTest COMMAND test_localfileoperationsservice)

if(BUILD_COVERAGE)
    add_coverage_target(localfileoperationsservice test_localfileoperationsservice)
    add_dependencies(coverage coverage_localfileoperationsservice)
endif()

# PlaylistWidget tests - control-flow and signal forwarding using null DeviceConnection
add_executable(test_playlistwidget
    test_playlistwidget.cpp
    stubs/playlistservice_test_stubs.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ifiledownloaderservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/playlistservice.cpp
    ${CMAKE_SOURCE_DIR}/src/core/playlistcore.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/playlistwidget.cpp
    # Headers listed so AUTOMOC generates moc files for stubs
    ${CMAKE_SOURCE_DIR}/src/services/streamingservice.h
    ${CMAKE_SOURCE_DIR}/src/services/songlengthsdatabaseservice.h
)

target_include_directories(test_playlistwidget PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_playlistwidget PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

add_test(NAME PlaylistWidgetTest COMMAND test_playlistwidget)

if(BUILD_COVERAGE)
    add_coverage_target(playlistwidget test_playlistwidget)
    add_dependencies(coverage coverage_playlistwidget)
endif()

# ViewPanel tests - control-flow and guard clause tests using stub services
add_executable(test_viewpanel
    test_viewpanel.cpp
    stubs/viewpanel_test_stubs.cpp
    mocks/mockrestclient.cpp
    mocks/mockftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/deviceconnectionmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64urestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/restresponsecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64uftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ftpcore.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftpcommandqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftptransferstate.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftpresponsehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/services/screenshotservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/errorhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/services/statusmessageservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ivideostreamreceiverservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iaudiostreamreceiverservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/keyboardinputservice.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/viewpanel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/videodisplaywidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/streamingdiagnosticswidget.cpp
    # Headers for AUTOMOC (stubs provide bodies for these Q_OBJECT classes)
    ${CMAKE_SOURCE_DIR}/src/services/streamingservice.h
    ${CMAKE_SOURCE_DIR}/src/services/videorecordingservice.h
    ${CMAKE_SOURCE_DIR}/src/services/streamingdiagnosticsservice.h
)

target_include_directories(test_viewpanel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_viewpanel PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    Qt6::Network
)

add_test(NAME ViewPanelTest COMMAND test_viewpanel)

if(BUILD_COVERAGE)
    add_coverage_target(viewpanel test_viewpanel)
    add_dependencies(coverage coverage_viewpanel)
endif()

# TransferPanel tests - settings round-trip and state propagation
add_executable(test_transferpanel
    test_transferpanel.cpp
    mocks/mockrestclient.cpp
    mocks/mockftpclient.cpp
    ${MOCK_FTP_SOURCES}
    ${MOCK_FTP_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/deviceconnectionmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64urestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/restresponsecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64uftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ftpcore.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftpcommandqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftptransferstate.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftpresponsehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/services/errorhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/services/statusmessageservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/remotefileoperationsservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/transferservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ilocalfilesystemservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/localfilesystemservice.cpp
    ${TRANSFER_CORE_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/core/filetypecore.cpp
    ${CMAKE_SOURCE_DIR}/src/models/remotefilemodel.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfermanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferwiring.cpp
    ${CMAKE_SOURCE_DIR}/src/models/singlefileenqueuehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferdispatchhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferdeletehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferftphandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/batchmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfertimeoutmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfereventhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/localfileproxymodel.cpp
    ${CMAKE_SOURCE_DIR}/src/services/localfileoperationsservice.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/recursivescancoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/remotedirectorycoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/folderoperationcoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/transferpanel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/remotefilebrowserwidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/remotefilebrowsercontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/refreshpolicymanager.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/localfilebrowserwidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filebrowserwidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/pathnavigationwidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/transferprogresscontainer.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/batchprogresswidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/transferqueuewidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/transferconfirmationdialogs.cpp
)

target_include_directories(test_transferpanel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_transferpanel PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    Qt6::Network
)

add_test(NAME TransferPanelTest COMMAND test_transferpanel)

if(BUILD_COVERAGE)
    add_coverage_target(transferpanel test_transferpanel)
    add_dependencies(coverage coverage_transferpanel)
endif()

# RemoteFileBrowserWidget tests - directory navigation and guard clauses
add_executable(test_remotefilebrowserwidget
    test_remotefilebrowserwidget.cpp
    mocks/mockrestclient.cpp
    mocks/mockftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/filetypecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/errorhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/services/statusmessageservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/remotefileoperationsservice.cpp
    ${CMAKE_SOURCE_DIR}/src/models/remotefilemodel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/remotefilebrowserwidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/remotefilebrowsercontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/refreshpolicymanager.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filebrowserwidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/pathnavigationwidget.cpp
)

target_include_directories(test_remotefilebrowserwidget PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_remotefilebrowserwidget PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    Qt6::Network
)

add_test(NAME RemoteFileBrowserWidgetTest COMMAND test_remotefilebrowserwidget)

if(BUILD_COVERAGE)
    add_coverage_target(remotefilebrowserwidget test_remotefilebrowserwidget)
    add_dependencies(coverage coverage_remotefilebrowserwidget)
endif()

# LocalFileBrowserWidget tests - selectedPath guards, setCurrentDirectory
add_executable(test_localfilebrowserwidget
    test_localfilebrowserwidget.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/errorhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/services/statusmessageservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/localfileoperationsservice.cpp
    ${CMAKE_SOURCE_DIR}/src/core/filetypecore.cpp
    ${CMAKE_SOURCE_DIR}/src/models/localfileproxymodel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/localfilebrowserwidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filebrowserwidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/pathnavigationwidget.cpp
)

target_include_directories(test_localfilebrowserwidget PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_localfilebrowserwidget PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

add_test(NAME LocalFileBrowserWidgetTest COMMAND test_localfilebrowserwidget)

if(BUILD_COVERAGE)
    add_coverage_target(localfilebrowserwidget test_localfilebrowserwidget)
    add_dependencies(coverage coverage_localfilebrowserwidget)
endif()

# FileDetailsPanel tests - file type detection and stacked widget dispatch
add_executable(test_filedetailspanel
    test_filedetailspanel.cpp
    stubs/filedetailspanel_test_stubs.cpp
    # Headers listed so AUTOMOC generates moc files for Q_OBJECT stubs
    ${CMAKE_SOURCE_DIR}/src/services/gamebase64service.h
    ${CMAKE_SOURCE_DIR}/src/services/hvscmetadataservice.h
    ${CMAKE_SOURCE_DIR}/src/services/songlengthsdatabaseservice.h
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ifiledownloaderservice.cpp
    ${CMAKE_SOURCE_DIR}/src/core/fileactioncore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/filetypecore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/filemetadatacore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/diskimagereader.cpp
    ${CMAKE_SOURCE_DIR}/src/core/sidfileparser.cpp
    ${CMAKE_SOURCE_DIR}/src/core/petsciiconverter.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filedetailspanel.cpp
)

target_include_directories(test_filedetailspanel PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_filedetailspanel PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    Qt6::Sql
)

add_test(NAME FileDetailsPanelTest COMMAND test_filedetailspanel)

if(BUILD_COVERAGE)
    add_coverage_target(filedetailspanel test_filedetailspanel)
    add_dependencies(coverage coverage_filedetailspanel)
endif()

# FilePreviewFactory tests - strategy selection by file extension
add_executable(test_filepreviewfactory
    test_filepreviewfactory.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filepreview/c64previewbase.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filepreview/filepreviewfactory.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filepreview/textfilepreview.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filepreview/diskimagepreview.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filepreview/sidfilepreview.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filepreview/defaultfilepreview.cpp
    ${CMAKE_SOURCE_DIR}/src/core/diskimagereader.cpp
    ${CMAKE_SOURCE_DIR}/src/core/sidfileparser.cpp
    ${CMAKE_SOURCE_DIR}/src/core/petsciiconverter.cpp
    ${CMAKE_SOURCE_DIR}/src/core/fileactioncore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/filetypecore.cpp
)

target_include_directories(test_filepreviewfactory BEFORE PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_filepreviewfactory PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Widgets
)

add_test(NAME FilePreviewFactoryTest COMMAND test_filepreviewfactory)

if(BUILD_COVERAGE)
    add_coverage_target(filepreviewfactory test_filepreviewfactory)
    add_dependencies(coverage coverage_filepreviewfactory)
endif()

# TransferQueueWidget tests - null guard and button state management
add_executable(test_transferqueuewidget
    test_transferqueuewidget.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ilocalfilesystemservice.cpp
    ${TRANSFER_CORE_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/services/localfilesystemservice.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfermanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferwiring.cpp
    ${CMAKE_SOURCE_DIR}/src/models/singlefileenqueuehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferdispatchhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferdeletehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferftphandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/batchmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfertimeoutmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfereventhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/recursivescancoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/remotedirectorycoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/folderoperationcoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/transferqueuewidget.cpp
)

target_include_directories(test_transferqueuewidget PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_transferqueuewidget PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    Qt6::Network
)

add_test(NAME TransferQueueWidgetTest COMMAND test_transferqueuewidget)

if(BUILD_COVERAGE)
    add_coverage_target(transferqueuewidget test_transferqueuewidget)
    add_dependencies(coverage coverage_transferqueuewidget)
endif()

# ConnectionTestHandler tests - injectable IRestClient for guard clause testing
add_executable(test_connectiontesthandler
    test_connectiontesthandler.cpp
    mocks/mockrestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64urestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/restresponsecore.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/connectiontesthandler.cpp
)

target_include_directories(test_connectiontesthandler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_connectiontesthandler PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    Qt6::Network
)

add_test(NAME ConnectionTestHandlerTest COMMAND test_connectiontesthandler)

if(BUILD_COVERAGE)
    add_coverage_target(connectiontesthandler test_connectiontesthandler)
    add_dependencies(coverage coverage_connectiontesthandler)
endif()

# BatchProgressWidget tests - state transitions and progress updates
add_executable(test_batchprogresswidget
    test_batchprogresswidget.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ilocalfilesystemservice.cpp
    ${TRANSFER_CORE_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/services/localfilesystemservice.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfermanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferwiring.cpp
    ${CMAKE_SOURCE_DIR}/src/models/singlefileenqueuehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferdispatchhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferdeletehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferftphandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/batchmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfertimeoutmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfereventhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/recursivescancoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/remotedirectorycoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/folderoperationcoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/batchprogresswidget.cpp
)

target_include_directories(test_batchprogresswidget PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_batchprogresswidget PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    Qt6::Network
)

add_test(NAME BatchProgressWidgetTest COMMAND test_batchprogresswidget)

if(BUILD_COVERAGE)
    add_coverage_target(batchprogresswidget test_batchprogresswidget)
    add_dependencies(coverage coverage_batchprogresswidget)
endif()

# ConfigItemsPanel tests - category switching and item display
add_executable(test_configitemspanel
    test_configitemspanel.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/models/configurationmodel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/configitemspanel.cpp
)

target_include_directories(test_configitemspanel PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_configitemspanel PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

add_test(NAME ConfigItemsPanelTest COMMAND test_configitemspanel)

if(BUILD_COVERAGE)
    add_coverage_target(configitemspanel test_configitemspanel)
    add_dependencies(coverage coverage_configitemspanel)
endif()

# ConnectionStatusWidget tests - display state and label visibility
add_executable(test_connectionstatuswidget
    test_connectionstatuswidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/connectionstatuswidget.cpp
)

target_include_directories(test_connectionstatuswidget PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_connectionstatuswidget PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

add_test(NAME ConnectionStatusWidgetTest COMMAND test_connectionstatuswidget)

if(BUILD_COVERAGE)
    add_coverage_target(connectionstatuswidget test_connectionstatuswidget)
    add_dependencies(coverage coverage_connectionstatuswidget)
endif()

# DriveStatusWidget tests - mount state and signal emission
add_executable(test_drivestatuswidget
    test_drivestatuswidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/drivestatuswidget.cpp
)

target_include_directories(test_drivestatuswidget PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_drivestatuswidget PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

add_test(NAME DriveStatusWidgetTest COMMAND test_drivestatuswidget)

if(BUILD_COVERAGE)
    add_coverage_target(drivestatuswidget test_drivestatuswidget)
    add_dependencies(coverage coverage_drivestatuswidget)
endif()

# PathNavigationWidget tests - path tracking and up-click signal
add_executable(test_pathnavigationwidget
    test_pathnavigationwidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/pathnavigationwidget.cpp
)

target_include_directories(test_pathnavigationwidget PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_pathnavigationwidget PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

add_test(NAME PathNavigationWidgetTest COMMAND test_pathnavigationwidget)

if(BUILD_COVERAGE)
    add_coverage_target(pathnavigationwidget test_pathnavigationwidget)
    add_dependencies(coverage coverage_pathnavigationwidget)
endif()

# VideoDisplayWidget tests - scaling mode, frame pacing, sizeHint
add_executable(test_videodisplaywidget
    test_videodisplaywidget.cpp
    stubs/viewpanel_test_stubs.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ivideostreamreceiverservice.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/videodisplaywidget.cpp
    # Headers for AUTOMOC (stubs provide bodies for these Q_OBJECT classes)
    ${CMAKE_SOURCE_DIR}/src/services/streamingservice.h
    ${CMAKE_SOURCE_DIR}/src/services/videorecordingservice.h
    ${CMAKE_SOURCE_DIR}/src/services/streamingdiagnosticsservice.h
)

target_include_directories(test_videodisplaywidget PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_videodisplaywidget PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    Qt6::Network
)

add_test(NAME VideoDisplayWidgetTest COMMAND test_videodisplaywidget)

if(BUILD_COVERAGE)
    add_coverage_target(videodisplaywidget test_videodisplaywidget)
    add_dependencies(coverage coverage_videodisplaywidget)
endif()

# StreamingDiagnosticsWidget tests - display mode switching
add_executable(test_streamingdiagnosticswidget
    test_streamingdiagnosticswidget.cpp
    stubs/viewpanel_test_stubs.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ivideostreamreceiverservice.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/streamingdiagnosticswidget.cpp
    # Headers for AUTOMOC (stubs provide bodies for these Q_OBJECT classes)
    ${CMAKE_SOURCE_DIR}/src/services/streamingservice.h
    ${CMAKE_SOURCE_DIR}/src/services/videorecordingservice.h
    ${CMAKE_SOURCE_DIR}/src/services/streamingdiagnosticsservice.h
)

target_include_directories(test_streamingdiagnosticswidget PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_streamingdiagnosticswidget PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    Qt6::Network
)

add_test(NAME StreamingDiagnosticsWidgetTest COMMAND test_streamingdiagnosticswidget)

if(BUILD_COVERAGE)
    add_coverage_target(streamingdiagnosticswidget test_streamingdiagnosticswidget)
    add_dependencies(coverage coverage_streamingdiagnosticswidget)
endif()

# C64PreviewBase and DefaultFilePreview tests - widget creation and file detail formatting
add_executable(test_c64previewbase
    test_c64previewbase.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filepreview/c64previewbase.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filepreview/defaultfilepreview.cpp
)

target_include_directories(test_c64previewbase PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_c64previewbase PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

add_test(NAME C64PreviewBaseTest COMMAND test_c64previewbase)

if(BUILD_COVERAGE)
    add_coverage_target(c64previewbase test_c64previewbase)
    add_dependencies(coverage coverage_c64previewbase)
endif()

# TransferProgressWidget tests - visibility transitions and signal emission
add_executable(test_transferprogresswidget
    test_transferprogresswidget.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/deviceconnectionmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64urestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/restresponsecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64uftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ftpcore.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftpcommandqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftptransferstate.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftpresponsehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/services/errorhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/services/statusmessageservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/transferservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ilocalfilesystemservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/localfilesystemservice.cpp
    ${TRANSFER_CORE_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/models/transferqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfermanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferwiring.cpp
    ${CMAKE_SOURCE_DIR}/src/models/singlefileenqueuehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferdispatchhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferdeletehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferftphandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/batchmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfertimeoutmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfereventhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/recursivescancoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/remotedirectorycoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/folderoperationcoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/transferprogresswidget.cpp
)

target_include_directories(test_transferprogresswidget PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_transferprogresswidget PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    Qt6::Network
)

add_test(NAME TransferProgressWidgetTest COMMAND test_transferprogresswidget)

if(BUILD_COVERAGE)
    add_coverage_target(transferprogresswidget test_transferprogresswidget)
    add_dependencies(coverage coverage_transferprogresswidget)
endif()

# TransferProgressContainer tests - construction and service wiring
add_executable(test_transferprogresscontainer
    test_transferprogresscontainer.cpp
    mocks/mockrestclient.cpp
    mocks/mockftpclient.cpp
    ${MOCK_FTP_SOURCES}
    ${MOCK_FTP_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/deviceconnectionmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64urestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/restresponsecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64uftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ftpcore.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftpcommandqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftptransferstate.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftpresponsehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/services/errorhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/services/statusmessageservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/transferservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ilocalfilesystemservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/localfilesystemservice.cpp
    ${TRANSFER_CORE_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/models/transferqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfermanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferwiring.cpp
    ${CMAKE_SOURCE_DIR}/src/models/singlefileenqueuehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferdispatchhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferdeletehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferftphandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/batchmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfertimeoutmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfereventhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/recursivescancoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/remotedirectorycoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/folderoperationcoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/transferprogresscontainer.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/batchprogresswidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/transferconfirmationdialogs.cpp
)

target_include_directories(test_transferprogresscontainer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_transferprogresscontainer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    Qt6::Network
)

add_test(NAME TransferProgressContainerTest COMMAND test_transferprogresscontainer)

if(BUILD_COVERAGE)
    add_coverage_target(transferprogresscontainer test_transferprogresscontainer)
    add_dependencies(coverage coverage_transferprogresscontainer)
endif()

# PreferencesDialog tests - construction and widget state with empty settings
add_executable(test_preferencesdialog
    test_preferencesdialog.cpp
    stubs/viewpanel_test_stubs.cpp
    stubs/filedetailspanel_test_stubs.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64urestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/restresponsecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/statusmessageservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/cacheddownloadservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ifiledownloaderservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ivideostreamreceiverservice.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/videodisplaywidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/preferencesdialog.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/connectiontesthandler.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/databasedownloadcontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/databasedownloadview.cpp
    # Headers for AUTOMOC (stubs provide bodies for these Q_OBJECT classes)
    ${CMAKE_SOURCE_DIR}/src/services/streamingservice.h
    ${CMAKE_SOURCE_DIR}/src/services/videorecordingservice.h
    ${CMAKE_SOURCE_DIR}/src/services/streamingdiagnosticsservice.h
    ${CMAKE_SOURCE_DIR}/src/services/songlengthsdatabaseservice.h
    ${CMAKE_SOURCE_DIR}/src/services/hvscmetadataservice.h
    ${CMAKE_SOURCE_DIR}/src/services/gamebase64service.h
)

if(APPLE)
    target_sources(test_preferencesdialog PRIVATE
        ${CMAKE_SOURCE_DIR}/src/services/platformkeychain_mac.mm
    )
else()
    target_sources(test_preferencesdialog PRIVATE
        ${CMAKE_SOURCE_DIR}/src/services/platformkeychain_stub.cpp
    )
endif()

target_include_directories(test_preferencesdialog PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_preferencesdialog PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    Qt6::Network
    Qt6::Sql
)

if(APPLE)
    target_link_libraries(test_preferencesdialog PRIVATE
        "-framework Security"
        "-framework Foundation"
    )
endif()

add_test(NAME PreferencesDialogTest COMMAND test_preferencesdialog)

if(BUILD_COVERAGE)
    add_coverage_target(preferencesdialog test_preferencesdialog)
    add_dependencies(coverage coverage_preferencesdialog)
endif()

# MenuBarBuilder tests - menu construction and refresh action
add_executable(test_menubarbuilder
    test_menubarbuilder.cpp
    mocks/mockrestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64urestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/restresponsecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/statusmessageservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/systemcommandcontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/menubarbuilder.cpp
)

target_include_directories(test_menubarbuilder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_menubarbuilder PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    Qt6::Network
)

add_test(NAME MenuBarBuilderTest COMMAND test_menubarbuilder)

if(BUILD_COVERAGE)
    add_coverage_target(menubarbuilder test_menubarbuilder)
    add_dependencies(coverage coverage_menubarbuilder)
endif()

# SystemToolBarBuilder tests - toolbar construction and result object population
add_executable(test_systemtoolbarbuilder
    test_systemtoolbarbuilder.cpp
    mocks/mockrestclient.cpp
    mocks/mockftpclient.cpp
    ${MOCK_FTP_SOURCES}
    ${MOCK_FTP_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/services/deviceconnectionmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64urestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/restresponsecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64uftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ftpcore.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftpcommandqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftptransferstate.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftpresponsehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/services/statusmessageservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/systemcommandcontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/connectionstatuswidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/connectionuicontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/systemtoolbarbuilder.cpp
)

target_include_directories(test_systemtoolbarbuilder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_systemtoolbarbuilder PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    Qt6::Network
)

add_test(NAME SystemToolBarBuilderTest COMMAND test_systemtoolbarbuilder)

if(BUILD_COVERAGE)
    add_coverage_target(systemtoolbarbuilder test_systemtoolbarbuilder)
    add_dependencies(coverage coverage_systemtoolbarbuilder)
endif()

# MainWindow integration tests - verifies construction, panel composition, and UI wiring
add_executable(test_mainwindow
    test_mainwindow.cpp
    stubs/viewpanel_test_stubs.cpp
    stubs/errorsourceconnector_stub.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/mainwindow.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/preferencesdialog.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/configitemspanel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/connectionstatuswidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/drivestatuswidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/pathnavigationwidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/transferqueuewidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filedetailspanel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/videodisplaywidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filebrowserwidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/localfilebrowserwidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/remotefilebrowserwidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/remotefilebrowsercontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/refreshpolicymanager.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/transferprogresswidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/batchprogresswidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/transferprogresscontainer.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/transferconfirmationdialogs.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/explorepanel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/explorepanelcore.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/fileactioncontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/dialogerrorpresenter.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/explorefavoritescontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/explorecontextmenucontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/explorenavigationcontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/navigationviewadapter.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/menubarbuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/systemtoolbarbuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/databasedownloadcontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/databasedownloadview.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/connectiontesthandler.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/previewcoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/panelcoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/connectionuicontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/transferpanel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/viewpanel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/configpanel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/playlistwidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filepreview/c64previewbase.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filepreview/textfilepreview.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filepreview/diskimagepreview.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filepreview/sidfilepreview.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/filepreview/defaultfilepreview.cpp
    ${CMAKE_SOURCE_DIR}/src/services/irestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64urestclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/c64uftpclient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/deviceconnectionmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/services/configfileloaderservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/filepreviewservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/transferservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/errorhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/services/statusmessageservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/istreamcontrolservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ivideostreamreceiverservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iaudiostreamreceiverservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/iaudioplaybackservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/keyboardinputservice.cpp
    ${CMAKE_SOURCE_DIR}/src/core/petsciiconverter.cpp
    ${CMAKE_SOURCE_DIR}/src/core/diskimagereader.cpp
    ${CMAKE_SOURCE_DIR}/src/core/sidfileparser.cpp
    ${CMAKE_SOURCE_DIR}/src/services/favoritesservice.cpp
    ${CMAKE_SOURCE_DIR}/src/core/favoritescore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/favoritesuicore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/filebrowsercore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/fileactioncore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/filetypecore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/diskbootsequencecore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/diskbootsequenceservice.cpp
    ${CMAKE_SOURCE_DIR}/src/core/filemetadatacore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ftpcore.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftpcommandqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/ftptransferstate.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ftpresponsehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/core/streamprotocolcore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/restresponsecore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/audiostreamcore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/videostreamcore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/avicore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/screenshotservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/localfileoperationsservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/remotefileoperationsservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/playlistservice.cpp
    ${CMAKE_SOURCE_DIR}/src/core/playlistcore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/transfercore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/transferconfirmcore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/transferlistingcore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/transferdecisioncore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/transferftpcore.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ilocalfilesystemservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/localfilesystemservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ifiledownloaderservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/httpfiledownloaderservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/cacheddownloadservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/songlengthsparser.cpp
    ${CMAKE_SOURCE_DIR}/src/services/songlengthsdatabaseservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/hvscparser.cpp
    ${CMAKE_SOURCE_DIR}/src/services/hvscmetadataservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/gamebase64service.cpp
    ${CMAKE_SOURCE_DIR}/src/services/servicefactory.cpp
    ${CMAKE_SOURCE_DIR}/src/services/configurationservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/deviceactionservice.cpp
    ${CMAKE_SOURCE_DIR}/src/services/systemcommandcontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/streamingdiagnosticswidget.cpp
    ${CMAKE_SOURCE_DIR}/src/models/remotefilemodel.cpp
    ${CMAKE_SOURCE_DIR}/src/models/localfileproxymodel.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferqueue.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfermanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferwiring.cpp
    ${CMAKE_SOURCE_DIR}/src/models/singlefileenqueuehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferdispatchhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferdeletehandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transferftphandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/batchmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfertimeoutmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/transfereventhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/models/configurationmodel.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/recursivescancoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/remotedirectorycoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ftp/folderoperationcoordinator.cpp
    # Headers for AUTOMOC (stubs provide bodies for these Q_OBJECT classes)
    ${CMAKE_SOURCE_DIR}/src/services/streamingservice.h
    ${CMAKE_SOURCE_DIR}/src/services/videorecordingservice.h
    ${CMAKE_SOURCE_DIR}/src/services/streamingdiagnosticsservice.h
    ${CMAKE_SOURCE_DIR}/src/services/songlengthsdatabaseservice.h
    ${CMAKE_SOURCE_DIR}/src/services/hvscmetadataservice.h
    ${CMAKE_SOURCE_DIR}/src/services/gamebase64service.h
)

if(APPLE)
    target_sources(test_mainwindow PRIVATE
        ${CMAKE_SOURCE_DIR}/src/services/platformkeychain_mac.mm
    )
else()
    target_sources(test_mainwindow PRIVATE
        ${CMAKE_SOURCE_DIR}/src/services/platformkeychain_stub.cpp
    )
endif()

target_include_directories(test_mainwindow PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_mainwindow PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
    Qt6::Sql
    Qt6::Test
    ZLIB::ZLIB
)

if(APPLE)
    target_link_libraries(test_mainwindow PRIVATE
        "-framework Security"
        "-framework Foundation"
    )
endif()

add_test(NAME MainWindowTest COMMAND test_mainwindow)

if(BUILD_COVERAGE)
    add_coverage_target(mainwindow test_mainwindow)
    add_dependencies(coverage coverage_mainwindow)
endif()

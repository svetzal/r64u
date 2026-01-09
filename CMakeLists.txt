cmake_minimum_required(VERSION 3.21)

project(r64u
    VERSION 0.3.0
    DESCRIPTION "Remote access tool for Commodore 64 Ultimate devices"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Export compile commands for clang-tidy and IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Quality tooling (formatting, static analysis, coverage, sanitizers)
include(cmake/Quality.cmake)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    WebEngineWidgets
)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/ui/preferencesdialog.cpp
    src/ui/transferqueuewidget.cpp
    src/ui/filedetailspanel.cpp
    src/services/c64urestclient.cpp
    src/services/c64uftpclient.cpp
    src/services/deviceconnection.cpp
    src/services/configfileloader.cpp
    src/services/streamcontrolclient.cpp
    src/models/remotefilemodel.cpp
    src/models/localfileproxymodel.cpp
    src/models/transferqueue.cpp
)

set(HEADERS
    src/mainwindow.h
    src/ui/preferencesdialog.h
    src/ui/transferqueuewidget.h
    src/ui/filedetailspanel.h
    src/services/c64urestclient.h
    src/services/c64uftpclient.h
    src/services/deviceconnection.h
    src/services/configfileloader.h
    src/services/credentialstore.h
    src/services/streamcontrolclient.h
    src/models/remotefilemodel.h
    src/models/localfileproxymodel.h
    src/models/transferqueue.h
)

# Platform-specific credential storage
if(APPLE)
    list(APPEND SOURCES src/services/credentialstore_mac.mm)
else()
    # Stub implementation for Windows/Linux (secure storage not yet implemented)
    list(APPEND SOURCES src/services/credentialstore_stub.cpp)
endif()

set(RESOURCES
    resources/r64u.qrc
)

if(APPLE)
    set(MACOSX_BUNDLE_ICON_FILE app.icns)
    set(APP_ICON_MACOS "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/app.icns")

    # Only include icon if it exists
    if(EXISTS ${APP_ICON_MACOS})
        set_source_files_properties(${APP_ICON_MACOS} PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources"
        )
        set(ICON_FILES ${APP_ICON_MACOS})
    else()
        set(ICON_FILES "")
    endif()

    add_executable(r64u MACOSX_BUNDLE
        ${SOURCES}
        ${HEADERS}
        ${RESOURCES}
        ${ICON_FILES}
    )

    set_target_properties(r64u PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.example.r64u"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
    )
elseif(WIN32)
    add_executable(r64u WIN32
        ${SOURCES}
        ${HEADERS}
        ${RESOURCES}
    )
else()
    add_executable(r64u
        ${SOURCES}
        ${HEADERS}
        ${RESOURCES}
    )
endif()

target_include_directories(r64u PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(r64u PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::WebEngineWidgets
)

# Platform-specific libraries
if(APPLE)
    target_link_libraries(r64u PRIVATE
        "-framework Security"
        "-framework Foundation"
    )
endif()

install(TARGETS r64u
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Tests
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

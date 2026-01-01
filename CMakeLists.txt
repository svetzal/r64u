cmake_minimum_required(VERSION 3.21)

project(r64u
    VERSION 0.2.0
    DESCRIPTION "Remote access tool for Commodore 64 Ultimate devices"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/ui/preferencesdialog.cpp
    src/ui/transferqueuewidget.cpp
    src/services/c64urestclient.cpp
    src/services/c64uftpclient.cpp
    src/services/deviceconnection.cpp
    src/models/remotefilemodel.cpp
    src/models/transferqueue.cpp
)

set(HEADERS
    src/mainwindow.h
    src/ui/preferencesdialog.h
    src/ui/transferqueuewidget.h
    src/services/c64urestclient.h
    src/services/c64uftpclient.h
    src/services/deviceconnection.h
    src/services/credentialstore.h
    src/models/remotefilemodel.h
    src/models/transferqueue.h
)

# Platform-specific credential storage
if(APPLE)
    list(APPEND SOURCES src/services/credentialstore_mac.mm)
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

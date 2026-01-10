cmake_minimum_required(VERSION 3.21)

project(r64u
    VERSION 0.5.0
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
    Multimedia
)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/ui/preferencesdialog.cpp
    src/ui/transferqueuewidget.cpp
    src/ui/filedetailspanel.cpp
    src/ui/videodisplaywidget.cpp
    src/services/c64urestclient.cpp
    src/services/c64uftpclient.cpp
    src/services/deviceconnection.cpp
    src/services/configfileloader.cpp
    src/services/streamcontrolclient.cpp
    src/services/videostreamreceiver.cpp
    src/services/audiostreamreceiver.cpp
    src/services/audioplaybackservice.cpp
    src/services/keyboardinputservice.cpp
    src/services/diskimagereader.cpp
    src/models/remotefilemodel.cpp
    src/models/localfileproxymodel.cpp
    src/models/transferqueue.cpp
)

set(HEADERS
    src/mainwindow.h
    src/ui/preferencesdialog.h
    src/ui/transferqueuewidget.h
    src/ui/filedetailspanel.h
    src/ui/videodisplaywidget.h
    src/services/c64urestclient.h
    src/services/c64uftpclient.h
    src/services/deviceconnection.h
    src/services/configfileloader.h
    src/services/credentialstore.h
    src/services/streamcontrolclient.h
    src/services/videostreamreceiver.h
    src/services/audiostreamreceiver.h
    src/services/audioplaybackservice.h
    src/services/keyboardinputservice.h
    src/services/diskimagereader.h
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
    # Windows resource file for embedding the application icon
    set(WIN_RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc")

    add_executable(r64u WIN32
        ${SOURCES}
        ${HEADERS}
        ${RESOURCES}
        ${WIN_RESOURCES}
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
    Qt6::Multimedia
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

# =============================================================================
# CPack Configuration - Platform installers
# =============================================================================
include(InstallRequiredSystemLibraries)

# Common package metadata
set(CPACK_PACKAGE_NAME "r64u")
set(CPACK_PACKAGE_VENDOR "r64u Project")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_DESCRIPTION "r64u provides remote access to Commodore 64 Ultimate devices (Ultimate 64, 1541 Ultimate II+) over your local network. Browse files, play SID music, launch programs, transfer files, and stream live video/audio.")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/svetzal/r64u")
set(CPACK_PACKAGE_CONTACT "r64u@example.com")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

# Package file naming
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

# -----------------------------------------------------------------------------
# macOS DMG Configuration
# -----------------------------------------------------------------------------
if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "${CPACK_PACKAGE_NAME}")
    set(CPACK_DMG_FORMAT "UDZO")  # Compressed disk image

    # DMG background and layout (optional - requires DS_Store file)
    # set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/packaging/dmg_background.png")
    # set(CPACK_DMG_DS_STORE "${CMAKE_CURRENT_SOURCE_DIR}/packaging/DS_Store")

    # Bundle identifier for code signing
    set(CPACK_BUNDLE_NAME "${CPACK_PACKAGE_NAME}")
    set(CPACK_BUNDLE_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/resources/Info.plist.in")

    # Qt deployment - copy frameworks into bundle
    # Note: Run macdeployqt before cpack, or use qt_generate_deploy_app_script()
    if(Qt6_VERSION VERSION_GREATER_EQUAL "6.3")
        qt_generate_deploy_app_script(
            TARGET r64u
            OUTPUT_SCRIPT deploy_script
            NO_UNSUPPORTED_PLATFORM_ERROR
        )
        install(SCRIPT ${deploy_script})
    endif()
endif()

# -----------------------------------------------------------------------------
# Windows NSIS and WiX Configuration
# -----------------------------------------------------------------------------
if(WIN32)
    set(CPACK_GENERATOR "NSIS;WIX")

    # Windows executable icon
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/app.ico")
        set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/app.ico")
        set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/app.ico")
    endif()

    # NSIS specific settings
    set(CPACK_NSIS_DISPLAY_NAME "${CPACK_PACKAGE_NAME}")
    set(CPACK_NSIS_PACKAGE_NAME "${CPACK_PACKAGE_NAME}")
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH OFF)
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY "bin")

    # Start menu and desktop shortcuts
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\r64u.lnk' '$INSTDIR\\\\bin\\\\r64u.exe'"
    )
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\r64u.lnk'"
    )

    # URL handlers/file associations (optional)
    set(CPACK_NSIS_URL_INFO_ABOUT "${CPACK_PACKAGE_HOMEPAGE_URL}")
    set(CPACK_NSIS_HELP_LINK "${CPACK_PACKAGE_HOMEPAGE_URL}")

    # WiX specific settings
    set(CPACK_WIX_UPGRADE_GUID "A1B2C3D4-E5F6-7890-ABCD-EF1234567890")  # Generate unique GUID for your project
    set(CPACK_WIX_PRODUCT_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/app.ico")
    set(CPACK_WIX_UI_BANNER "${CMAKE_CURRENT_SOURCE_DIR}/packaging/wix_banner.png")
    set(CPACK_WIX_UI_DIALOG "${CMAKE_CURRENT_SOURCE_DIR}/packaging/wix_dialog.png")

    # Qt deployment for Windows
    if(Qt6_VERSION VERSION_GREATER_EQUAL "6.3")
        qt_generate_deploy_app_script(
            TARGET r64u
            OUTPUT_SCRIPT deploy_script
            NO_UNSUPPORTED_PLATFORM_ERROR
        )
        install(SCRIPT ${deploy_script})
    endif()
endif()

# -----------------------------------------------------------------------------
# Linux DEB, RPM, and AppImage Configuration
# -----------------------------------------------------------------------------
if(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB;RPM")

    # Install the desktop file and icon
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/packaging/r64u.desktop"
        DESTINATION share/applications
    )
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/app.png"
        DESTINATION share/icons/hicolor/512x512/apps
        RENAME r64u.png
        OPTIONAL
    )

    # DEB specific settings
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${CPACK_PACKAGE_HOMEPAGE_URL}")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

    # DEB dependencies - Qt6 packages on Ubuntu/Debian
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "libqt6core6 (>= 6.2), libqt6gui6 (>= 6.2), libqt6widgets6 (>= 6.2), libqt6network6 (>= 6.2), libqt6multimedia6 (>= 6.2)"
    )

    # RPM specific settings
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
    set(CPACK_RPM_PACKAGE_GROUP "Applications/System")
    set(CPACK_RPM_PACKAGE_URL "${CPACK_PACKAGE_HOMEPAGE_URL}")
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)

    # RPM dependencies - Qt6 packages on Fedora/RHEL
    set(CPACK_RPM_PACKAGE_REQUIRES
        "qt6-qtbase >= 6.2, qt6-qtmultimedia >= 6.2"
    )

    # AppImage configuration (requires linuxdeployqt or linuxdeploy)
    # Note: AppImage is typically built outside of CPack using linuxdeploy
    # See packaging/build-appimage.sh for the build script
endif()

# Include CPack after all configuration
include(CPack)

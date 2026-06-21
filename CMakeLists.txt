cmake_minimum_required(VERSION 3.21)
project(snipQ VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Only the modules snipQ actually uses — no WebEngine
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Sql)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/appsettings.h
    src/sidebar.cpp
    src/snippeteditor.cpp
    src/highlighter.cpp
    src/codeformatter.cpp
    src/codeeditor.cpp
    src/snippetlist.cpp
    src/database.cpp
    src/models/snippet.cpp
    src/models/folder.cpp
    src/dialogs/newfolderdialog.cpp
    src/dialogs/newsnippetdialog.cpp
    src/dialogs/settingsdialog.cpp
    src/importexport.cpp
    resources/resources.qrc
)

qt_add_executable(snipQ ${SOURCES})

target_include_directories(snipQ PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/models
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dialogs
)

target_link_libraries(snipQ PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Sql
)

# Note: the Arch/Manjaro Qt6 packages have broken qcolor.h / qlayoutitem.h that
# reference USHRT_MAX / INT_MAX without including <climits>. The fix is applied
# directly in the project headers (codeeditor.h, sidebar.h, etc.) and limits.h.

if(WIN32)
    set_target_properties(snipQ PROPERTIES WIN32_EXECUTABLE TRUE)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc")
        target_sources(snipQ PRIVATE resources/app.rc)
    endif()
elseif(APPLE)
    set_target_properties(snipQ PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.snipq.app"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    )
endif()

install(TARGETS snipQ
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ── Desktop integration (Linux) ───────────────────────────────────────────────
if(UNIX AND NOT APPLE)
    install(FILES resources/app.png
        DESTINATION share/icons/hicolor/256x256/apps
        RENAME snipq.png)
    install(FILES resources/app.svg
        DESTINATION share/icons/hicolor/scalable/apps
        RENAME snipq.svg)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/packaging/snipq.desktop.in
        ${CMAKE_CURRENT_BINARY_DIR}/snipq.desktop
        @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/snipq.desktop
        DESTINATION share/applications)
endif()

# ── CPack (DEB + RPM) ─────────────────────────────────────────────────────────
set(CPACK_PACKAGE_NAME              "snipQ")
set(CPACK_PACKAGE_VENDOR            "snipQ contributors")
set(CPACK_PACKAGE_DESCRIPTION_SHORT "Cross-platform code snippet manager")
set(CPACK_PACKAGE_HOMEPAGE_URL      "https://github.com/appsQ/snipQ")
set(CPACK_PACKAGE_VERSION_MAJOR     ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR     ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH     ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_CONTACT           "snipQ contributors <noreply@github.com>")

# DEB-specific
set(CPACK_DEBIAN_PACKAGE_SECTION      "devel")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libqt6core6 (>= 6.2), libqt6gui6 (>= 6.2), libqt6widgets6 (>= 6.2), libqt6sql6 (>= 6.2), libqt6sql6-sqlite (>= 6.2)")
set(CPACK_DEBIAN_FILE_NAME            DEB-DEFAULT)

# RPM-specific
set(CPACK_RPM_PACKAGE_GROUP         "Development/Tools")
set(CPACK_RPM_PACKAGE_LICENSE       "MIT")
set(CPACK_RPM_PACKAGE_ARCHITECTURE  "x86_64")
set(CPACK_RPM_PACKAGE_REQUIRES
    "qt6-qtbase >= 6.2, qt6-qtbase-sqlite >= 6.2")
set(CPACK_RPM_FILE_NAME             RPM-DEFAULT)

include(CPack)

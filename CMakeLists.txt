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

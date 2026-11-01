cmake_minimum_required(VERSION 3.18.0 FATAL_ERROR)
# CMP0000: Call the cmake_minimum_required() command at the beginning of the top-level
# CMakeLists.txt file even before calling the project() command.
# The cmake_minimum_required(VERSION) command implicitly invokes the cmake_policy(VERSION)
# command to specify that the current project code is written for the given range of CMake
# versions.
project(maze-terminal)

include(GNUInstallDirs)

# qterminal version
set(QTERMINAL_VERSION "2.4.0")
# Maze Terminal's own version; QTERMINAL_VERSION stays the upstream base.
set(MAZE_TERMINAL_VERSION "1.1.0")

option(UPDATE_TRANSLATIONS "Update source translation translations/*.ts files" OFF)
option(BUILD_TESTS "Builds tests" ON)

# we need qpa/qplatformnativeinterface.h for global shortcut

# Minimum Versions
set(QTERMWIDGET_MINIMUM_VERSION "2.4.0")
set(QT_MINIMUM_VERSION "6.6.0")
set(QT_MAJOR_VERSION "6")
set(SHELLQT_MINIMUM_VERSION "6.0.0")

find_package(Qt6Core ${QT_MINIMUM_VERSION} REQUIRED)
find_package(Qt6Gui ${QT_MINIMUM_VERSION} REQUIRED)
find_package(Qt6LinguistTools ${QT_MINIMUM_VERSION} REQUIRED)
find_package(Qt6Widgets ${QT_MINIMUM_VERSION} REQUIRED)
find_package(LayerShellQt ${SHELLQT_MINIMUM_VERSION} REQUIRED)
if(UNIX)
    find_package(Qt6DBus ${QT_MINIMUM_VERSION} REQUIRED)
    find_package(Qt6 COMPONENTS Core REQUIRED)
endif()
# QTermWidget is bundled (3rdparty/qtermwidget), patched for Maze Terminal.
# The three LXQt build modules used here are bundled (cmake/lxqt, BSD-3-Clause),
# so building needs no lxqt-build-tools package.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/lxqt")

if (BUILD_TESTS)
    find_package(Qt6 ${QT_MINIMUM_VERSION} CONFIG REQUIRED Test)
endif()

include(LXQtPreventInSourceBuilds)
include(FindPkgConfig)
pkg_check_modules(LIBCANBERRA libcanberra)
include(LXQtTranslateTs)
include(LXQtCompilerSettings NO_POLICY_SCOPE)

# KWindowSystem (KDE Frameworks): the glass frame blurs what is behind the
# window. Optional — without it the frame is simply translucent.
find_package(KF6WindowSystem QUIET)
if(KF6WindowSystem_FOUND)
    add_definitions(-DHAVE_KWINDOWSYSTEM)
    message(STATUS "Building with KWindowSystem (window blur)")
endif()

# The terminal widget, bundled and built in as a static library.
add_subdirectory(3rdparty/qtermwidget)
message(STATUS "Qt version: ${Qt6Core_VERSION}")

# TODO remove Qxt
message(STATUS "Using bundled Qxt...")
set(QXT_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src/third-party")

# Maze Terminal is Linux-only (QTerminal's macOS and Windows parts are gone).
find_package(X11 REQUIRED)
message(STATUS "X11_X11_LIB: ${X11_X11_LIB}")

add_definitions(
    -DQTERMINAL_VERSION=\"${QTERMINAL_VERSION}\"
    -DMAZE_TERMINAL_VERSION=\"${MAZE_TERMINAL_VERSION}\"
)

set(EXE_NAME maze-terminal)

set(QTERM_SRC
    src/main.cpp
    src/maze/pasteguard.cpp
    src/maze/pastedialog.cpp
    src/maze/chrome.cpp
    src/mainwindow.cpp
    src/tabbar.cpp
    src/tabwidget.cpp
    src/termwidget.cpp
    src/termwidgetholder.cpp
    src/terminalconfig.cpp
    src/properties.cpp
    src/propertiesdialog.cpp
    src/bookmarkswidget.cpp
    src/fontdialog.cpp
    src/dbusaddressable.cpp
    src/tab-switcher.cpp
    src/qterminalutils.cpp
)

set(QTERM_MOC_SRC
    src/qterminalapp.h
    src/maze/pastedialog.h
    src/maze/chrome.h
    src/mainwindow.h
    src/tabbar.h
    src/tabwidget.h
    src/termwidget.h
    src/termwidgetholder.h
    src/propertiesdialog.h
    src/bookmarkswidget.h
    src/fontdialog.h
    src/tab-switcher.h
)

if (Qt6DBus_FOUND)
    add_definitions(-DHAVE_QDBUS)
    QT6_ADD_DBUS_ADAPTOR(QTERM_SRC src/org.lxqt.QTerminal.Window.xml mainwindow.h MainWindow)
    QT6_ADD_DBUS_ADAPTOR(QTERM_SRC src/org.lxqt.QTerminal.Tab.xml termwidgetholder.h TermWidgetHolder)
    QT6_ADD_DBUS_ADAPTOR(QTERM_SRC src/org.lxqt.QTerminal.Terminal.xml termwidget.h TermWidget)
    QT6_ADD_DBUS_ADAPTOR(QTERM_SRC src/org.lxqt.QTerminal.Process.xml qterminalapp.h QTerminalApp)

    set(QTERM_MOC_SRC ${QTERM_MOC_SRC} src/dbusaddressable.h)
    message(STATUS "Building with Qt6DBus support")
endif()

if(NOT QXT_FOUND)
    set(QTERM_SRC ${QTERM_SRC} src/third-party/qxtglobalshortcut.cpp)
    set(QTERM_MOC_SRC ${QTERM_MOC_SRC} src/third-party/qxtglobalshortcut.h)

    set(QTERM_SRC ${QTERM_SRC} src/third-party/qxtglobalshortcut_x11.cpp)
endif()

set(QTERM_UI_SRC
    src/forms/qterminal.ui
    src/forms/propertiesdialog.ui
    src/forms/bookmarkswidget.ui
    src/forms/fontdialog.ui
)

set(QTERM_RCC_SRC
    src/icons.qrc
    maze/chrome/chrome.qrc
)

qt6_wrap_ui( QTERM_UI ${QTERM_UI_SRC} )
qt6_wrap_cpp( QTERM_MOC ${QTERM_MOC_SRC} )
qt6_add_resources( QTERM_RCC ${QTERM_RCC_SRC} )

lxqt_translate_ts(QTERM_QM
    UPDATE_TRANSLATIONS
        ${UPDATE_TRANSLATIONS}
    SOURCES
        ${QTERM_SRC}
        ${QTERM_UI_SRC}
        ${QTERM_MOC_SRC}
    TRANSLATION_DIR "src/translations"
    # Still QTerminal's file names: the project is called maze-terminal now,
    # and the default template would look for maze-terminal_*.ts.
    TEMPLATE "qterminal"
)

# Maze Terminal's desktop entries carry their own English and Turkish names
# (maze/), instead of QTerminal's translated ones.
set(DESKTOP_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/maze/maze-terminal.desktop"
    "${CMAKE_CURRENT_SOURCE_DIR}/maze/maze-terminal-drop.desktop")

include_directories(
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}"
    ${QXT_INCLUDE_DIRS}
)
if(X11_FOUND)
    include_directories("${X11_INCLUDE_DIR}")
endif()


set(TRANSLATIONS_DIR "${CMAKE_INSTALL_FULL_DATADIR}/maze-terminal/translations")
add_definitions(-DTRANSLATIONS_DIR=\"${TRANSLATIONS_DIR}\")

add_executable(${EXE_NAME}
    ${QTERM_SRC}
    ${QTERM_UI}
    ${QTERM_MOC}
    ${QTERM_RCC}
    ${QTERM_QM}
    ${DESKTOP_FILES}
)

target_link_libraries(${EXE_NAME}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    mazetermwidget
    $<$<BOOL:${KF6WindowSystem_FOUND}>:KF6::WindowSystem>
    LayerShellQtInterface
)
if(QXT_FOUND)
    target_link_libraries(${EXE_NAME} ${QXT_CORE_LIB} ${QXT_GUI_LIB})
endif()

if (Qt6DBus_FOUND)
    target_link_libraries(${EXE_NAME} ${Qt6DBus_LIBRARIES})
endif()

if(X11_FOUND)
    target_link_libraries(${EXE_NAME} ${X11_X11_LIB})
endif()

if(LIBCANBERRA_FOUND)
    add_definitions(-DHAVE_LIBCANBERRA)
    include_directories(${LIBCANBERRA_INCLUDE_DIRS})
    target_link_libraries(${EXE_NAME} ${LIBCANBERRA_LIBRARIES})
endif()

set(APP_DIR "${CMAKE_INSTALL_FULL_DATADIR}/maze-terminal")
add_definitions(-DAPP_DIR=\"${APP_DIR}\")

install(FILES
    ${DESKTOP_FILES}
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
)

install(FILES
    maze/maze-terminal.metainfo.xml
    DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo"
)

# The Maze colour scheme, found through the app's data dir (setupCustomDirs),
# and the Maze icon.
install(FILES maze/Maze.colorscheme DESTINATION "${APP_DIR}/color-schemes")
install(FILES maze/maze-terminal.svg DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps")

install(FILES
    qterminal_bookmarks_example.xml
    DESTINATION ${APP_DIR}
)

install(TARGETS ${EXE_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES ${QTERM_QM} DESTINATION ${TRANSLATIONS_DIR})

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

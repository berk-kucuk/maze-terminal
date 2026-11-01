# QTermWidget 2.4.0 (LXQt, LGPL-2.0-or-later), built into Maze Terminal as a
# static library instead of using the system qtermwidget package.
#
# Why bundled: the terminal's text drawing lives here, and Maze Terminal
# patches it — block elements (█ ▀ ▄ ▌ ▐ ░ ▒ ▓ …) are drawn as filled cells,
# as Konsole does, instead of font glyphs that leave hairline gaps between
# cells when the font's advance is not a whole pixel (fastfetch logos).
# Changes against upstream are marked "Maze:".
#
# Its keyboard layouts and colour schemes go under Maze Terminal's own data
# dir, so this never conflicts with an installed qtermwidget package.

set(QTERMWIDGET_VERSION_MAJOR "2")
set(QTERMWIDGET_VERSION_MINOR "4")
set(QTERMWIDGET_VERSION_PATCH "0")

include(GenerateExportHeader)
include(CheckFunctionExists)

set(QTW_DATA_DIR "${CMAKE_INSTALL_FULL_DATADIR}/maze-terminal/qtermwidget")
set(QTW_KB_LAYOUT_DIR "${QTW_DATA_DIR}/kb-layouts")
set(QTW_COLORSCHEMES_DIR "${QTW_DATA_DIR}/color-schemes")
set(QTW_TRANSLATIONS_DIR "${QTW_DATA_DIR}/translations")

set(QTW_SRCS
    lib/BlockArray.cpp
    lib/ColorScheme.cpp
    lib/Emulation.cpp
    lib/Filter.cpp
    lib/History.cpp
    lib/HistorySearch.cpp
    lib/KeyboardTranslator.cpp
    lib/konsole_wcwidth.cpp
    lib/kprocess.cpp
    lib/kpty.cpp
    lib/kptydevice.cpp
    lib/kptyprocess.cpp
    lib/Pty.cpp
    lib/qtermwidget.cpp
    lib/Screen.cpp
    lib/ScreenWindow.cpp
    lib/SearchBar.cpp
    lib/Session.cpp
    lib/ShellCommand.cpp
    lib/TerminalCharacterDecoder.cpp
    lib/TerminalDisplay.cpp
    lib/tools.cpp
    lib/Vt102Emulation.cpp
)
set(QTW_MOC_HDRS
    lib/Emulation.h
    lib/Filter.h
    lib/HistorySearch.h
    lib/kprocess.h
    lib/kptydevice.h
    lib/kptyprocess.h
    lib/Pty.h
    lib/qtermwidget.h
    lib/ScreenWindow.h
    lib/SearchBar.h
    lib/Session.h
    lib/TerminalDisplay.h
    lib/Vt102Emulation.h
)

check_function_exists(updwtmpx HAVE_UPDWTMPX)

qt6_wrap_cpp(QTW_MOCS ${QTW_MOC_HDRS})
qt6_wrap_ui(QTW_UI_SRCS lib/SearchBar.ui)

lxqt_translate_ts(QTW_QM
    TEMPLATE "qtermwidget"
    TRANSLATION_DIR "lib/translations"
    SOURCES ${QTW_SRCS} ${QTW_MOC_HDRS} lib/SearchBar.ui
    INSTALL_DIR ${QTW_TRANSLATIONS_DIR}
)

add_library(mazetermwidget STATIC ${QTW_SRCS} ${QTW_MOCS} ${QTW_UI_SRCS} ${QTW_QM})
set_target_properties(mazetermwidget PROPERTIES AUTOMOC OFF POSITION_INDEPENDENT_CODE ON)
target_link_libraries(mazetermwidget PUBLIC Qt6::Widgets)
target_compile_definitions(mazetermwidget
    PUBLIC
        QTERMWIDGET_STATIC_DEFINE
    PRIVATE
        "KB_LAYOUT_DIR=\"${QTW_KB_LAYOUT_DIR}\""
        "COLORSCHEMES_DIR=\"${QTW_COLORSCHEMES_DIR}\""
        "TRANSLATIONS_DIR=\"${QTW_TRANSLATIONS_DIR}\""
        HAVE_POSIX_OPENPT
        HAVE_SYS_TIME_H
)
if(HAVE_UPDWTMPX)
    target_compile_definitions(mazetermwidget PRIVATE HAVE_UPDWTMPX)
endif()

generate_export_header(mazetermwidget
    EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/lib/qtermwidget_export.h"
    BASE_NAME QTERMWIDGET
)
configure_file(lib/qtermwidget_version.h.in "${CMAKE_CURRENT_BINARY_DIR}/lib/qtermwidget_version.h" @ONLY)

target_include_directories(mazetermwidget
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/lib"
        "${CMAKE_CURRENT_BINARY_DIR}/lib"
    PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}"
)

install(DIRECTORY lib/kb-layouts/ DESTINATION "${QTW_KB_LAYOUT_DIR}" FILES_MATCHING PATTERN "*.keytab")
install(DIRECTORY lib/color-schemes/ DESTINATION "${QTW_COLORSCHEMES_DIR}" FILES_MATCHING PATTERN "*.*scheme")

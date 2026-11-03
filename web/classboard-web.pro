# Web version of ClassBoard (Qt for WebAssembly, Qt 5.15).
#
# The web version is built from exactly the same sources as the desktop app (src/), with qmake,
# which is the build system Qt 5.15 supports for WebAssembly:
#
#   source /path/to/emsdk/emsdk_env.sh            # Emscripten 1.39.8 for Qt 5.15.2
#   /path/to/Qt/5.15.2/wasm_32/bin/qmake web/classboard-web.pro
#   make -j4
#   ./web/package.sh <build dir> <output dir>      # page, loader, pdf.js
#
# See docs/WEB.md. The GitHub workflow .github/workflows/web.yml does all of this and publishes
# the result on GitHub Pages.

TEMPLATE = app
TARGET = classboard
QT += core gui widgets svg
CONFIG += c++17 release warn_off
CONFIG -= debug_and_release

SRC = $$clean_path($$PWD/../src)
INCLUDEPATH += $$SRC

# Every source file of the application (the same files CMake builds into ClassBoard).
SOURCES += $$files($$SRC/*.cpp, true)
HEADERS += $$files($$SRC/*.h, true)
RESOURCES += $$clean_path($$PWD/../resources/classboard.qrc)

# Version: project(ClassBoard VERSION x.y.z) in CMakeLists.txt is the single source.
isEmpty(CLASSBOARD_VERSION) {
    CMAKE_LINES = $$cat($$PWD/../CMakeLists.txt, lines)
    VERSION_LINE = $$find(CMAKE_LINES, "^\\s+VERSION\\s+[0-9]")
    CLASSBOARD_VERSION = $$replace(VERSION_LINE, "^\\s*VERSION\\s+", "")
    CLASSBOARD_VERSION = $$first(CLASSBOARD_VERSION)
}
isEmpty(CLASSBOARD_VERSION): CLASSBOARD_VERSION = 0.0.0
message("ClassBoard web version $$CLASSBOARD_VERSION")

DEFINES += \
    CLASSBOARD_VERSION=\\\"$$CLASSBOARD_VERSION\\\" \
    QT_NO_CAST_FROM_BYTEARRAY \
    QT_NO_URL_CAST_FROM_STRING \
    QT_DISABLE_DEPRECATED_BEFORE=0x050F00

wasm {
    # IndexedDB file system: settings, templates, saved lessons and autosave copies are kept in
    # the browser (mounted by web/classboard-web.js before the application starts).
    QMAKE_LFLAGS += -lidbfs.js
}

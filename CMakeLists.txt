cmake_minimum_required(VERSION 3.16)

# The single source of the version: the app's About text, ClassBoard.exe's file properties and
# the installer name (ClassBoard-Setup-<version>-x64.exe) all come from here. A release is
# tagged v<version>; the release workflow refuses a tag that does not match.
project(ClassBoard
    VERSION 0.3.1
    DESCRIPTION "Digital classroom whiteboard for interactive displays"
    HOMEPAGE_URL "https://github.com/An0nym0us-glitch/Smart-board22"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(CLASSBOARD_BUILD_TESTS "Build unit tests" ON)
option(CLASSBOARD_DEPLOY_QT "Run windeployqt after building (Windows)" OFF)

find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Widgets Svg)
find_package(Qt5 5.15 QUIET OPTIONAL_COMPONENTS Pdf)

add_subdirectory(src)

if(CLASSBOARD_BUILD_TESTS)
    find_package(Qt5 5.15 REQUIRED COMPONENTS Test)
    enable_testing()
    add_subdirectory(tests)
endif()

include(cmake/Packaging.cmake)

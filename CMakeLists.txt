cmake_minimum_required(VERSION 3.22)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Toolchain.cmake)

project(time_tracker
    VERSION 0.1.0
    DESCRIPTION "Desktop time tracker for categories, topics, and subtopics"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

option(TIME_TRACKER_BUILD_TESTS "Build unit tests" ON)
option(TIME_TRACKER_BUILD_APP "Build the Qt Widgets desktop app" ON)
set(TIME_TRACKER_APP_VERSION "${PROJECT_VERSION} (localbuild)" CACHE STRING
    "Version string embedded in the build")

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilerWarnings.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Dependencies.cmake)

add_subdirectory(core)

if(TIME_TRACKER_BUILD_APP)
    add_subdirectory(ui)
    add_subdirectory(app)
endif()

if(TIME_TRACKER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "")
message(STATUS "============================================================")
message(STATUS "  ${PROJECT_NAME} ${PROJECT_VERSION}")
message(STATUS "  Compiler:        ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Build type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "  Build tests:     ${TIME_TRACKER_BUILD_TESTS}")
message(STATUS "  Build app:       ${TIME_TRACKER_BUILD_APP}")
message(STATUS "  App version:     ${TIME_TRACKER_APP_VERSION}")
message(STATUS "============================================================")
message(STATUS "")

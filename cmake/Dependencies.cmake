include(FetchContent)

set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)

if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

if(TIME_TRACKER_BUILD_TESTS)
    message(STATUS "[time_tracker] Fetching doctest ...")
    FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG        v2.4.11
        GIT_SHALLOW    TRUE
    )
    set(DOCTEST_WITH_TESTS OFF CACHE INTERNAL "")
    set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE INTERNAL "")
    FetchContent_MakeAvailable(doctest)
endif()

message(STATUS "[time_tracker] Fetching SQLite amalgamation ...")
FetchContent_Declare(
    sqlite_amalgamation
    URL                    https://www.sqlite.org/2026/sqlite-amalgamation-3530400.zip
    URL_HASH               SHA3_256=628a44cfe82c66aed1ccbbe85a562d2e33ebe64b3288981ed76285612227934e
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_GetProperties(sqlite_amalgamation)
if(NOT sqlite_amalgamation_POPULATED)
    FetchContent_Populate(sqlite_amalgamation)
endif()

add_library(sqlite3 STATIC
    ${sqlite_amalgamation_SOURCE_DIR}/sqlite3.c
)
target_include_directories(sqlite3
    PUBLIC $<BUILD_INTERFACE:${sqlite_amalgamation_SOURCE_DIR}>
)
target_compile_definitions(sqlite3 PUBLIC
    SQLITE_THREADSAFE=1
    SQLITE_OMIT_LOAD_EXTENSION=1
    SQLITE_DQS=0
    SQLITE_DEFAULT_FOREIGN_KEYS=1
)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(sqlite3 PRIVATE -w)
endif()
if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(sqlite3 PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
endif()

if(TIME_TRACKER_BUILD_APP)
    if(CMAKE_CXX_COMPILER)
        get_filename_component(_TT_COMPILER_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
        get_filename_component(_TT_COMPILER_PREFIX "${_TT_COMPILER_BIN}" DIRECTORY)
        list(PREPEND CMAKE_PREFIX_PATH "${_TT_COMPILER_PREFIX}")
    endif()
    find_package(Qt6 6.5 QUIET COMPONENTS Widgets)
    if(NOT Qt6_FOUND)
        message(FATAL_ERROR
            "Qt 6.5+ Widgets was not found. Install the MSYS2 UCRT64 package "
            "mingw-w64-ucrt-x86_64-qt6-base and configure with the mingw preset, "
            "or pass -DTIME_TRACKER_BUILD_APP=OFF to build the library and tests only. "
            "MSVC Qt kits are not supported.")
    endif()
    message(STATUS "[time_tracker] Qt ${Qt6_VERSION} Widgets")
endif()

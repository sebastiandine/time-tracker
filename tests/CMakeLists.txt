add_executable(time_tracker_core_tests
    main.cpp
    time_format_tests.cpp
    stats_range_tests.cpp
    data_paths_tests.cpp
    data_location_tests.cpp
    database_tests.cpp
    catalog_tests.cpp
    config_tests.cpp
    json_tests.cpp
    text_file_tests.cpp
    tracking_tests.cpp
    stats_tests.cpp
    settings_tests.cpp
)

target_include_directories(time_tracker_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(time_tracker_core_tests
    PRIVATE
        time_tracker_core
        sqlite3
        doctest::doctest
        time_tracker_warnings
)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(time_tracker_core_tests)

set(CMAKE_AUTOMOC ON)

add_library(time_tracker_ui STATIC
    include/timetracker/ui/NameNotesDialog.hpp
    include/timetracker/ui/LibraryPage.hpp
    include/timetracker/ui/TrackPage.hpp
    include/timetracker/ui/StatisticsPage.hpp
    include/timetracker/ui/MainWindow.hpp
    src/Theme.cpp
    src/CatalogTree.cpp
    src/NameNotesDialog.cpp
    src/LibraryPage.cpp
    src/TrackPage.cpp
    src/ShareChart.cpp
    src/StatisticsPage.cpp
    src/MainWindow.cpp
)

target_include_directories(time_tracker_ui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(time_tracker_ui
    PUBLIC
        time_tracker_core
        Qt6::Widgets
)

target_compile_features(time_tracker_ui PUBLIC cxx_std_20)

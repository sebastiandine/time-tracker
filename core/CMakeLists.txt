add_library(time_tracker_core STATIC
    src/config/ConfigFile.cpp
    src/config/ConfigService.cpp
    src/infra/HostTextFile.cpp
    src/infra/SqlUtil.cpp
    src/infra/SqliteStmt.cpp
    src/infra/SqliteDatabase.cpp
    src/infra/SqliteCatalogRepository.cpp
    src/infra/SqliteTimeEntryRepository.cpp
    src/infra/SqliteSettingsRepository.cpp
    src/json/JsonObjectBag.cpp
    src/paths/DataPaths.cpp
    src/services/CatalogService.cpp
    src/services/DataLocationService.cpp
    src/services/TrackingService.cpp
    src/services/StatsService.cpp
    src/services/SettingsService.cpp
    src/text/TextRules.cpp
    src/time/TimeFormat.cpp
    src/time/StatsRange.cpp
)

target_include_directories(time_tracker_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(time_tracker_core
    PUBLIC
        nlohmann_json::nlohmann_json
    PRIVATE
        sqlite3
        time_tracker_warnings
)

target_compile_features(time_tracker_core PUBLIC cxx_std_20)

#pragma once

#include "support/FakeClock.hpp"

#include "timetracker/infra/SqliteCatalogRepository.hpp"
#include "timetracker/infra/SqliteDatabase.hpp"
#include "timetracker/infra/SqliteSettingsRepository.hpp"
#include "timetracker/infra/SqliteTimeEntryRepository.hpp"
#include "timetracker/services/CatalogService.hpp"
#include "timetracker/services/SettingsService.hpp"
#include "timetracker/services/StatsService.hpp"
#include "timetracker/services/TrackingService.hpp"
#include "timetracker/time/TimeFormat.hpp"

#include <doctest/doctest.h>

#include <string_view>

namespace timetracker::test {

/**
 * @brief Parse a UTC timestamp or fail the current test.
 * @param text Timestamp in @c YYYY-MM-DDTHH:MM:SSZ.
 * @return The parsed instant.
 */
inline std::chrono::system_clock::time_point requireUtc(std::string_view text) {
    auto parsed = parseUtc(text);
    REQUIRE(parsed.succeeded());
    return parsed.value();
}

/**
 * @brief One category, main topic, and subtopic created for a test.
 */
struct Seed {
    Category category;
    MainTopic mainTopic;
    SubTopic subTopic;
};

/**
 * @brief In-memory database plus the application services.
 *
 * Call @c open() before using the services. The clock starts at 10:00 UTC on
 * 2026-10-23 unless the constructor is given another timestamp.
 */
struct Fixture {
    FakeClock clock;
    SqliteDatabase db;
    SqliteCatalogRepository catalogRepo;
    SqliteTimeEntryRepository timeRepo;
    SqliteSettingsRepository settingsRepo;
    CatalogService catalog;
    TrackingService tracking;
    StatsService stats;
    SettingsService settings;

    explicit Fixture(std::string_view nowUtc = "2026-10-23T10:00:00Z")
        : clock(requireUtc(nowUtc)),
          catalogRepo(db),
          timeRepo(db),
          settingsRepo(db),
          catalog(catalogRepo),
          tracking(catalogRepo, timeRepo, clock),
          stats(timeRepo, clock),
          settings(settingsRepo) {}

    void open() {
        const auto opened = db.openMemory();
        REQUIRE(opened.succeeded());
    }

    /**
     * @brief Create Work / Design / API.
     * @return The three stored rows.
     */
    Seed seed() {
        auto category = catalog.createCategory("Work", "c");
        REQUIRE(category.succeeded());
        auto mainTopic = catalog.createMainTopic(category.value().id, "Design", "m");
        REQUIRE(mainTopic.succeeded());
        auto subTopic = catalog.createSubTopic(mainTopic.value().id, "API", "s");
        REQUIRE(subTopic.succeeded());
        return Seed{category.value(), mainTopic.value(), subTopic.value()};
    }
};

}  // namespace timetracker::test

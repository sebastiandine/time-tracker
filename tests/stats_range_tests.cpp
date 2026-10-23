#include "timetracker/time/StatsRange.hpp"

#include <doctest/doctest.h>

using namespace timetracker;

TEST_CASE("today at UTC midnight stays on that civil day") {
    auto range = boundsForPreset(StatsPreset::Today, CivilDate{2026, 10, 23}, 0, {}, {});
    REQUIRE(range.succeeded());
    CHECK(range.value().startUtc == "2026-10-23T00:00:00Z");
    CHECK(range.value().endUtc == "2026-10-24T00:00:00Z");
}

TEST_CASE("a positive offset moves local midnight to the previous UTC day") {
    auto range = boundsForPreset(StatsPreset::Today, CivilDate{2026, 10, 23}, 120, {}, {});
    REQUIRE(range.succeeded());
    CHECK(range.value().startUtc == "2026-10-22T22:00:00Z");
    CHECK(range.value().endUtc == "2026-10-23T22:00:00Z");
}

TEST_CASE("a negative offset moves local midnight forward") {
    auto range = boundsForPreset(StatsPreset::Today, CivilDate{2026, 10, 23}, -300, {}, {});
    REQUIRE(range.succeeded());
    CHECK(range.value().startUtc == "2026-10-23T05:00:00Z");
    CHECK(range.value().endUtc == "2026-10-24T05:00:00Z");
}

TEST_CASE("the week starts on Monday for a Friday and a Sunday") {
    auto friday = boundsForPreset(StatsPreset::ThisWeek, CivilDate{2026, 10, 23}, 0, {}, {});
    REQUIRE(friday.succeeded());
    CHECK(friday.value().startUtc == "2026-10-19T00:00:00Z");
    CHECK(friday.value().endUtc == "2026-10-26T00:00:00Z");

    auto sunday = boundsForPreset(StatsPreset::ThisWeek, CivilDate{2026, 10, 25}, 0, {}, {});
    REQUIRE(sunday.succeeded());
    CHECK(sunday.value().startUtc == "2026-10-19T00:00:00Z");

    auto monday = boundsForPreset(StatsPreset::ThisWeek, CivilDate{2026, 10, 19}, 0, {}, {});
    REQUIRE(monday.succeeded());
    CHECK(monday.value().startUtc == "2026-10-19T00:00:00Z");
}

TEST_CASE("a month range includes the first of the next month as the end") {
    auto october = boundsForPreset(StatsPreset::ThisMonth, CivilDate{2026, 10, 23}, 0, {}, {});
    REQUIRE(october.succeeded());
    CHECK(october.value().startUtc == "2026-10-01T00:00:00Z");
    CHECK(october.value().endUtc == "2026-11-01T00:00:00Z");

    auto december = boundsForPreset(StatsPreset::ThisMonth, CivilDate{2026, 12, 15}, 0, {}, {});
    REQUIRE(december.succeeded());
    CHECK(december.value().startUtc == "2026-12-01T00:00:00Z");
    CHECK(december.value().endUtc == "2027-01-01T00:00:00Z");
}

TEST_CASE("a custom range is inclusive of the last local day") {
    auto range = boundsForPreset(StatsPreset::Custom, {}, 0, CivilDate{2026, 10, 23},
                                 CivilDate{2026, 10, 23});
    REQUIRE(range.succeeded());
    CHECK(range.value().startUtc == "2026-10-23T00:00:00Z");
    CHECK(range.value().endUtc == "2026-10-24T00:00:00Z");
}

TEST_CASE("year 2000 is a leap year and year 2100 is not") {
    auto leap = boundsForPreset(StatsPreset::Custom, {}, 0, CivilDate{2000, 2, 29},
                                CivilDate{2000, 2, 29});
    REQUIRE(leap.succeeded());
    CHECK(leap.value().startUtc == "2000-02-29T00:00:00Z");

    auto century = boundsForPreset(StatsPreset::Custom, {}, 0, CivilDate{2100, 2, 29},
                                   CivilDate{2100, 2, 29});
    CHECK_FALSE(century.succeeded());
    CHECK(century.error() == "That date is not valid.");
}

TEST_CASE("a leap day is a valid custom range and the next year is not") {
    auto leap = boundsForPreset(StatsPreset::Custom, {}, 0, CivilDate{2024, 2, 29},
                                CivilDate{2024, 2, 29});
    REQUIRE(leap.succeeded());
    CHECK(leap.value().startUtc == "2024-02-29T00:00:00Z");
    CHECK(leap.value().endUtc == "2024-03-01T00:00:00Z");

    auto missing = boundsForPreset(StatsPreset::Custom, {}, 0, CivilDate{2026, 2, 29},
                                   CivilDate{2026, 2, 29});
    CHECK_FALSE(missing.succeeded());
    CHECK(missing.error() == "That date is not valid.");
}

TEST_CASE("a custom end before the start is rejected") {
    auto range = boundsForPreset(StatsPreset::Custom, {}, 0, CivilDate{2026, 10, 24},
                                 CivilDate{2026, 10, 23});
    CHECK_FALSE(range.succeeded());
    CHECK(range.error() == "End is before start.");
}

TEST_CASE("an offset outside fourteen hours is rejected") {
    auto high = boundsForPreset(StatsPreset::Today, CivilDate{2026, 10, 23}, 841, {}, {});
    CHECK_FALSE(high.succeeded());
    CHECK(high.error() == "The time zone offset is outside the supported range.");

    auto low = boundsForPreset(StatsPreset::Today, CivilDate{2026, 10, 23}, -841, {}, {});
    CHECK_FALSE(low.succeeded());
    CHECK(low.error() == "The time zone offset is outside the supported range.");
}

TEST_CASE("a month number outside 1 through 12 is rejected") {
    auto low = boundsForPreset(StatsPreset::Today, CivilDate{2026, 0, 1}, 0, {}, {});
    CHECK_FALSE(low.succeeded());
    CHECK(low.error() == "That date is not valid.");

    auto high = boundsForPreset(StatsPreset::Today, CivilDate{2026, 13, 1}, 0, {}, {});
    CHECK_FALSE(high.succeeded());
    CHECK(high.error() == "That date is not valid.");
}

TEST_CASE("all time ignores the local date") {
    auto range = boundsForPreset(StatsPreset::AllTime, CivilDate{2026, 2, 31}, 9999, {}, {});
    REQUIRE(range.succeeded());
    CHECK(range.value().startUtc == "1970-01-01T00:00:00Z");
    CHECK(range.value().endUtc == "2100-01-01T00:00:00Z");
}

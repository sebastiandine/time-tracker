#include "timetracker/time/TimeFormat.hpp"

#include <doctest/doctest.h>

using namespace timetracker;

TEST_CASE("a UTC timestamp round-trips") {
    auto parsed = parseUtc("2026-10-23T10:00:00Z");
    REQUIRE(parsed.succeeded());
    auto formatted = formatUtc(parsed.value());
    REQUIRE(formatted.succeeded());
    CHECK(formatted.value() == "2026-10-23T10:00:00Z");
}

TEST_CASE("a far UTC timestamp round-trips") {
    auto parsed = parseUtc("2100-01-01T00:00:00Z");
    REQUIRE(parsed.succeeded());
    auto formatted = formatUtc(parsed.value());
    REQUIRE(formatted.succeeded());
    CHECK(formatted.value() == "2100-01-01T00:00:00Z");
}

TEST_CASE("a timestamp that is not the UTC form is rejected") {
    CHECK_FALSE(parseUtc("2026-10-23 10:00:00Z").succeeded());
    CHECK_FALSE(parseUtc("2026-10-23T10:00:00").succeeded());
    CHECK_FALSE(parseUtc("").succeeded());
    CHECK(parseUtc("nope").error() == "The timestamp is not valid.");
}

TEST_CASE("an impossible calendar date is rejected") {
    CHECK_FALSE(parseUtc("2026-02-31T00:00:00Z").succeeded());
    CHECK_FALSE(parseUtc("2026-13-01T00:00:00Z").succeeded());
    CHECK_FALSE(parseUtc("2026-10-23T24:00:00Z").succeeded());
    CHECK_FALSE(parseUtc("2026-10-23T10:60:00Z").succeeded());
}

TEST_CASE("seconds between two timestamps") {
    auto hour = secondsBetweenUtc("2026-10-23T10:00:00Z", "2026-10-23T11:00:00Z");
    REQUIRE(hour.succeeded());
    CHECK(hour.value() == 3600);

    auto same = secondsBetweenUtc("2026-10-23T10:00:00Z", "2026-10-23T10:00:00Z");
    REQUIRE(same.succeeded());
    CHECK(same.value() == 0);
}

TEST_CASE("a clock earlier than the start is rejected") {
    auto backwards = secondsBetweenUtc("2026-10-23T10:00:00Z", "2026-10-23T09:00:00Z");
    CHECK_FALSE(backwards.succeeded());
    CHECK(backwards.error() == "The clock is earlier than the start time.");
}

TEST_CASE("duration text uses hours that can pass 24") {
    CHECK(formatDuration(0) == "0:00:00");
    CHECK(formatDuration(-5) == "0:00:00");
    CHECK(formatDuration(3661) == "1:01:01");
    CHECK(formatDuration(26 * 3600 + 1) == "26:00:01");
}

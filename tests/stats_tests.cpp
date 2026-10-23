#include "support/Fixture.hpp"

#include "timetracker/time/StatsRange.hpp"

#include <doctest/doctest.h>

#include <sqlite3.h>

using namespace timetracker;
using test::Fixture;

namespace {

UtcRange allTime() {
    auto range = boundsForPreset(StatsPreset::AllTime, {}, 0, {}, {});
    REQUIRE(range.succeeded());
    return range.value();
}

UtcRange window(const char* start, const char* end) {
    UtcRange range;
    range.startUtc = start;
    range.endUtc = end;
    return range;
}

void closedMain(Fixture& fixture, std::int64_t mainId, const char* start, const char* end) {
    fixture.clock.set(test::requireUtc(start));
    REQUIRE(fixture.tracking.start(TimeTarget{TimeTargetKind::MainTopic, mainId}, "").succeeded());
    fixture.clock.set(test::requireUtc(end));
    REQUIRE(fixture.tracking.stop("").succeeded());
}

void closedSub(Fixture& fixture, std::int64_t subId, const char* start, const char* end) {
    fixture.clock.set(test::requireUtc(start));
    REQUIRE(fixture.tracking.start(TimeTarget{TimeTargetKind::SubTopic, subId}, "").succeeded());
    fixture.clock.set(test::requireUtc(end));
    REQUIRE(fixture.tracking.stop("").succeeded());
}

const StatsLine* findLine(const StatsReport& report, StatsLineKind kind, const std::string& detail) {
    for (const auto& line : report.lines) {
        if (line.kind == kind && line.detail == detail) {
            return &line;
        }
    }
    return nullptr;
}

}  // namespace

TEST_CASE("stats roll up subtopic time into the main topic") {
    Fixture fixture;
    fixture.open();
    auto seeded = fixture.seed();
    auto review = fixture.catalog.createSubTopic(seeded.mainTopic.id, "Review", "");
    REQUIRE(review.succeeded());
    closedMain(fixture, seeded.mainTopic.id, "2026-10-23T08:00:00Z", "2026-10-23T09:00:00Z");
    closedSub(fixture, seeded.subTopic.id, "2026-10-23T09:00:00Z", "2026-10-23T09:30:00Z");
    fixture.clock.set(test::requireUtc("2026-10-23T09:30:00Z"));
    REQUIRE(fixture.tracking
                .start(TimeTarget{TimeTargetKind::SubTopic, review.value().id}, "")
                .succeeded());
    fixture.clock.set(test::requireUtc("2026-10-23T10:00:00Z"));

    auto report = fixture.stats.summarize(allTime(), std::nullopt);
    REQUIRE(report.succeeded());
    CHECK(report.value().totalSeconds == 7200);
    const auto* total = findLine(report.value(), StatsLineKind::MainTotal, std::string(kStatsTotalLabel));
    const auto* direct = findLine(report.value(), StatsLineKind::Direct, std::string(kStatsDirectLabel));
    const auto* api = findLine(report.value(), StatsLineKind::SubTopic, "API");
    const auto* reviewLine = findLine(report.value(), StatsLineKind::SubTopic, "Review");
    REQUIRE(total != nullptr);
    REQUIRE(direct != nullptr);
    REQUIRE(api != nullptr);
    REQUIRE(reviewLine != nullptr);
    CHECK(total->seconds == 7200);
    CHECK(total->percent == 100);
    CHECK(direct->seconds == 3600);
    CHECK(direct->percent == 50);
    CHECK(api->seconds == 1800);
    CHECK(api->percent == 25);
    CHECK(reviewLine->seconds == 1800);
    CHECK(reviewLine->percent == 25);
    REQUIRE(report.value().lines.size() >= 4);
    CHECK(report.value().lines[1].kind == StatsLineKind::Direct);
    CHECK(report.value().lines[2].detail == "API");
    CHECK(report.value().lines[3].detail == "Review");
}

TEST_CASE("an open entry is clipped to now and to the range") {
    Fixture fixture;
    fixture.open();
    auto seeded = fixture.seed();
    closedMain(fixture, seeded.mainTopic.id, "2026-10-23T08:00:00Z", "2026-10-23T09:00:00Z");
    closedSub(fixture, seeded.subTopic.id, "2026-10-23T09:00:00Z", "2026-10-23T09:30:00Z");
    fixture.clock.set(test::requireUtc("2026-10-23T09:30:00Z"));
    REQUIRE(fixture.tracking
                .start(TimeTarget{TimeTargetKind::MainTopic, seeded.mainTopic.id}, "")
                .succeeded());
    fixture.clock.set(test::requireUtc("2026-10-23T10:00:00Z"));

    auto clipped = fixture.stats.summarize(
        window("2026-10-23T09:15:00Z", "2026-10-23T09:45:00Z"), std::nullopt);
    REQUIRE(clipped.succeeded());
    CHECK(clipped.value().totalSeconds == 1800);
    const auto* api = findLine(clipped.value(), StatsLineKind::SubTopic, "API");
    const auto* directOpen =
        findLine(clipped.value(), StatsLineKind::Direct, std::string(kStatsDirectLabel));
    REQUIRE(api != nullptr);
    REQUIRE(directOpen != nullptr);
    CHECK(api->seconds == 900);
    CHECK(directOpen->seconds == 900);
    const auto* total = findLine(clipped.value(), StatsLineKind::MainTotal, std::string(kStatsTotalLabel));
    REQUIRE(total != nullptr);
    CHECK(total->seconds == 1800);

    auto untilNow = fixture.stats.summarize(
        window("2026-10-23T09:00:00Z", "2026-10-23T12:00:00Z"), std::nullopt);
    REQUIRE(untilNow.succeeded());
    const auto* openDirect =
        findLine(untilNow.value(), StatsLineKind::Direct, std::string(kStatsDirectLabel));
    REQUIRE(openDirect != nullptr);
    CHECK(openDirect->seconds == 1800);
}

TEST_CASE("a range with no entries is empty") {
    Fixture fixture;
    fixture.open();
    fixture.seed();
    auto report = fixture.stats.summarize(window("2030-01-01T00:00:00Z", "2030-01-02T00:00:00Z"),
                                          std::nullopt);
    REQUIRE(report.succeeded());
    CHECK(report.value().totalSeconds == 0);
    CHECK(report.value().lines.empty());
}

TEST_CASE("a category filter excludes other categories") {
    Fixture fixture;
    fixture.open();
    auto work = fixture.catalog.createCategory("Work", "");
    auto personal = fixture.catalog.createCategory("Personal", "");
    REQUIRE(work.succeeded());
    REQUIRE(personal.succeeded());
    auto design = fixture.catalog.createMainTopic(work.value().id, "Design", "");
    auto chores = fixture.catalog.createMainTopic(personal.value().id, "Chores", "");
    REQUIRE(design.succeeded());
    REQUIRE(chores.succeeded());
    closedMain(fixture, design.value().id, "2026-10-23T08:00:00Z", "2026-10-23T09:00:00Z");
    closedMain(fixture, chores.value().id, "2026-10-23T09:00:00Z", "2026-10-23T09:30:00Z");

    auto report = fixture.stats.summarize(allTime(), work.value().id);
    REQUIRE(report.succeeded());
    CHECK(report.value().totalSeconds == 3600);
    CHECK(report.value().lines.size() == 2);
    CHECK(report.value().lines[0].categoryName == "Work");
    CHECK(findLine(report.value(), StatsLineKind::MainTotal, std::string(kStatsTotalLabel))->mainTopicName ==
          "Design");
}

TEST_CASE("categories in a report sort without regard to ASCII case") {
    Fixture fixture;
    fixture.open();
    auto later = fixture.catalog.createCategory("C", "");
    auto earlier = fixture.catalog.createCategory("b", "");
    REQUIRE(later.succeeded());
    REQUIRE(earlier.succeeded());
    auto laterTopic = fixture.catalog.createMainTopic(later.value().id, "One", "");
    auto earlierTopic = fixture.catalog.createMainTopic(earlier.value().id, "Two", "");
    REQUIRE(laterTopic.succeeded());
    REQUIRE(earlierTopic.succeeded());
    closedMain(fixture, laterTopic.value().id, "2026-10-23T08:00:00Z", "2026-10-23T08:10:00Z");
    closedMain(fixture, earlierTopic.value().id, "2026-10-23T08:10:00Z", "2026-10-23T08:20:00Z");
    auto report = fixture.stats.summarize(allTime(), std::nullopt);
    REQUIRE(report.succeeded());
    REQUIRE(report.value().lines.size() >= 2);
    CHECK(report.value().lines[0].categoryName == "b");
}

TEST_CASE("a stored timestamp that cannot be read fails the report") {
    Fixture fixture;
    fixture.open();
    auto seeded = fixture.seed();
    char* message = nullptr;
    const std::string sql =
        "INSERT INTO TimeEntry (main_topic_id, sub_topic_id, started_at, ended_at, is_running, notes) "
        "VALUES (" +
        std::to_string(seeded.mainTopic.id) +
        ", NULL, '2026-13-01T00:00:00Z', '2026-10-23T11:00:00Z', 0, '');";
    REQUIRE(sqlite3_exec(fixture.db.handle(), sql.c_str(), nullptr, nullptr, &message) == SQLITE_OK);
    sqlite3_free(message);
    auto report = fixture.stats.summarize(allTime(), std::nullopt);
    CHECK_FALSE(report.succeeded());
    CHECK(report.error() == "The timestamp is not valid.");
}

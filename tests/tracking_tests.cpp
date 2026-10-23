#include "support/Fixture.hpp"

#include <doctest/doctest.h>

using namespace timetracker;
using test::Fixture;

TEST_CASE("start stores an open entry and a second start is rejected") {
    Fixture fixture;
    fixture.open();
    auto seeded = fixture.seed();
    auto started = fixture.tracking.start(
        TimeTarget{TimeTargetKind::MainTopic, seeded.mainTopic.id}, "  first  ");
    REQUIRE(started.succeeded());
    CHECK(started.value().running);
    CHECK(started.value().startedAt == "2026-10-23T10:00:00Z");
    CHECK(started.value().notes == "first");
    CHECK_FALSE(started.value().endedAt.has_value());

    auto second = fixture.tracking.start(
        TimeTarget{TimeTargetKind::SubTopic, seeded.subTopic.id}, "other");
    CHECK_FALSE(second.succeeded());
    CHECK(second.error() == "A timer is already running.");
    auto current = fixture.tracking.current();
    REQUIRE(current.succeeded());
    REQUIRE(current.value().has_value());
    CHECK(current.value()->id == started.value().id);
}

TEST_CASE("elapsed time includes a clock jump while the entry stays open") {
    Fixture fixture;
    fixture.open();
    auto seeded = fixture.seed();
    auto started = fixture.tracking.start(
        TimeTarget{TimeTargetKind::SubTopic, seeded.subTopic.id}, "keep");
    REQUIRE(started.succeeded());
    fixture.clock.set(test::requireUtc("2026-10-23T15:00:00Z"));
    auto elapsed = fixture.tracking.elapsedSeconds();
    REQUIRE(elapsed.succeeded());
    CHECK(elapsed.value() == 5 * 3600);
    auto current = fixture.tracking.current();
    REQUIRE(current.succeeded());
    REQUIRE(current.value().has_value());
    CHECK(current.value()->startedAt == "2026-10-23T10:00:00Z");
    CHECK(current.value()->running);
}

TEST_CASE("a running note update is still there before stop") {
    Fixture fixture;
    fixture.open();
    auto seeded = fixture.seed();
    REQUIRE(fixture.tracking
                .start(TimeTarget{TimeTargetKind::MainTopic, seeded.mainTopic.id}, "start")
                .succeeded());
    auto updated = fixture.tracking.updateRunningNote("after a crash");
    REQUIRE(updated.succeeded());
    CHECK(updated.value().notes == "after a crash");
    CHECK(updated.value().running);
}

TEST_CASE("stop saves the note and the end time") {
    Fixture fixture;
    fixture.open();
    auto seeded = fixture.seed();
    REQUIRE(fixture.tracking
                .start(TimeTarget{TimeTargetKind::MainTopic, seeded.mainTopic.id}, "start")
                .succeeded());
    fixture.clock.set(test::requireUtc("2026-10-23T10:30:00Z"));
    auto stopped = fixture.tracking.stop("  finished  ");
    REQUIRE(stopped.succeeded());
    CHECK_FALSE(stopped.value().running);
    CHECK(stopped.value().endedAt.has_value());
    CHECK(*stopped.value().endedAt == "2026-10-23T10:30:00Z");
    CHECK(stopped.value().notes == "finished");
    auto current = fixture.tracking.current();
    REQUIRE(current.succeeded());
    CHECK_FALSE(current.value().has_value());
}

TEST_CASE("stop is rejected when the clock is earlier than the start") {
    Fixture fixture;
    fixture.open();
    auto seeded = fixture.seed();
    REQUIRE(fixture.tracking
                .start(TimeTarget{TimeTargetKind::MainTopic, seeded.mainTopic.id}, "")
                .succeeded());
    fixture.clock.set(test::requireUtc("2026-10-23T09:00:00Z"));
    auto stopped = fixture.tracking.stop("nope");
    CHECK_FALSE(stopped.succeeded());
    CHECK(stopped.error() == "The clock is earlier than the start time.");
    auto elapsed = fixture.tracking.elapsedSeconds();
    REQUIRE(elapsed.succeeded());
    CHECK(elapsed.value() == 0);
    auto current = fixture.tracking.current();
    REQUIRE(current.succeeded());
    CHECK(current.value().has_value());
}

TEST_CASE("stop with nothing running is rejected") {
    Fixture fixture;
    fixture.open();
    auto stopped = fixture.tracking.stop("");
    CHECK_FALSE(stopped.succeeded());
    CHECK(stopped.error() == "Nothing is running.");
}

TEST_CASE("a zero-length entry can be stopped") {
    Fixture fixture;
    fixture.open();
    auto seeded = fixture.seed();
    REQUIRE(fixture.tracking
                .start(TimeTarget{TimeTargetKind::MainTopic, seeded.mainTopic.id}, "")
                .succeeded());
    CHECK(fixture.tracking.stop("").succeeded());
}

TEST_CASE("a running entry cannot be deleted or edited as a closed entry") {
    Fixture fixture;
    fixture.open();
    auto seeded = fixture.seed();
    auto started = fixture.tracking.start(
        TimeTarget{TimeTargetKind::MainTopic, seeded.mainTopic.id}, "open");
    REQUIRE(started.succeeded());
    auto deleted = fixture.tracking.deleteEntry(started.value().id);
    CHECK_FALSE(deleted.succeeded());
    CHECK(deleted.error() == "That entry is still running.");
    auto edited = fixture.tracking.updateClosedNote(started.value().id, "no");
    CHECK_FALSE(edited.succeeded());
    CHECK(edited.error() == "That entry is still running.");
    CHECK(fixture.tracking.current().value().has_value());
}

TEST_CASE("a closed entry note can be edited and the entry can be deleted") {
    Fixture fixture;
    fixture.open();
    auto seeded = fixture.seed();
    auto started = fixture.tracking.start(
        TimeTarget{TimeTargetKind::SubTopic, seeded.subTopic.id}, "old");
    REQUIRE(started.succeeded());
    fixture.clock.set(test::requireUtc("2026-10-23T11:00:00Z"));
    REQUIRE(fixture.tracking.stop("old").succeeded());
    auto edited = fixture.tracking.updateClosedNote(started.value().id, "new");
    REQUIRE(edited.succeeded());
    CHECK(edited.value().notes == "new");
    REQUIRE(fixture.tracking.deleteEntry(started.value().id).succeeded());
    auto missing = fixture.tracking.deleteEntry(started.value().id);
    CHECK_FALSE(missing.succeeded());
    CHECK(missing.error() == "Time entry not found.");
}

TEST_CASE("deleting the only time entry lets the topic be deleted") {
    Fixture fixture;
    fixture.open();
    auto category = fixture.catalog.createCategory("Work", "");
    REQUIRE(category.succeeded());
    auto mainTopic = fixture.catalog.createMainTopic(category.value().id, "Design", "");
    REQUIRE(mainTopic.succeeded());
    auto started = fixture.tracking.start(
        TimeTarget{TimeTargetKind::MainTopic, mainTopic.value().id}, "drop me");
    REQUIRE(started.succeeded());
    REQUIRE(fixture.tracking.stop("drop me").succeeded());
    REQUIRE(fixture.tracking.deleteEntry(started.value().id).succeeded());
    CHECK(fixture.catalog.deleteMainTopic(mainTopic.value().id).succeeded());
}

TEST_CASE("start on a missing topic is rejected") {
    Fixture fixture;
    fixture.open();
    auto started = fixture.tracking.start(TimeTarget{TimeTargetKind::MainTopic, 404}, "");
    CHECK_FALSE(started.succeeded());
    CHECK(started.error() == "Main topic not found.");
}

TEST_CASE("a time entry note is stored as text") {
    Fixture fixture;
    fixture.open();
    auto seeded = fixture.seed();
    const std::string notes = "'; DROP TABLE TimeEntry; --";
    auto started = fixture.tracking.start(
        TimeTarget{TimeTargetKind::MainTopic, seeded.mainTopic.id}, notes);
    REQUIRE(started.succeeded());
    CHECK(started.value().notes == notes);
    auto categories = fixture.catalog.loadAll();
    REQUIRE(categories.succeeded());
    CHECK(categories.value().categories.size() == 1);
}

TEST_CASE("recent entries page forward without overlap or skips") {
    Fixture fixture;
    fixture.open();
    auto seeded = fixture.seed();
    const TimeTarget target{TimeTargetKind::MainTopic, seeded.mainTopic.id};

    fixture.clock.set(test::requireUtc("2026-10-23T08:00:00Z"));
    REQUIRE(fixture.tracking.start(target, "older").succeeded());
    REQUIRE(fixture.tracking.stop("older").succeeded());

    fixture.clock.set(test::requireUtc("2026-10-23T08:00:00Z"));
    REQUIRE(fixture.tracking.start(target, "newer").succeeded());
    REQUIRE(fixture.tracking.stop("newer").succeeded());

    fixture.clock.set(test::requireUtc("2026-10-23T12:00:00Z"));
    REQUIRE(fixture.tracking.start(target, "latest").succeeded());
    REQUIRE(fixture.tracking.stop("latest").succeeded());

    auto first = fixture.tracking.listRecent(target, 2, std::nullopt);
    REQUIRE(first.succeeded());
    REQUIRE(first.value().size() == 2);
    CHECK(first.value()[0].notes == "latest");
    CHECK(first.value()[1].notes == "newer");

    RecentCursor cursor;
    cursor.startedAt = first.value()[1].startedAt;
    cursor.id = first.value()[1].id;
    auto second = fixture.tracking.listRecent(target, 2, cursor);
    REQUIRE(second.succeeded());
    REQUIRE(second.value().size() == 1);
    CHECK(second.value()[0].notes == "older");
    CHECK(second.value()[0].id != first.value()[0].id);
    CHECK(second.value()[0].id != first.value()[1].id);
}

TEST_CASE("a running entry is not in the closed history") {
    Fixture fixture;
    fixture.open();
    auto seeded = fixture.seed();
    const TimeTarget target{TimeTargetKind::MainTopic, seeded.mainTopic.id};
    REQUIRE(fixture.tracking.start(target, "closed").succeeded());
    REQUIRE(fixture.tracking.stop("closed").succeeded());
    auto running = fixture.tracking.start(target, "open");
    REQUIRE(running.succeeded());
    auto recent = fixture.tracking.listRecent(target, 10, std::nullopt);
    REQUIRE(recent.succeeded());
    REQUIRE(recent.value().size() == 1);
    CHECK(recent.value()[0].notes == "closed");
    CHECK(recent.value()[0].id != running.value().id);
}

TEST_CASE("a non-positive history limit returns no rows") {
    Fixture fixture;
    fixture.open();
    auto seeded = fixture.seed();
    const TimeTarget target{TimeTargetKind::MainTopic, seeded.mainTopic.id};
    REQUIRE(fixture.tracking.start(target, "one").succeeded());
    REQUIRE(fixture.tracking.stop("one").succeeded());
    auto page = fixture.tracking.listRecent(target, 0, std::nullopt);
    REQUIRE(page.succeeded());
    CHECK(page.value().empty());
}

TEST_CASE("history for one topic does not include another topic") {
    Fixture fixture;
    fixture.open();
    auto seeded = fixture.seed();
    REQUIRE(fixture.tracking
                .start(TimeTarget{TimeTargetKind::SubTopic, seeded.subTopic.id}, "sub")
                .succeeded());
    REQUIRE(fixture.tracking.stop("sub").succeeded());
    auto mainPage = fixture.tracking.listRecent(
        TimeTarget{TimeTargetKind::MainTopic, seeded.mainTopic.id}, 10, std::nullopt);
    REQUIRE(mainPage.succeeded());
    CHECK(mainPage.value().empty());
    auto subPage = fixture.tracking.listRecent(
        TimeTarget{TimeTargetKind::SubTopic, seeded.subTopic.id}, 10, std::nullopt);
    REQUIRE(subPage.succeeded());
    CHECK(subPage.value().size() == 1);
}

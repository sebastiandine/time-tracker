#include "support/Fixture.hpp"
#include "support/TempDir.hpp"

#include "timetracker/infra/SqliteDatabase.hpp"
#include "timetracker/infra/SqliteStmt.hpp"

#include <doctest/doctest.h>

#include <sqlite3.h>

#include <string>

using namespace timetracker;

namespace {

void clearAuthorizer(sqlite3* db) { sqlite3_set_authorizer(db, nullptr, nullptr); }

}  // namespace

TEST_CASE("a fresh database is at the current schema") {
    SqliteDatabase db;
    REQUIRE(db.openMemory().succeeded());
    clearAuthorizer(db.handle());
    auto statement = sqlite3_prepare_v2;
    sqlite3_stmt* raw = nullptr;
    REQUIRE(statement(db.handle(), "PRAGMA user_version;", -1, &raw, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_step(raw) == SQLITE_ROW);
    CHECK(sqlite3_column_int(raw, 0) == kSchemaVersion);
    sqlite3_finalize(raw);

    REQUIRE(db.installAuthorizer().succeeded());
    const char* names[] = {"Category", "MainTopic", "SubTopic", "TimeEntry", "Setting"};
    for (const char* name : names) {
        sqlite3_stmt* query = nullptr;
        REQUIRE(sqlite3_prepare_v2(db.handle(), "SELECT sql FROM sqlite_master WHERE name = ?;", -1,
                                   &query, nullptr) == SQLITE_OK);
        sqlite3_bind_text(query, 1, name, -1, SQLITE_TRANSIENT);
        REQUIRE(sqlite3_step(query) == SQLITE_ROW);
        const auto sql = reinterpret_cast<const char*>(sqlite3_column_text(query, 0));
        REQUIRE(sql != nullptr);
        CHECK(std::string(sql).find("STRICT") != std::string::npos);
        sqlite3_finalize(query);
    }
}

TEST_CASE("an empty previous database migrates to the current schema") {
    SqliteDatabase db;
    REQUIRE(db.openMemory().succeeded());
    clearAuthorizer(db.handle());
    REQUIRE(sqlite3_exec(db.handle(),
                         "DROP TABLE IF EXISTS TimeEntry;"
                         "DROP TABLE IF EXISTS SubTopic;"
                         "DROP TABLE IF EXISTS MainTopic;"
                         "DROP TABLE IF EXISTS Setting;"
                         "DROP TABLE IF EXISTS Category;"
                         "PRAGMA user_version = 0;",
                         nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(db.migrate().succeeded());
    sqlite3_stmt* raw = nullptr;
    REQUIRE(sqlite3_prepare_v2(db.handle(), "PRAGMA user_version;", -1, &raw, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_step(raw) == SQLITE_ROW);
    CHECK(sqlite3_column_int(raw, 0) == kSchemaVersion);
    sqlite3_finalize(raw);
}

TEST_CASE("a newer database is rejected") {
    SqliteDatabase db;
    REQUIRE(db.openMemory().succeeded());
    clearAuthorizer(db.handle());
    const std::string sql =
        "PRAGMA user_version = " + std::to_string(kSchemaVersion + 1) + ";";
    REQUIRE(sqlite3_exec(db.handle(), sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK);
    auto migrated = db.migrate();
    CHECK_FALSE(migrated.succeeded());
    CHECK(migrated.error().find("newer") != std::string::npos);
}

TEST_CASE("the authorizer denies attach") {
    SqliteDatabase db;
    REQUIRE(db.openMemory().succeeded());
    char* message = nullptr;
    const int rc =
        sqlite3_exec(db.handle(), "ATTACH DATABASE ':memory:' AS extra;", nullptr, nullptr, &message);
    CHECK(rc != SQLITE_OK);
    sqlite3_free(message);
}

TEST_CASE("text bound into an integer column fails") {
    SqliteDatabase db;
    REQUIRE(db.openMemory().succeeded());
    sqlite3_stmt* statement = nullptr;
    REQUIRE(sqlite3_prepare_v2(db.handle(),
                               "INSERT INTO Category (id, name, notes) VALUES (?, 'A', '');", -1,
                               &statement, nullptr) == SQLITE_OK);
    sqlite3_bind_text(statement, 1, "nope", -1, SQLITE_TRANSIENT);
    CHECK(sqlite3_step(statement) != SQLITE_DONE);
    sqlite3_finalize(statement);
}

TEST_CASE("a check constraint rejects a time entry with no topic") {
    SqliteDatabase db;
    REQUIRE(db.openMemory().succeeded());
    char* message = nullptr;
    const int rc = sqlite3_exec(db.handle(),
                                "INSERT INTO TimeEntry (main_topic_id, sub_topic_id, started_at, "
                                "ended_at, is_running, notes) VALUES (NULL, NULL, "
                                "'2026-01-01T00:00:00Z', NULL, 1, '');",
                                nullptr, nullptr, &message);
    CHECK(rc != SQLITE_OK);
    sqlite3_free(message);
}

TEST_CASE("backup copies rows to another memory database") {
    test::Fixture source;
    source.open();
    auto created = source.catalog.createCategory("Work", "notes");
    REQUIRE(created.succeeded());

    SqliteDatabase destination;
    REQUIRE(destination.openMemory().succeeded());
    REQUIRE(source.db.backupTo(destination.handle()).succeeded());

    SqliteCatalogRepository copied(destination);
    auto listed = copied.listCategories();
    REQUIRE(listed.succeeded());
    REQUIRE(listed.value().size() == 1);
    CHECK(listed.value()[0].name == "Work");
    CHECK(listed.value()[0].notes == "notes");
}

TEST_CASE("backup to a null destination fails") {
    SqliteDatabase db;
    REQUIRE(db.openMemory().succeeded());
    CHECK_FALSE(db.backupTo(nullptr).succeeded());
}

TEST_CASE("opening a memory database twice closes the first connection") {
    SqliteDatabase db;
    REQUIRE(db.openMemory().succeeded());
    REQUIRE(db.openMemory().succeeded());
}

TEST_CASE("opening a directory as a database file fails") {
    test::TempDir dir;
    SqliteDatabase db;
    auto opened = db.open(dir.path);
    CHECK_FALSE(opened.succeeded());
    CHECK(opened.error().find("Failed to open database") != std::string::npos);
}

TEST_CASE("a file database opens") {
    test::TempDir dir;
    {
        SqliteDatabase db;
        auto opened = db.open(dir.path / "time_tracker.db");
        REQUIRE(opened.succeeded());
    }
}

TEST_CASE("a new file database has no categories, topics, or time entries") {
    test::TempDir dir;
    SqliteDatabase db;
    REQUIRE(db.open(dir.path / "time_tracker.db").succeeded());
    const char* tables[] = {"Category", "MainTopic", "SubTopic", "TimeEntry"};
    for (const char* table : tables) {
        const auto sql = std::string("SELECT COUNT(*) FROM ") + table + ";";
        sqlite3_stmt* query = nullptr;
        REQUIRE(sqlite3_prepare_v2(db.handle(), sql.c_str(), -1, &query, nullptr) == SQLITE_OK);
        REQUIRE(sqlite3_step(query) == SQLITE_ROW);
        CHECK(sqlite3_column_int(query, 0) == 0);
        sqlite3_finalize(query);
    }
}

TEST_CASE("a second open timer is rejected by the database") {
    test::Fixture fixture;
    fixture.open();
    auto seeded = fixture.seed();
    TimeEntryDraft first;
    first.target = TimeTarget{TimeTargetKind::MainTopic, seeded.mainTopic.id};
    first.startedAt = "2026-10-23T10:00:00Z";
    first.notes = "one";
    REQUIRE(fixture.timeRepo.insertOpen(first).succeeded());
    TimeEntryDraft second = first;
    second.notes = "two";
    auto rejected = fixture.timeRepo.insertOpen(second);
    CHECK_FALSE(rejected.succeeded());
    CHECK(rejected.error() == "A timer is already running.");
}

TEST_CASE("a time entry for a missing topic is rejected") {
    test::Fixture fixture;
    fixture.open();
    TimeEntryDraft draft;
    draft.target = TimeTarget{TimeTargetKind::MainTopic, 404};
    draft.startedAt = "2026-10-23T10:00:00Z";
    draft.notes = "";
    auto rejected = fixture.timeRepo.insertOpen(draft);
    CHECK_FALSE(rejected.succeeded());
    CHECK(rejected.error() == "That topic no longer exists.");
}

TEST_CASE("a failed prepare leaves no statement") {
    SqliteDatabase db;
    REQUIRE(db.openMemory().succeeded());
    int rc = 0;
    auto statement = prepareStmt(db.handle(), "THIS IS NOT SQL", rc);
    CHECK(rc != SQLITE_OK);
    CHECK(statement == nullptr);
}

TEST_CASE("the statement deleter ignores a null pointer") {
    SqliteStmtDeleter deleter;
    deleter(nullptr);
}

TEST_CASE("a connection with no error is not a constraint") {
    SqliteDatabase db;
    REQUIRE(db.openMemory().succeeded());
    CHECK(classifyConstraint(db.handle()) == SqliteConstraint::Other);
}

TEST_CASE("binding null into a required name is a not-null constraint") {
    SqliteDatabase db;
    REQUIRE(db.openMemory().succeeded());
    sqlite3_stmt* statement = nullptr;
    REQUIRE(sqlite3_prepare_v2(db.handle(), "INSERT INTO Category (name, notes) VALUES (?, '');", -1,
                               &statement, nullptr) == SQLITE_OK);
    sqlite3_bind_null(statement, 1);
    CHECK(sqlite3_step(statement) != SQLITE_DONE);
    CHECK(classifyConstraint(db.handle()) == SqliteConstraint::NotNull);
    sqlite3_finalize(statement);
}

TEST_CASE("a repeated primary key is a unique constraint") {
    SqliteDatabase db;
    REQUIRE(db.openMemory().succeeded());
    REQUIRE(sqlite3_exec(db.handle(), "INSERT INTO Setting (key, value) VALUES ('theme', 'light');",
                         nullptr, nullptr, nullptr) == SQLITE_OK);
    CHECK(sqlite3_exec(db.handle(), "INSERT INTO Setting (key, value) VALUES ('theme', 'dark');",
                       nullptr, nullptr, nullptr) != SQLITE_OK);
    CHECK(classifyConstraint(db.handle()) == SqliteConstraint::Unique);
}

TEST_CASE("a main topic for a missing category is rejected") {
    test::Fixture fixture;
    fixture.open();
    auto rejected = fixture.catalogRepo.createMainTopic(404, "Design", "");
    CHECK_FALSE(rejected.succeeded());
    CHECK(rejected.error() == "Category not found.");
}

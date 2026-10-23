#include "timetracker/infra/SqliteDatabase.hpp"

#include "timetracker/infra/SqliteStmt.hpp"

#include <sqlite3.h>

#include <string>
#include <utility>

namespace timetracker {
namespace {

[[nodiscard]] Result<void> execSql(sqlite3* db, const char* sql, const char* prefix) {
    char* errMsg = nullptr;
    const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::string message = errMsg != nullptr ? errMsg : "unknown error";
        sqlite3_free(errMsg);
        return Result<void>::err(std::string(prefix) + message);
    }
    return Result<void>::ok();
}

[[nodiscard]] Result<int> readUserVersion(sqlite3* db) {
    int rc = 0;
    auto statement = prepareStmt(db, "PRAGMA user_version;", rc);
    if (rc != SQLITE_OK) {
        return Result<int>::err(std::string("Failed to read schema version: ") + sqlite3_errmsg(db));
    }
    if (sqlite3_step(statement.get()) != SQLITE_ROW) {
        return Result<int>::err("Failed to read schema version.");
    }
    return Result<int>::ok(sqlite3_column_int(statement.get(), 0));
}

[[nodiscard]] Result<void> rollback(sqlite3* db, Result<void> error) {
    sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
    return error;
}

[[nodiscard]] Result<void> migrateTo1(sqlite3* db) {
    auto begun = execSql(db, "BEGIN IMMEDIATE;", "Migration 1 failed: ");
    if (!begun) {
        return begun;
    }
    auto created = execSql(db,
                           "CREATE TABLE Category ("
                           "  id    INTEGER PRIMARY KEY,"
                           "  name  TEXT NOT NULL UNIQUE COLLATE NOCASE,"
                           "  notes TEXT NOT NULL DEFAULT ''"
                           ") STRICT;"
                           "CREATE TABLE MainTopic ("
                           "  id          INTEGER PRIMARY KEY,"
                           "  category_id INTEGER NOT NULL REFERENCES Category(id),"
                           "  name        TEXT NOT NULL COLLATE NOCASE,"
                           "  notes       TEXT NOT NULL DEFAULT '',"
                           "  UNIQUE (category_id, name)"
                           ") STRICT;"
                           "CREATE TABLE SubTopic ("
                           "  id            INTEGER PRIMARY KEY,"
                           "  main_topic_id INTEGER NOT NULL REFERENCES MainTopic(id),"
                           "  name          TEXT NOT NULL COLLATE NOCASE,"
                           "  notes         TEXT NOT NULL DEFAULT '',"
                           "  UNIQUE (main_topic_id, name)"
                           ") STRICT;"
                           "CREATE TABLE TimeEntry ("
                           "  id            INTEGER PRIMARY KEY,"
                           "  main_topic_id INTEGER REFERENCES MainTopic(id),"
                           "  sub_topic_id  INTEGER REFERENCES SubTopic(id),"
                           "  started_at    TEXT NOT NULL,"
                           "  ended_at      TEXT,"
                           "  is_running    INTEGER NOT NULL CHECK (is_running IN (0, 1)),"
                           "  notes         TEXT NOT NULL DEFAULT '',"
                           "  CHECK ((main_topic_id IS NOT NULL AND sub_topic_id IS NULL) OR"
                           "         (main_topic_id IS NULL AND sub_topic_id IS NOT NULL)),"
                           "  CHECK ((ended_at IS NULL AND is_running = 1) OR"
                           "         (ended_at IS NOT NULL AND is_running = 0))"
                           ") STRICT;"
                           "CREATE TABLE Setting ("
                           "  key   TEXT PRIMARY KEY,"
                           "  value TEXT NOT NULL,"
                           "  CHECK (key IN ('theme'))"
                           ") STRICT;"
                           "CREATE UNIQUE INDEX idx_time_entry_single_open"
                           "  ON TimeEntry (is_running) WHERE is_running = 1;"
                           "CREATE INDEX idx_time_entry_started"
                           "  ON TimeEntry (started_at, id);"
                           "CREATE INDEX idx_time_entry_main"
                           "  ON TimeEntry (main_topic_id, started_at DESC, id DESC);"
                           "CREATE INDEX idx_time_entry_sub"
                           "  ON TimeEntry (sub_topic_id, started_at DESC, id DESC);",
                           "Migration 1 failed: ");
    if (!created) {
        return rollback(db, created);
    }
    auto stamped = execSql(db, "PRAGMA user_version = 1;", "Migration 1 failed: ");
    if (!stamped) {
        return rollback(db, stamped);
    }
    auto committed = execSql(db, "COMMIT;", "Migration 1 failed: ");
    if (!committed) {
        return rollback(db, committed);
    }
    return Result<void>::ok();
}

[[nodiscard]] Result<void> applyStep(sqlite3* db, int version) {
    switch (version) {
    case 1:
        return migrateTo1(db);
    default:
        return Result<void>::err("Unknown schema migration step.");
    }
}

int sqliteAuth(void* /*user*/, int action, const char* /*arg1*/, const char* /*arg2*/,
               const char* /*db*/, const char* /*trigger*/) {
    switch (action) {
    case SQLITE_SELECT:
    case SQLITE_READ:
    case SQLITE_INSERT:
    case SQLITE_UPDATE:
    case SQLITE_DELETE:
    case SQLITE_TRANSACTION:
    case SQLITE_SAVEPOINT:
    case SQLITE_FUNCTION:
        return SQLITE_OK;
    default:
        return SQLITE_DENY;
    }
}

}  // namespace

SqliteDatabase::~SqliteDatabase() { close(); }

void SqliteDatabase::close() noexcept {
    if (db_ != nullptr) {
        sqlite3_close_v2(db_);
        db_ = nullptr;
    }
}

Result<void> SqliteDatabase::openRaw(const char* path, bool fileDb) {
    close();
    sqlite3* raw = nullptr;
    const int rc = sqlite3_open_v2(path, &raw, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    db_ = raw;
    if (rc != SQLITE_OK) {
        std::string message = db_ != nullptr ? sqlite3_errmsg(db_) : "unknown error";
        close();
        return Result<void>::err("Failed to open database: " + message);
    }
    return configure(fileDb);
}

Result<void> SqliteDatabase::open(const std::filesystem::path& dbPath) {
    return openRaw(dbPath.generic_string().c_str(), true);
}

Result<void> SqliteDatabase::openMemory() { return openRaw(":memory:", false); }

Result<void> SqliteDatabase::configure(bool fileDb) {
    sqlite3_extended_result_codes(db_, 1);
    sqlite3_busy_timeout(db_, 5000);
    auto foreignKeys = execSql(db_, "PRAGMA foreign_keys = ON;", "Failed to enable foreign keys: ");
    if (!foreignKeys) {
        return foreignKeys;
    }
    if (fileDb) {
        auto wal = execSql(db_, "PRAGMA journal_mode = WAL;", "Failed to enable WAL: ");
        if (!wal) {
            return wal;
        }
        auto synchronous =
            execSql(db_, "PRAGMA synchronous = NORMAL;", "Failed to set synchronous: ");
        if (!synchronous) {
            return synchronous;
        }
    }
    auto migrated = migrate();
    if (!migrated) {
        return migrated;
    }
    return installAuthorizer();
}

Result<void> SqliteDatabase::migrate() {
    auto current = readUserVersion(db_);
    if (!current) {
        return Result<void>::err(current.error());
    }
    if (current.value() > kSchemaVersion) {
        return Result<void>::err("Database schema is newer than this build.");
    }
    for (int step = current.value() + 1; step <= kSchemaVersion; ++step) {
        auto applied = applyStep(db_, step);
        if (!applied) {
            return applied;
        }
    }
    return Result<void>::ok();
}

Result<void> SqliteDatabase::installAuthorizer() {
    const int rc = sqlite3_set_authorizer(db_, sqliteAuth, nullptr);
    if (rc != SQLITE_OK) {
        return Result<void>::err("Failed to install SQL authorizer.");
    }
    return Result<void>::ok();
}

Result<void> SqliteDatabase::backupTo(sqlite3* destination) {
    if (destination == nullptr || db_ == nullptr) {
        return Result<void>::err("Backup failed: unknown error");
    }
    sqlite3_backup* backup = sqlite3_backup_init(destination, "main", db_, "main");
    if (backup == nullptr) {
        return Result<void>::err(std::string("Backup failed: ") + sqlite3_errmsg(destination));
    }
    const int rc = sqlite3_backup_step(backup, -1);
    sqlite3_backup_finish(backup);
    if (rc != SQLITE_DONE) {
        return Result<void>::err(std::string("Backup failed: ") + sqlite3_errmsg(db_));
    }
    return Result<void>::ok();
}

}  // namespace timetracker

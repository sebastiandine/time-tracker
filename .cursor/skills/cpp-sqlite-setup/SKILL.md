---
name: cpp-sqlite-setup
description: >-
  Embeds SQLite 3.53.4 (amalgamation) in a C++20 CMake repo: FetchContent pin,
  RAII connection, user_version migration runner, WAL, bound parameters,
  extended result codes, and the backup API. Use when adding or changing the
  amalgamation, sqlite3_open_v2, IDatabaseSession, WAL, or the backup API.
  Table shape, indexes, and queries are cpp-sqlite.
---

# C++ SQLite Setup

Apply when the program needs an embedded SQLite file: pin, connection,
migration **runner**, WAL, bind helpers, or backup. Do not re-scaffold
CMake, the filesystem port, or the test harness. Do not add a second
SQLite (system package, SQLiteCpp, ORM). Tests follow
`cpp-unit-testing`. Statement ownership follows `cpp-code-style-security`.
JSON **files** stay in `cpp-json-files`. Table shape, indexes, CHECK,
STRICT, query generation, the authorizer, and pagination are `cpp-sqlite`.

Substitute `PROJ`, `PREFIX`, and `NS`. Do not leave those tokens in the
project. Skip steps that already match. Fill gaps; do not rewrite a
healthy tree.

```
SQLite setup progress (copy into the task):
- [ ] Amalgamation 3.53.4 static `sqlite3`; C language enabled
- [ ] IDatabaseSession port; SqliteDatabase adapter; sqlite3.h only in infra .cpp
- [ ] open_v2 / close_v2; extended codes; busy 5000 ms; foreign_keys ON; WAL on files
- [ ] PRAGMA user_version runner; reject a newer database
- [ ] Bound parameters; SqliteStmt unique_ptr; extended_errcode (not errmsg text)
- [ ] BEGIN IMMEDIATE for any change with more than one statement
- [ ] Live copy only through sqlite3_backup_*; schema page under docs/
- [ ] Tests listed below; :memory: only
- [ ] Table shape, indexes, queries, authorizer: cpp-sqlite
- [ ] Stop
```

# Invariants

- Pin **SQLite 3.53.4** amalgamation. No `find_package(SQLite3)`, no git
  mirror of `sqlite3.c`, no second database library. Public domain /
  blessing (MIT-compatible). `project()` lists `C` and `CXX`.
- Link `sqlite3` **PRIVATE** on the logic target. A test target that
  includes `sqlite3.h` links `sqlite3` itself. Register infra `.cpp`
  files in `core/CMakeLists.txt` (no glob). Do not bump the pin as a
  side effect of a schema change.
- `sqlite3.h` only in `core/src/infra/*.cpp` and adapter tests. The
  connection header forward-declares `struct sqlite3`. Repository
  headers forward-declare `SqliteDatabase`. Services and the UI target
  never include `sqlite3.h`.
- One connection, owned by the composition root (`unique_ptr`), passed
  by reference into adapters. Session port: `open(path)` returns `Result`;
  no SQLite types. Repository ports speak domain types.
- Filesystem port creates the parent directory, then
  `open(dataDir / "PROJ.db")`. Persist the directory as
  `path::generic_string()`.
- `sqlite3_open_v2` (`READWRITE | CREATE`) on `generic_string()` (UTF-8).
  On failure, `sqlite3_close_v2` the handle SQLite may still have set.
  Destructor/`close` use `sqlite3_close_v2`. Non-copyable. Do not pass
  `SQLITE_OPEN_NOMUTEX`.
- After a successful open, in order: extended result codes on,
  `sqlite3_busy_timeout` **5000** ms, `PRAGMA foreign_keys = ON`, then
  for a **file** database `PRAGMA journal_mode = WAL` and
  `PRAGMA synchronous = NORMAL`. `:memory:` skips WAL. Then migrate.
- Schema version is `PRAGMA user_version`. Do not evolve schema with
  `CREATE TABLE IF NOT EXISTS` on every open. A newer `user_version`
  than this build fails the open. The runner lives here; the SQL inside
  each step is `cpp-sqlite`.
- Bound parameters (`SQLITE_TRANSIENT` for text). Never concatenate user
  input into SQL. `sqlite3_exec` is trusted migration/`PRAGMA` text only.
  Copy `sqlite3_column_text` into `std::string` before the next step,
  reset, or finalize. Map failures with `sqlite3_extended_errcode`, not
  English `errmsg` text.
- More than one changing statement is one `BEGIN IMMEDIATE` transaction;
  failure rolls back.
- Copy a live database only with `sqlite3_backup_init` / `step` /
  `finish`. Do not copy the file, `-wal`, or `-shm` while a connection
  is open. After the only connection `close_v2`s, the checkpointed file
  may be copied as one file.
- `INTEGER PRIMARY KEY` for ids. `AUTOINCREMENT` only when an id must
  never be reused. `FOREIGN KEY` without `ON DELETE CASCADE` unless the
  task asks. `COLLATE NOCASE` folds ASCII `A-Z` only; Unicode case-fold
  in application code before write and compare.
- No commit, push, or branch switch unless the user explicitly asked.

# CMake

In `cmake/Dependencies.cmake`. Do **not** `FetchContent_MakeAvailable`
this zip: CMake 3.22 always `add_subdirectory`, and the amalgamation has
no `CMakeLists.txt`.

```cmake
message(STATUS "[PROJ] Fetching SQLite amalgamation ...")
FetchContent_Declare(
    sqlite_amalgamation
    URL                    https://www.sqlite.org/2026/sqlite-amalgamation-3530400.zip
    URL_HASH               SHA3_256=628a44cfe82c66aed1ccbbe85a562d2e33ebe64b3288981ed76285612227934e
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_GetProperties(sqlite_amalgamation)
if(NOT sqlite_amalgamation_POPULATED)
    FetchContent_Populate(sqlite_amalgamation)
endif()

add_library(sqlite3 STATIC
    ${sqlite_amalgamation_SOURCE_DIR}/sqlite3.c
)
target_include_directories(sqlite3
    PUBLIC $<BUILD_INTERFACE:${sqlite_amalgamation_SOURCE_DIR}>
)
target_compile_definitions(sqlite3 PUBLIC
    SQLITE_THREADSAFE=1
    SQLITE_OMIT_LOAD_EXTENSION=1
    SQLITE_DQS=0
    SQLITE_DEFAULT_FOREIGN_KEYS=1
)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(sqlite3 PRIVATE -w)
endif()
if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(sqlite3 PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
endif()
```

`target_link_libraries(PROJ_core PRIVATE sqlite3)` and the same PRIVATE
link on `PROJ_core_tests`.

# Ports and layering

`core/include/NS/ports/IDatabaseSession.hpp`. No SQLite types.

```cpp
class IDatabaseSession {
public:
    virtual ~IDatabaseSession() = default;
    [[nodiscard]] virtual Result<void> open(const std::filesystem::path& dbPath) = 0;
};
```

```cpp
auto ensured = fs.ensureDirectory(dataDir);
if (!ensured) return ensured;
return session.open(dataDir / "PROJ.db");
```

Adapters take `SqliteDatabase&`. The composition root constructs
`SqliteDatabase`, then each `Sqlite*Repository`, then services that
depend only on ports.

# Connection (copy)

`core/include/NS/infra/SqliteStmt.hpp` — infra `.cpp` only.

```cpp
#pragma once
#include <memory>
struct sqlite3;
struct sqlite3_stmt;
namespace NS {
struct SqliteStmtDeleter {
    void operator()(sqlite3_stmt* s) const noexcept;
};
using SqliteStmt = std::unique_ptr<sqlite3_stmt, SqliteStmtDeleter>;
enum class SqliteConstraint { None, Unique, ForeignKey, NotNull, Check, Other };
[[nodiscard]] SqliteStmt prepareStmt(sqlite3* db, const char* sql, int& rc);
[[nodiscard]] SqliteConstraint classifyConstraint(sqlite3* db) noexcept;
}
```

`core/include/NS/infra/SqliteDatabase.hpp`. `handle()` is for adapters
and tests, not services. Bump `kSchemaVersion` only with a new step.
`openMemory` / `migrate` are public so tests can install a previous
`user_version` and upgrade.

```cpp
#pragma once
#include "NS/ports/IDatabaseSession.hpp"
#include "NS/util/Result.hpp"
#include <filesystem>
struct sqlite3;
namespace NS {
inline constexpr int kSchemaVersion = 1;
class SqliteDatabase final : public IDatabaseSession {
public:
    SqliteDatabase() = default;
    ~SqliteDatabase() override;
    SqliteDatabase(const SqliteDatabase&) = delete;
    SqliteDatabase& operator=(const SqliteDatabase&) = delete;
    [[nodiscard]] Result<void> open(const std::filesystem::path& dbPath) override;
    [[nodiscard]] Result<void> openMemory();
    [[nodiscard]] Result<void> migrate();
    [[nodiscard]] Result<void> backupTo(sqlite3* dest);
    [[nodiscard]] sqlite3* handle() const noexcept { return db_; }
private:
    void close() noexcept;
    [[nodiscard]] Result<void> openRaw(const char* path, bool fileDb);
    [[nodiscard]] Result<void> configure(bool fileDb);
    sqlite3* db_{nullptr};
};
}
```

# Open, statements, configure (copy)

`core/src/infra/SqliteStmt.cpp` and `SqliteDatabase.cpp`. `<sqlite3.h>`
only here. A failed prepare leaves a null `unique_ptr`.

```cpp
void SqliteStmtDeleter::operator()(sqlite3_stmt* s) const noexcept {
    if (s != nullptr) sqlite3_finalize(s);
}
SqliteStmt prepareStmt(sqlite3* db, const char* sql, int& rc) {
    sqlite3_stmt* raw = nullptr;
    rc = sqlite3_prepare_v2(db, sql, -1, &raw, nullptr);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(raw);
        return {};
    }
    return SqliteStmt{raw};
}
SqliteConstraint classifyConstraint(sqlite3* db) noexcept {
    switch (sqlite3_extended_errcode(db)) {
    case SQLITE_CONSTRAINT_UNIQUE:
    case SQLITE_CONSTRAINT_PRIMARYKEY: return SqliteConstraint::Unique;
    case SQLITE_CONSTRAINT_FOREIGNKEY: return SqliteConstraint::ForeignKey;
    case SQLITE_CONSTRAINT_NOTNULL:    return SqliteConstraint::NotNull;
    case SQLITE_CONSTRAINT_CHECK:      return SqliteConstraint::Check;
    default:                           return SqliteConstraint::Other;
    }
}

namespace {
Result<void> execSql(sqlite3* db, const char* sql, const char* prefix) {
    char* errMsg = nullptr;
    const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::string msg = errMsg ? errMsg : "unknown error";
        sqlite3_free(errMsg);
        return Result<void>::err(std::string(prefix) + msg);
    }
    return Result<void>::ok();
}
Result<int> readUserVersion(sqlite3* db) {
    int rc = 0;
    auto stmt = prepareStmt(db, "PRAGMA user_version;", rc);
    if (rc != SQLITE_OK)
        return Result<int>::err(std::string("Failed to read schema version: ") + sqlite3_errmsg(db));
    if (sqlite3_step(stmt.get()) != SQLITE_ROW)
        return Result<int>::err("Failed to read schema version.");
    return Result<int>::ok(sqlite3_column_int(stmt.get(), 0));
}
Result<void> rollback(sqlite3* db, Result<void> err) {
    sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
    return err;
}
}

SqliteDatabase::~SqliteDatabase() { close(); }
void SqliteDatabase::close() noexcept {
    if (db_ != nullptr) { sqlite3_close_v2(db_); db_ = nullptr; }
}
Result<void> SqliteDatabase::openRaw(const char* path, bool fileDb) {
    close();
    sqlite3* raw = nullptr;
    const int rc = sqlite3_open_v2(
        path, &raw, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    db_ = raw;
    if (rc != SQLITE_OK) {
        std::string msg = db_ ? sqlite3_errmsg(db_) : "unknown error";
        close();
        return Result<void>::err("Failed to open database: " + msg);
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
    auto fk = execSql(db_, "PRAGMA foreign_keys = ON;", "Failed to enable foreign keys: ");
    if (!fk) return fk;
    if (fileDb) {
        auto wal = execSql(db_, "PRAGMA journal_mode = WAL;", "Failed to enable WAL: ");
        if (!wal) return wal;
        auto sync = execSql(db_, "PRAGMA synchronous = NORMAL;", "Failed to set synchronous: ");
        if (!sync) return sync;
    }
    return migrate();
}
```

# Migrations

Each step is one function that owns its transaction. Version 0 runs
steps 1..`kSchemaVersion`. Set `user_version` to that step **before**
`COMMIT`. Failure `ROLLBACK`s. Do not edit a step that already shipped.
The `CREATE TABLE` below only shows the runner; table shape is
`cpp-sqlite`.

`PRAGMA foreign_keys` is a no-op inside a transaction. A rebuild that
must drop or recreate a table turns foreign keys **off before** `BEGIN`,
runs `PRAGMA foreign_key_check` before `COMMIT`, then turns them on
again. On failure: `ROLLBACK`, then `PRAGMA foreign_keys = ON`.

```
PRAGMA foreign_keys = OFF;
BEGIN IMMEDIATE;
CREATE TABLE t_new (...); INSERT INTO t_new SELECT ... FROM t;
DROP TABLE t; ALTER TABLE t_new RENAME TO t;
PRAGMA foreign_key_check;
PRAGMA user_version = N;
COMMIT;
PRAGMA foreign_keys = ON;
```

```cpp
Result<void> migrateTo1(sqlite3* db) {
    auto begun = execSql(db, "BEGIN IMMEDIATE;", "Migration 1 failed: ");
    if (!begun) return begun;
    auto created = execSql(db,
        "CREATE TABLE Example ("
        "  id   INTEGER PRIMARY KEY,"
        "  name TEXT NOT NULL UNIQUE COLLATE NOCASE"
        ");",
        "Migration 1 failed: ");
    if (!created) return rollback(db, created);
    auto stamped = execSql(db, "PRAGMA user_version = 1;", "Migration 1 failed: ");
    if (!stamped) return rollback(db, stamped);
    auto committed = execSql(db, "COMMIT;", "Migration 1 failed: ");
    if (!committed) return rollback(db, committed);
    return Result<void>::ok();
}
Result<void> applyStep(sqlite3* db, int version) {
    switch (version) {
    case 1: return migrateTo1(db);
    default: return Result<void>::err("Unknown schema migration step.");
    }
}
Result<void> SqliteDatabase::migrate() {
    auto current = readUserVersion(db_);
    if (!current) return Result<void>::err(current.error());
    if (*current > kSchemaVersion)
        return Result<void>::err("Database schema is newer than this build.");
    for (int step = *current + 1; step <= kSchemaVersion; ++step) {
        auto applied = applyStep(db_, step);
        if (!applied) return applied;
    }
    return Result<void>::ok();
}
```

One `CREATE TABLE` per new entity in the step that introduces it.
`ALTER TABLE ... ADD COLUMN` with a `DEFAULT` is a new step. Follow
`cpp-sqlite` for STRICT, CHECK, and indexes.

# Queries

```cpp
Result<Row> SqliteRowRepository::create(const std::string& name) {
    const char* sql = "INSERT INTO Example (name) VALUES (?);";
    int rc = 0;
    auto stmt = prepareStmt(db_.handle(), sql, rc);
    if (rc != SQLITE_OK)
        return Result<Row>::err(std::string("Failed to prepare insert: ") + sqlite3_errmsg(db_.handle()));
    sqlite3_bind_text(stmt.get(), 1, name.c_str(), -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        if (classifyConstraint(db_.handle()) == SqliteConstraint::Unique)
            return Result<Row>::err("A row with that name already exists.");
        return Result<Row>::err(std::string("Failed to create row: ") + sqlite3_errmsg(db_.handle()));
    }
    Row row;
    row.id = sqlite3_last_insert_rowid(db_.handle());
    row.name = name;
    return Result<Row>::ok(std::move(row));
}
```

`sqlite3_bind_int64` for integer ids. On `SQLITE_ROW`, copy text before
the next `step`. Map `ForeignKey`, `NotNull`, and `Check` the same way
whenever that adapter can hit them. Filters, `IN` lists, `LIKE`, keyset
pages, and bulk loops are `cpp-sqlite`.

Two or more changing statements: `BEGIN IMMEDIATE`, statements, `COMMIT`;
on any failure `ROLLBACK` and return the error.

# Backup (copy)

```cpp
Result<void> SqliteDatabase::backupTo(sqlite3* dest) {
    sqlite3_backup* bak = sqlite3_backup_init(dest, "main", db_, "main");
    if (bak == nullptr)
        return Result<void>::err(std::string("Backup failed: ") + sqlite3_errmsg(dest));
    const int rc = sqlite3_backup_step(bak, -1);
    sqlite3_backup_finish(bak);
    if (rc != SQLITE_DONE)
        return Result<void>::err(std::string("Backup failed: ") + sqlite3_errmsg(db_));
    return Result<void>::ok();
}
```

# Schema docs

When a table is added, changed, or removed: add the migration step and
bump `kSchemaVersion`; update `docs/database/schema.md` (CREATE text +
column list, following `cpp-sqlite`); list the page in `docs/README.md`
and `docs/AGENTS.md`.

# Tests

Hermetic: `openMemory()` only. No host database file. List each new
`*_tests.cpp` in `tests/CMakeLists.txt`. Include `sqlite3.h` and link
`sqlite3`.

- Fresh `openMemory`: tables exist, `user_version == kSchemaVersion`.
- `PRAGMA user_version = kSchemaVersion + 1` then `migrate()` fails.
- Install the previous schema (or empty version 0), set that
  `user_version`, call `migrate()`, assert the new shape and current
  version.
- One case per constraint the adapter maps (`Unique`, `ForeignKey`,
  `NotNull`, `Check` if used). Assert the mapped `Result` error, not a
  substring of `sqlite3_errmsg`.
- `backupTo` a second `:memory:` connection; dest has the same rows.
- Bind a value that contains `'; DROP TABLE` — stored as text, not
  executed.

Do not add a test that copies `-wal` / `-shm` or the database file while
open. Authorizer, STRICT, keyset, and `IN`-list cases are `cpp-sqlite`.

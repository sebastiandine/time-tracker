---
name: cpp-sqlite
description: >-
  Writes SQLite schema and queries in a C++20 repo: STRICT tables, CHECK
  constraints, indexes, bound prepared statements, IN lists, LIKE escaping,
  keyset pagination, bulk inserts, an authorizer, and user_version migration
  steps. Use when adding or changing a table, index, CHECK, prepared
  statement, IN clause, keyset, bulk insert, schema, migration step, or
  authorizer. The amalgamation and connection are cpp-sqlite-setup.
---

# C++ SQLite

Apply when changing tables, indexes, migration-step SQL, repository
queries, bulk writes, the authorizer, or pagination. `cpp-sqlite-setup`
already provided `SqliteDatabase`, `prepareStmt`, `classifyConstraint`,
`kSchemaVersion`, WAL, and the migration runner. Do not re-fetch SQLite,
rewrite `open` / `close`, or add a second database library. Tests follow
the unit-test skill. Statement ownership follows the ownership skill.
The UI toolkit owns the worker / UI-thread hop; this skill only owns
the extra connection.

Substitute `PROJ` and `NS`. Do not leave those tokens in the project.
Skip steps that already match.

```
SQLite progress (copy into the task):
- [ ] STRICT tables; INTEGER/REAL/TEXT/BLOB; CHECK on closed sets and booleans
- [ ] Indexes match WHERE/JOIN/ORDER BY; documented; named idx_<table>_<columns>
- [ ] New user_version step; never edit an old step; stamp before COMMIT
- [ ] Bound SQL; explicit columns; IN placeholders; LIKE escaped
- [ ] No N+1; keyset pages; bulk reset/rebind in one BEGIN IMMEDIATE
- [ ] Authorizer after migrate; worker uses its own connection
- [ ] Tests listed below; :memory: only
- [ ] Stop
```

# Invariants

- Every `CREATE TABLE` is `STRICT`. Column types are only `INTEGER`,
  `REAL`, `TEXT`, or `BLOB`. No `ANY`. No `SELECT *`.
- Closed sets (`CHECK (col IN (...))`). Booleans are `INTEGER` with
  `CHECK (col IN (0, 1))`. Dates are ISO-8601 `TEXT`. Application code
  still rejects bad input; `CHECK` is the backstop.
- `INTEGER PRIMARY KEY` without `AUTOINCREMENT` unless an id must never
  be reused. `FOREIGN KEY` without `ON DELETE CASCADE` unless the task
  asks. `COLLATE NOCASE` is ASCII `A-Z` only; Unicode case-fold in
  application code before write and compare.
- Indexes: equality columns, then the range column, then `ORDER BY`
  columns. Do not duplicate a `UNIQUE` constraint. Skip extra indexes on
  a lookup table that is always loaded whole. Name `idx_<table>_<columns>`.
  Document each index beside the table in `docs/database/schema.md`.
- One static statement per method. Optional filters append
  `AND <allow-listed column> = ?` only. Never concatenate user text,
  table names, or column names from outside the process.
- `IN` lists are `?,?,?` with one bound value each. An empty list
  returns no rows and is never `IN ()`.
- `LIKE` binds the pattern after escaping `\`, `%`, and `_`, with
  `ESCAPE '\'`.
- A loop that would query once per parent row is one `JOIN` or one bound
  `IN` list.
- Ordered lists use a keyset (`ORDER BY` columns plus a unique `id`
  tie-break) and `LIMIT ?`. `OFFSET` only when the task asks for a small
  fixed page index.
- A loop or import is one `BEGIN IMMEDIATE`. Prepare once, then
  `sqlite3_reset`, rebind, and `step` per row. Failure rolls back.
- After `migrate`, `sqlite3_set_authorizer` allows only `SELECT`,
  `READ`, `INSERT`, `UPDATE`, `DELETE`, `TRANSACTION`, `SAVEPOINT`, and
  `FUNCTION`. Deny `ATTACH`, `DETACH`, `PRAGMA`, and DDL. `configure`
  and `migrate` run first. The backup API copies pages; it is not
  repository SQL.
- The UI connection and its statements stay on the UI thread. A worker
  opens a second connection to the same file and closes it before the
  worker ends. Do not share one `sqlite3*` behind a mutex. Do not hold
  `BEGIN` across a UI call.
- New and changed tables are a new `user_version` step. Do not edit an
  old step. Stamp `user_version` before `COMMIT`. A rebuild turns
  foreign keys **off before** `BEGIN IMMEDIATE`, runs
  `PRAGMA foreign_key_check` before `COMMIT`, then turns them on again.
- Map failures with `sqlite3_extended_errcode` (`classifyConstraint`),
  not English `errmsg` text. Copy `sqlite3_column_text` into
  `std::string` before the next `step`, `reset`, or finalize.
- No commit, push, or branch switch unless the user explicitly asked.

# Schema

Replace `Example` with this project's tables. One `CREATE TABLE` per new
entity in the step that introduces it. `ALTER TABLE ... ADD COLUMN`
with a `DEFAULT` is a new step.

```sql
CREATE TABLE Example (
  id     INTEGER PRIMARY KEY,
  name   TEXT    NOT NULL UNIQUE COLLATE NOCASE,
  status TEXT    NOT NULL CHECK (status IN ('Open', 'Closed')),
  active INTEGER NOT NULL CHECK (active IN (0, 1)),
  due_on TEXT    NOT NULL
) STRICT;
```

# Indexes

A composite index is useful when a query filters and sorts the same way.
Leftmost columns are equalities, then the range, then the sort.

```sql
CREATE INDEX idx_example_status_due
  ON Example (status, due_on DESC, id DESC);
```

That index serves:

```sql
SELECT id, name, status, active, due_on FROM Example
 WHERE status = ?
 ORDER BY due_on DESC, id DESC
 LIMIT ?;
```

`status` is the equality. `due_on DESC, id DESC` match the `ORDER BY`,
so SQLite can walk the index and stop at `LIMIT`. Do not add
`idx_example_name` if `name` is already `UNIQUE`.

# Migrations

Use the runner from `cpp-sqlite-setup`. Bump `kSchemaVersion`. Add
`case N: return migrateToN(db);`. Never change a shipped step.

```
PRAGMA foreign_keys = OFF;
BEGIN IMMEDIATE;
-- CREATE / rebuild / CREATE INDEX
PRAGMA foreign_key_check;
PRAGMA user_version = N;
COMMIT;
PRAGMA foreign_keys = ON;
```

`PRAGMA foreign_keys` is a no-op inside a transaction. On failure:
`ROLLBACK`, then `PRAGMA foreign_keys = ON`. These PRAGMAs run in the
migration step, **before** the authorizer is installed.

# Queries

Allow-listed filters append `AND col = ?`. Build `IN` placeholders from
the value count. Escape `LIKE` in C++, then bind. Keyset: compare the
sort columns plus `id`. Optional filters start from a static
`SELECT ... WHERE 1 = 1` and append `AND <allow-listed column> = ?`
only. Bind in the same order.

```cpp
Result<Row> SqliteRowRepository::create(const std::string& name,
                                       const std::string& dueOn) {
    const char* sql = "INSERT INTO Example (name, status, active, due_on) "
                      "VALUES (?, 'Open', 1, ?);";
    int rc = 0;
    auto stmt = prepareStmt(db_.handle(), sql, rc);
    if (rc != SQLITE_OK)
        return Result<Row>::err("Failed to prepare insert.");
    sqlite3_bind_text(stmt.get(), 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 2, dueOn.c_str(), -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        if (classifyConstraint(db_.handle()) == SqliteConstraint::Unique)
            return Result<Row>::err("A row with that name already exists.");
        if (classifyConstraint(db_.handle()) == SqliteConstraint::Check)
            return Result<Row>::err("Invalid status or flag.");
        return Result<Row>::err("Failed to create row.");
    }
    Row row;
    row.id = sqlite3_last_insert_rowid(db_.handle());
    row.name = name;
    return Result<Row>::ok(std::move(row));
}
```

`sqlite3_bind_int64` for integer ids. On `SQLITE_ROW`, copy text before
the next `step`. Map `ForeignKey`, `NotNull`, and `Check` whenever that
adapter can hit them.

```cpp
[[nodiscard]] std::string inPlaceholders(std::size_t n) {
    std::string sql;
    sql.reserve(n * 2);
    for (std::size_t i = 0; i < n; ++i) {
        if (i != 0) sql += ',';
        sql += '?';
    }
    return sql;
}

[[nodiscard]] std::string likeEscape(std::string_view raw) {
    std::string out;
    out.reserve(raw.size());
    for (char c : raw) {
        if (c == '\\' || c == '%' || c == '_') out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

Result<std::vector<Row>> SqliteRowRepository::findByIds(
    const std::vector<std::int64_t>& ids) {
    if (ids.empty()) return Result<std::vector<Row>>::ok({});
    const std::string sql =
        "SELECT id, name, status, active, due_on FROM Example WHERE id IN (" +
        inPlaceholders(ids.size()) + ");";
    int rc = 0;
    auto stmt = prepareStmt(db_.handle(), sql.c_str(), rc);
    if (rc != SQLITE_OK) return Result<std::vector<Row>>::err("Failed to prepare.");
    for (std::size_t i = 0; i < ids.size(); ++i)
        sqlite3_bind_int64(stmt.get(), static_cast<int>(i + 1), ids[i]);
    std::vector<Row> rows;
    while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
        Row row;
        row.id = sqlite3_column_int64(stmt.get(), 0);
        row.name = reinterpret_cast<const char*>(
            sqlite3_column_text(stmt.get(), 1));
        rows.push_back(std::move(row));
    }
    if (rc != SQLITE_DONE) return Result<std::vector<Row>>::err("Failed to query.");
    return Result<std::vector<Row>>::ok(std::move(rows));
}

Result<std::vector<Row>> SqliteRowRepository::searchName(
    const std::string& needle) {
    const char* sql =
        "SELECT id, name, status, active, due_on FROM Example "
        "WHERE name LIKE ? ESCAPE '\\' ORDER BY id ASC;";
    int rc = 0;
    auto stmt = prepareStmt(db_.handle(), sql, rc);
    if (rc != SQLITE_OK) return Result<std::vector<Row>>::err("Failed to prepare.");
    const std::string pattern = likeEscape(needle) + "%";
    sqlite3_bind_text(stmt.get(), 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
    std::vector<Row> rows;
    while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
        Row row;
        row.id = sqlite3_column_int64(stmt.get(), 0);
        row.name = reinterpret_cast<const char*>(
            sqlite3_column_text(stmt.get(), 1));
        rows.push_back(std::move(row));
    }
    if (rc != SQLITE_DONE) return Result<std::vector<Row>>::err("Failed to query.");
    return Result<std::vector<Row>>::ok(std::move(rows));
}

Result<std::vector<Row>> SqliteRowRepository::pageByDue(
    const std::string& status, const std::string& afterDue,
    std::int64_t afterId, int limit) {
    const char* sql =
        "SELECT id, name, status, active, due_on FROM Example "
        "WHERE status = ? AND (due_on < ? OR (due_on = ? AND id < ?)) "
        "ORDER BY due_on DESC, id DESC LIMIT ?;";
    int rc = 0;
    auto stmt = prepareStmt(db_.handle(), sql, rc);
    if (rc != SQLITE_OK) return Result<std::vector<Row>>::err("Failed to prepare.");
    sqlite3_bind_text(stmt.get(), 1, status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 2, afterDue.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 3, afterDue.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt.get(), 4, afterId);
    sqlite3_bind_int(stmt.get(), 5, limit);
    std::vector<Row> rows;
    while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
        Row row;
        row.id = sqlite3_column_int64(stmt.get(), 0);
        row.name = reinterpret_cast<const char*>(
            sqlite3_column_text(stmt.get(), 1));
        rows.push_back(std::move(row));
    }
    if (rc != SQLITE_DONE) return Result<std::vector<Row>>::err("Failed to query.");
    return Result<std::vector<Row>>::ok(std::move(rows));
}
```

The first page omits the keyset predicate and only binds `status` and
`LIMIT`. The second page uses the last row's `due_on` and `id`.

A loop that queries once per parent is one join, or one bound `IN` list
when the parent ids are already in memory:

```sql
SELECT c.id, c.parent_id, c.name
  FROM Child  c
  JOIN Parent p ON p.id = c.parent_id
 WHERE p.owner_id = ?
 ORDER BY c.parent_id ASC, c.id ASC;
```

# Bulk writes

Two or more changing statements, or a row loop, is one
`BEGIN IMMEDIATE`. Prepare once; `sqlite3_reset`, rebind, `step` per
row. On any failure `ROLLBACK`. `execSql` is the helper from
`cpp-sqlite-setup`. Do not prepare per row.

```cpp
Result<void> SqliteRowRepository::insertMany(
    const std::vector<Row>& rows) {
    auto begun = execSql(db_.handle(), "BEGIN IMMEDIATE;", "Bulk insert failed: ");
    if (!begun) return begun;
    const char* sql =
        "INSERT INTO Example (name, status, active, due_on) VALUES (?,?,?,?);";
    int rc = 0;
    auto stmt = prepareStmt(db_.handle(), sql, rc);
    if (rc != SQLITE_OK) {
        sqlite3_exec(db_.handle(), "ROLLBACK;", nullptr, nullptr, nullptr);
        return Result<void>::err("Bulk insert failed: prepare.");
    }
    for (const auto& row : rows) {
        sqlite3_reset(stmt.get());
        sqlite3_bind_text(stmt.get(), 1, row.name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 2, row.status.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt.get(), 3, row.active ? 1 : 0);
        sqlite3_bind_text(stmt.get(), 4, row.dueOn.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt.get());
        if (rc != SQLITE_DONE) {
            sqlite3_exec(db_.handle(), "ROLLBACK;", nullptr, nullptr, nullptr);
            if (classifyConstraint(db_.handle()) == SqliteConstraint::Check)
                return Result<void>::err("Invalid status or flag.");
            return Result<void>::err("Bulk insert failed.");
        }
    }
    auto committed = execSql(db_.handle(), "COMMIT;", "Bulk insert failed: ");
    if (!committed) {
        sqlite3_exec(db_.handle(), "ROLLBACK;", nullptr, nullptr, nullptr);
        return committed;
    }
    return Result<void>::ok();
}
```

# Authorizer

Install at the end of `configure`, after `migrate` succeeds. Deny
everything not in the switch.

```cpp
namespace {
int sqliteAuth(void* /*user*/, int action, const char* /*arg1*/,
               const char* /*arg2*/, const char* /*db*/,
               const char* /*trigger*/) {
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

Result<void> SqliteDatabase::installAuthorizer() {
    const int rc = sqlite3_set_authorizer(db_, sqliteAuth, nullptr);
    if (rc != SQLITE_OK)
        return Result<void>::err("Failed to install SQL authorizer.");
    return Result<void>::ok();
}
```

```cpp
    auto migrated = migrate();
    if (!migrated) return migrated;
    return installAuthorizer();
```

`installAuthorizer` calls `sqlite3_set_authorizer`. Repositories never
run `PRAGMA`, `ATTACH`, or DDL. Tests that must stamp `user_version` or
emit DDL call `sqlite3_set_authorizer(handle, nullptr, nullptr)` for
that fixture, then `installAuthorizer` again. After the authorizer is
on, assert tables with `SELECT sql FROM sqlite_master WHERE name = ?`,
not `PRAGMA table_info`.

# Threads

The UI connection stays on the UI thread. A worker:

```cpp
SqliteDatabase worker;
auto opened = worker.open(dbPath);  // same file; WAL already on
if (!opened) return opened;
// queries on worker.handle() only
// worker destructor close_v2s before the hop returns
```

Do not pass `sqlite3*` or `SqliteStmt` across threads. Do not lock a
mutex around one shared connection. Do not `BEGIN` on the UI connection
and finish the transaction in the worker.

# Schema docs

When a table or index is added, changed, or removed: add the migration
step and bump `kSchemaVersion`; update `docs/database/schema.md`
(`CREATE` text, column list, each index); list the page in
`docs/README.md` and `docs/AGENTS.md`.

# Tests

Hermetic: `openMemory()` from the setup skill. No host database file.
List each new `*_tests.cpp` in `tests/CMakeLists.txt`. Include
`sqlite3.h` and link `sqlite3`.

- Authorizer denies `ATTACH` (prepare or exec returns not-OK).
- A `CHECK` failure maps to `SqliteConstraint::Check` and the adapter
  `Result` error, not a substring of `sqlite3_errmsg`.
- Binding text into an `INTEGER` column fails under `STRICT`.
- An `IN` list of two ids returns those rows; an empty id list returns
  no rows and does not prepare `IN ()`.
- A keyset second page starts after the last `(sort, id)` of the first
  page; no overlap, no skip.
- A bulk insert that fails on a later row rolls back; the table is empty
  (or unchanged).
- Bind a value that contains `'; DROP TABLE` — stored as text, not
  executed.

Do not add a test that copies `-wal` / `-shm` or the database file while
open.

# C++ SQLite

Cursor Agent Skill for **schema and queries** against an embedded SQLite
database in a C++20 repository: `STRICT` tables, `CHECK` constraints,
indexes, bound prepared statements, `IN` lists, `LIKE` escaping, keyset
pagination, bulk inserts, an authorizer, and `user_version` migration
steps.

This is the human-readable companion to [`SKILL.md`](SKILL.md). Agents
follow `SKILL.md` only; every template lives in that file.

The amalgamation, RAII connection, WAL, bind helpers, and migration
**runner** live in `cpp-sqlite-setup`. This skill assumes that wiring
already exists.

## Scope

The skill is for **how** the program stores and reads rows. It does not
pin SQLite, open the file, or copy a live database. It does **not**
persist JSON documents (`cpp-json-files`) and it does **not** talk to a
client/server database.

It assumes the project's `Result` type, `SqliteDatabase`, `prepareStmt`,
and `classifyConstraint`. Tests follow the unit-test skill: unit tests,
`:memory:` databases, no host database file. Statement handles follow
the ownership skill. If the UI blocks on a long query, the UI skill owns
the worker-thread hop; this skill only requires a second connection on
that worker.

## When to apply

Use this skill when adding or changing a table, index, `CHECK`,
prepared-statement query, `IN` list, keyset page, bulk insert, migration
step SQL, or the authorizer.

Trigger phrases: `STRICT`, `CHECK`, authorizer, prepared statement,
keyset, `IN` clause, index, bulk insert, schema, migration step.

If the task is the amalgamation pin, `sqlite3_open_v2`, WAL, or the
backup API, use `cpp-sqlite-setup`. If the task is only reading or
writing a JSON **file**, use `cpp-json-files`.

## Conventions

| Area | Rule |
|---|---|
| Types | `STRICT` tables. Columns are `INTEGER`, `REAL`, `TEXT`, or `BLOB` only. |
| Closed sets | `CHECK (col IN (...))`. Booleans: `INTEGER` plus `CHECK (col IN (0, 1))`. Dates: ISO-8601 `TEXT`. |
| Ids | `INTEGER PRIMARY KEY`. `AUTOINCREMENT` only if ids must never reuse. |
| Indexes | Equality, then range, then `ORDER BY`. Name `idx_<table>_<columns>`. Do not duplicate `UNIQUE`. Skip extra indexes on a lookup loaded whole. |
| SQL | One static statement per method. Explicit columns. No `SELECT *`. No concatenated user input. |
| IN | `?,?,?` placeholders, one bind each. Empty list returns no rows; never `IN ()`. |
| LIKE | Escape `\`, `%`, `_` in C++; bind with `ESCAPE '\'`. |
| N+1 | One `JOIN` or one bound `IN` list, not a query per parent row. |
| Pages | Keyset on sort columns plus unique `id`, plus `LIMIT ?`. `OFFSET` only when asked. |
| Bulk | One `BEGIN IMMEDIATE`. Prepare once; `reset`, rebind, `step` per row. Failure rolls back. |
| Authorizer | After migrate. Allow `SELECT`/`READ`/`INSERT`/`UPDATE`/`DELETE`/`TRANSACTION`/`SAVEPOINT`/`FUNCTION`. Deny `ATTACH`, `DETACH`, `PRAGMA`, DDL. |
| Threads | UI connection on the UI thread. Worker opens its own connection to the same file. No shared `sqlite3*` behind a mutex. No `BEGIN` across a UI call. |
| Migrations | New `user_version` step in the setup runner. Never edit a shipped step. Stamp version before `COMMIT`. Rebuilds: foreign keys off before `BEGIN`, `foreign_key_check` before `COMMIT`. |
| Errors | `sqlite3_extended_errcode` via `classifyConstraint`. Do not search English `errmsg`. |
| Docs | `docs/database/schema.md` (CREATE, columns, each index) whenever a table changes. |
| Tests | `:memory:` only. Authorizer denies `ATTACH`. CHECK, STRICT, IN, keyset, bulk rollback, injection string stored as text. |
| Git (agents) | No commit, push, or branch-switch-to-edit unless the user explicitly asked. |

Placeholders in `SKILL.md`:

- `PROJ` — `project()` name and target stem
- `NS` — C++ namespace

Agents must substitute real names. Do not leave tokens in the project.

## Why STRICT and CHECK

Without `STRICT`, SQLite's type affinity will store `'x'` in an
`INTEGER` column. A later `sqlite3_column_int64` then returns `0`, and
the bad write is silent. `STRICT` rejects that write.

Application validation still runs first so the user sees a domain error.
`CHECK` is the backstop when a caller skips that path, including a bulk
import. Closed sets (status strings, booleans stored as 0/1) belong in
`CHECK (col IN (...))` so the database, not only the UI, refuses them.

## Why the authorizer

Bound parameters stop a value from becoming SQL. They do not stop a
compromised caller from preparing `ATTACH` or a writable `PRAGMA`. The
authorizer is a second gate on the connection: after migrate, only
row-level `SELECT` / `INSERT` / `UPDATE` / `DELETE` (and the reads,
transactions, savepoints, and functions those statements need) are
allowed. `ATTACH`, `DETACH`, `PRAGMA`, and DDL are denied.

Migrate and the open-path PRAGMAs (foreign keys, WAL) run **before** the
authorizer is installed. The backup API copies pages through SQLite; it
does not go through repository SQL, so it is not blocked.

## Why keyset pages, not OFFSET

`OFFSET n` still walks `n` rows and throws them away. As the list grows,
each next page gets slower, and a concurrent insert can skip or repeat a
row. A keyset (`WHERE (due_on, id) < (?, ?)` written as
`due_on < ? OR (due_on = ? AND id < ?)`) starts at the last row of the
previous page and uses the same index as the `ORDER BY`.

`OFFSET` remains only when the task asks for a small, fixed page index
(for example "page 3 of 5" on a short lookup).

## Why one transaction around a bulk write

Each auto-commit `INSERT` is its own transaction: fsync, lock, repeat.
A loop of a thousand rows then pays a thousand commits, and a failure
halfway leaves a partial import. `BEGIN IMMEDIATE` takes the write lock
once. Prepare the statement once; `sqlite3_reset` and rebind per row.
Any failure `ROLLBACK`s so the table is unchanged.

## Why one connection per thread

A `sqlite3*` and its statements are not safe to use from two threads at
once. Putting a mutex around the UI connection still means the UI waits
on the worker, and a `BEGIN` that spans a UI call can block paints or
deadlock. WAL already allows two connections to the same file: the UI
keeps its connection; the worker opens another, uses it, and closes it
before the worker ends. The hop itself (posting back to the UI thread)
is the ownership skill.

## Files

| File | Audience |
|---|---|
| [`SKILL.md`](SKILL.md) | Agent procedure and all examples |
| [`README.md`](README.md) | This human explanation |

## Using the skill in another repo

This copy lives only in this skills repository
(`.cursor/skills/cpp-sqlite/`). Cursor loads **project** skills from
the repo you have open, and **personal** skills from `~/.cursor/skills/`.

To use it while working in other C++ projects, copy the directory
(and `cpp-sqlite-setup` if that wiring is not already in the project):

```text
.cursor/skills/cpp-sqlite/  →  ~/.cursor/skills/cpp-sqlite/
```

On Windows the personal skills directory is typically
`C:\Users\<you>\.cursor\skills\`. Do not copy it into
`~/.cursor/skills-cursor/` (that tree is reserved for Cursor's built-in
skills).

After copying, a later agent session can pick the skill up from the
description (`STRICT`, `CHECK`, authorizer, prepared statement, keyset)
or you can mention it by name.

# C++ SQLite Setup

Cursor Agent Skill for **embedding SQLite** in a C++20 CMake repository:
the official amalgamation compiled as a static library, a RAII connection,
a `user_version` migration runner, WAL, bound parameters, extended result
codes, and copies through the backup API.

This is the human-readable companion to [`SKILL.md`](SKILL.md). Agents
follow `SKILL.md` only; every template lives in that file.

Table shape, indexes, `CHECK` / `STRICT`, query generation, the
authorizer, and pagination are a separate skill: `cpp-sqlite`.

## Scope

The skill is for an embedded **SQL database file**. The program owns the
schema and the connection. It does **not** persist JSON documents (that
is `cpp-json-files`), and it does **not** talk to a client/server database.

It assumes the `cpp-project-setup` layout (`core/`, `app/`,
`cmake/Dependencies.cmake`) and the project's `Result` type. Tests follow
`cpp-unit-testing`: unit tests, `:memory:` databases, no host database
file. Statement handles follow `cpp-code-style-security` (`unique_ptr`
plus a finalize deleter). If the UI blocks on a long query, the UI skill
owns the worker-thread / UI-thread-post pattern; this skill only points
at it.

It does **not** re-scaffold the repo, add MSVC, or introduce a second
SQLite or ORM.

## When to apply

Use this skill when adding or changing the amalgamation pin, the
connection wrapper, the migration runner, WAL, bind helpers, constraint
mapping, or a live backup.

Trigger phrases: amalgamation, `sqlite3_open_v2`, `IDatabaseSession`,
WAL, backup API, `user_version` runner.

If the task is table shape, indexes, `CHECK`, prepared-statement query
generation, bulk writes, the authorizer, or pagination, use `cpp-sqlite`.
If the task is only reading or writing a JSON **file**, use
`cpp-json-files`.

## Conventions

| Area | Rule |
|---|---|
| Library | Official amalgamation **3.53.4**. FetchContent URL + SHA3-256. Populate; do not `MakeAvailable` (no `CMakeLists.txt` in the zip). |
| Compile | `SQLITE_THREADSAFE=1`, `SQLITE_OMIT_LOAD_EXTENSION=1`, `SQLITE_DQS=0`, `SQLITE_DEFAULT_FOREIGN_KEYS=1`. `-w` on that C file. UNIX: pthreads + `dl`. |
| Link | `sqlite3` PRIVATE on the logic target. Tests that include `sqlite3.h` link `sqlite3` themselves. `project()` lists `C`. |
| Layering | `sqlite3.h` only in infra `.cpp` (and adapter tests). Connection header forward-declares `struct sqlite3`. UI target does not include it. |
| Port | `IDatabaseSession::open(path)` returns `Result`. Repositories speak domain types. Composition root owns one connection. |
| File | Filesystem port creates the directory; then `{dataDir}/PROJ.db`. Path as `generic_string()`. |
| Open | `sqlite3_open_v2` (`READWRITE \| CREATE`). `close_v2` on failure and in the destructor. No `SQLITE_OPEN_NOMUTEX`. |
| After open | Extended result codes, busy timeout **5000** ms, `foreign_keys ON`, then WAL + `synchronous=NORMAL` on files only. Then migrate. |
| Migrations | `PRAGMA user_version` runner. One function per step, `BEGIN IMMEDIATE` / `COMMIT`, stamp version before commit. Newer than this build is an error. Step SQL is `cpp-sqlite`. |
| Rebuilds | `PRAGMA foreign_keys` is a no-op inside a transaction. Turn it off **before** `BEGIN`, `foreign_key_check` before `COMMIT`, on again after. |
| SQL | Bound parameters. No concatenated user input. Copy column text before the next step. |
| Errors | `sqlite3_extended_errcode` (`UNIQUE`, `FOREIGNKEY`, `NOTNULL`, `CHECK`). Do not search English `errmsg`. |
| Transactions | Any change that uses more than one statement is one `BEGIN IMMEDIATE`. |
| Backup | Live copy only through `sqlite3_backup_*`. Closed, checkpointed file may be copied as one file. |
| Schema | `INTEGER PRIMARY KEY`; `AUTOINCREMENT` only if ids must never reuse. No `ON DELETE CASCADE` unless asked. `COLLATE NOCASE` is ASCII-only. STRICT / CHECK / indexes: `cpp-sqlite`. |
| Docs | `docs/database/schema.md` plus the docs index, whenever a table changes. |
| Tests | `:memory:` only. Upgrade from the previous `user_version`. One case per mapped constraint. Backup round-trip. |
| Git (agents) | No commit, push, or branch-switch-to-edit unless the user explicitly asked. |

Placeholders in `SKILL.md`:

- `PROJ` — `project()` name and target stem
- `PREFIX` — CMake option prefix
- `NS` — C++ namespace

Agents must substitute real names. Do not leave tokens in the project.

## Why the amalgamation, not a system library

A system `libsqlite3` varies by distro and by the Windows runtime the
user already has. Compile options (`THREADSAFE`, `OMIT_LOAD_EXTENSION`,
`DQS`, `DEFAULT_FOREIGN_KEYS`) would not match. Fetching the official
`sqlite3.c` / `sqlite3.h` zip and compiling it as `sqlite3` gives every
builder the same engine without a package install.

The zip has no `CMakeLists.txt`. CMake 3.22's `FetchContent_MakeAvailable`
always calls `add_subdirectory` and would fail, so the skill populates
the content and declares the static library itself.

## Why `user_version`, not `CREATE TABLE IF NOT EXISTS` on every open

`CREATE TABLE IF NOT EXISTS` is a no-op once the table exists. Adding a
column, tightening a unique key, or rebuilding a table never runs. The
file then silently stays on the old shape.

`PRAGMA user_version` is an integer SQLite stores in the database header.
This build knows `kSchemaVersion`. On open it runs each missing step
exactly once, inside a transaction, and stamps the version before
commit. A file from a newer build is refused instead of being opened
half-understood.

A table rebuild that must drop foreign keys cannot toggle
`PRAGMA foreign_keys` inside that transaction — SQLite ignores the
pragma until commit. The step turns enforcement off first, rebuilds,
checks, commits, then turns it back on.

## Why extended codes, WAL, and the backup API

`sqlite3_errmsg` is English text. Searching it for `"UNIQUE constraint"`
breaks when the message changes and cannot tell unique from foreign-key
from not-null. Extended result codes are stable integers
(`SQLITE_CONSTRAINT_UNIQUE` and the rest) once enabled on the connection.

WAL lets readers proceed while a writer is in a transaction. The busy
timeout (5 s) waits out a short lock instead of failing immediately.
`:memory:` databases skip WAL; it is a file-journal mode.

Copying the `.db` file (or its `-wal` / `-shm` siblings) while a
connection is open can capture a torn snapshot. `sqlite3_backup_init` /
`step` / `finish` copies a consistent image through SQLite. After the
only connection closes, WAL is checkpointed and the single file may be
copied.

## Why bind everything and fold Unicode in application code

A value that originated outside the process — user text, a file, a
network body — is a bound parameter. Concatenating it into SQL is how
injection happens. `SQLITE_TRANSIENT` tells SQLite to copy the bytes so
the `std::string` may die before the statement is reset.

`COLLATE NOCASE` is ASCII `A-Z` / `a-z` only. Names that must compare
equal across Unicode case (German `ß`, Turkish `I`) have to be folded in
application code before they are written or compared. Do not assume the
collation does that.

`SQLITE_DEFAULT_FOREIGN_KEYS=1` makes a new connection enforce foreign
keys even if a caller forgets the pragma. The open path still sets
`PRAGMA foreign_keys = ON` so an existing connection cannot drift off.

## Files

| File | Audience |
|---|---|
| [`SKILL.md`](SKILL.md) | Agent procedure and all examples |
| [`README.md`](README.md) | This human explanation |

## Using the skill in another repo

This copy lives only in this skills repository
(`.cursor/skills/cpp-sqlite-setup/`). Cursor loads **project** skills from
the repo you have open, and **personal** skills from `~/.cursor/skills/`.

To use it while working in other C++ projects, copy the directory:

```text
.cursor/skills/cpp-sqlite-setup/  →  ~/.cursor/skills/cpp-sqlite-setup/
```

On Windows the personal skills directory is typically
`C:\Users\<you>\.cursor\skills\`. Do not copy it into
`~/.cursor/skills-cursor/` (that tree is reserved for Cursor's built-in
skills).

After copying, a later agent session can pick the skill up from the
description (amalgamation, `sqlite3_open_v2`, WAL, `IDatabaseSession`)
or you can mention it by name.

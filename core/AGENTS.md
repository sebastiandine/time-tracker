# core/AGENTS.md

`time_tracker_core` — logic library. **No Qt headers.** Read the root `AGENTS.md` first.

## Layer pointers

- `include/timetracker/domain/Records.hpp` — catalog, time entry, and statistics records.
- `include/timetracker/ports/` — clock, environment, filesystem, database session, repositories.
- `include/timetracker/services/` — catalog, tracking, statistics, settings.
- `include/timetracker/infra/` — SQLite connection and repositories. `sqlite3.h` is included only from `src/infra/`.
- `include/timetracker/time/` — UTC formatting and local statistics windows.
- `include/timetracker/paths/DataPaths.hpp` — per-user directory and database path.
- `include/timetracker/config/ConfigService.hpp` — data-directory cascade. The loader does not open the database.
- `include/timetracker/services/DataLocationService.hpp` — menu choice that reopens `time_tracker.db`.
- `include/timetracker/json/DocumentIo.hpp` — `loadJson` / `storeJson`. Not for UI headers.
- `include/timetracker/infra/HostTextFile.hpp` — durable text replace used by the process filesystem.
- `src/text/TextRules.hpp` — name and note limits. Not a public header.
- `src/infra/SqlUtil.hpp` — statement helpers. Includes `sqlite3.h`. Infra `.cpp` files only.

## Conventions

1. Public headers under `include/timetracker/`. Implementations under `src/`.
2. This target links `time_tracker_warnings` `PRIVATE` and `sqlite3` `PRIVATE`.
3. No Qt includes. Verify with a search of `core/`.
4. Add each new `.cpp` to `CMakeLists.txt`. No glob.
5. One open `TimeEntry`. Schema changes are a new `user_version` step in `SqliteDatabase.cpp`. Do not edit a shipped step.
6. After a schema or timer-rule change, update `docs/database/schema.md` or `docs/time-tracking.md`.

## Commands

`cmake --build build --target time_tracker_core`

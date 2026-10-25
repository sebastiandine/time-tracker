# tests/AGENTS.md

`time_tracker_core_tests` — doctest for `time_tracker_core`. Hermetic, no real network. Read the root `AGENTS.md` first.

## File pointers

- `main.cpp` — `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN`. Do not put tests here.
- `CMakeLists.txt` — explicit `.cpp` list (no glob).
- `support/Fixture.hpp` — in-memory database and services.
- `support/FakeClock.hpp` — clock that moves only when a test says so.
- `support/TempDir.hpp` — unique temporary directory, removed on destruction.
- `catalog_tests.cpp` — names, notes, and blocked deletes.
- `tracking_tests.cpp` — start, stop, downtime, and history pages.
- `stats_tests.cpp` — rollup, clipping, and category filter.
- `stats_range_tests.cpp` — local day, week, and month windows.
- `settings_tests.cpp` — theme values.
- `database_tests.cpp` — schema, authorizer, backup, and constraints.
- `data_paths_tests.cpp` — per-user directory confinement.
- `data_location_tests.cpp` — switching the database directory.
- `config_tests.cpp` — configuration cascade and data-directory overrides.
- `json_tests.cpp` — JSON documents and unknown-key handling.
- `text_file_tests.cpp` — durable text replace.
- `time_format_tests.cpp` — UTC text and duration labels.

## Conventions

1. doctest. `#include <doctest/doctest.h>`, `TEST_CASE`. `CHECK`, `REQUIRE`.
2. Repository tests call `openMemory()`. A unique temp directory is only for path and file-open cases.
3. Single-file fakes stay in that file. Shared fakes live in `tests/support/`.
4. Names describe behavior, not implementation.
5. Add each `.cpp` to `tests/CMakeLists.txt`. No glob.
6. Do not link `time_tracker_ui`.

## Commands

- Build: `cmake --build build --target time_tracker_core_tests`
- All: `ctest --test-dir build --output-on-failure`
- One case: `./build/bin/time_tracker_core_tests --test-case="*pattern*"`

## Anti-patterns

- Don't depend on the UI target. Don't hard-code host paths. Don't require network.

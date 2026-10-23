# AGENTS.md

Desktop time tracker: categories, main topics, and subtopics, with one timer that survives shutdown.

## Project structure

- `core/` — `time_tracker_core` static library. Logic and SQLite. No Qt. See `core/AGENTS.md`.
- `ui/` — `time_tracker_ui` Qt Widgets. See `ui/AGENTS.md`.
- `app/` — `time-tracker` executable (composition root). See `app/AGENTS.md`.
- `tests/` — `time_tracker_core_tests` doctest binary. Hermetic tests. See `tests/AGENTS.md`.
- `docs/` — contributor documentation. See `docs/AGENTS.md`. Timer rules: `docs/time-tracking.md`. Schema: `docs/database/schema.md`.
- `.github/workflows/` — GitHub Actions. See `.github/workflows/AGENTS.md`.
- `scripts/` — CI helper scripts. See `scripts/AGENTS.md`.
- `cmake/` — `Toolchain.cmake` (Clang first, MinGW-w64 fallback), `Dependencies.cmake` (FetchContent pins), `CompilerWarnings.cmake` (`time_tracker_warnings` interface target).
- `CMakeLists.txt` — `TIME_TRACKER_BUILD_TESTS` (default ON), `TIME_TRACKER_BUILD_APP` (default ON), `TIME_TRACKER_APP_VERSION` (default `${PROJECT_VERSION} (localbuild)`).

## Architecture rules (do not break)

1. Dependencies point inward: `app` -> `ui` -> `core`. `core` depends on no other first-party target.
2. Logic targets must not include Qt headers. `sqlite3.h` is included only from `core/src/infra/` and from tests that talk to SQLite directly.
3. One directory per first-party target. Explicit source lists; no `GLOB`.
4. Public headers live under `core/include/timetracker/`. Implementations live under `core/src/`.
5. At most one time entry is running. Start writes `started_at` immediately. Closing the app does not stop that entry.
6. Deletes do not cascade. A parent with children, or a topic with time entries, is refused.
7. Do not link `time_tracker_warnings` on `ui` or `app`. Qt headers are noisy under those flags.

## Toolchain

- **Compilers**: Clang 14+ preferred, MinGW-w64 GCC 11+ fallback on Windows. **Do not** add MSVC support.
- **C++ standard**: C++20 (`CMAKE_CXX_STANDARD 20`, `CXX_EXTENSIONS OFF`).
- **Build system**: CMake 3.22+ with `FetchContent`. Pin every dep by tag in `cmake/Dependencies.cmake`; never use `master`.
- **Desktop UI**: Qt 6.5+ Widgets, shared libraries, found with `find_package`. The Windows kit is MSYS2 UCRT64. Windows CI installs that kit, builds the app, and publishes a portable zip plus an NSIS installer. Linux CI configures with `TIME_TRACKER_BUILD_APP=OFF`.

## Key dependencies

| Library | Version | Purpose |
|---|---|---|
| doctest | v2.4.11 | Tests (only when `TIME_TRACKER_BUILD_TESTS=ON`) |
| SQLite amalgamation | 3.53.4 | Embedded database (public domain) |
| Qt Widgets | 6.5+ | Desktop UI when `TIME_TRACKER_BUILD_APP=ON` (LGPL, linked dynamically) |

## Commands

Run from the workspace root.

- Configure: `cmake --preset ninja` (MinGW: `cmake --preset mingw`)
- Build: `cmake --build --preset ninja`
- Run: `./build/bin/time-tracker` (`.\build\bin\time-tracker.exe` on Windows)
- Tests: `ctest --test-dir build --output-on-failure`
- Tests only: `cmake --build build --target time_tracker_core_tests`

Windows: linking fails with `Permission denied` if the exe is still running.
After configure, copy `build/compile_commands.json` to the repo root for clangd.

## Required follow-ups

- After adding a `.cpp` you **must** add it to that package's `CMakeLists.txt`. No glob.
- After adding a dependency you **must** verify license compatibility before merging.
- For new logic, add or update tests. Exercise both outcomes of meaningful conditionals.
- After adding a file under `docs/` you **must** list it in `docs/README.md` and `docs/AGENTS.md`.
- Qt headers that use `Q_OBJECT` must be listed in `ui/CMakeLists.txt` so AUTOMOC sees them.

## Agent collaboration (Cursor / AI)

- **Never** `git commit` or `git push` unless the user **explicitly** asked you to commit and/or push (e.g. "commit this", "push to origin"). Preparing diffs and suggesting commands is fine; performing those Git writes without explicit instruction is not.
- **Never** check out another branch **to change it** unless the user **explicitly** asked you to work on that branch. Temporarily checking out another branch **read-only** (inspect history, compare files, run `git show`) is fine without asking; switch back to the working branch before making edits unless instructed otherwise.

## Anti-patterns

- Don't include Qt headers from `core/`.
- Don't include `sqlite3.h` outside `core/src/infra/` and the tests that need it.
- Don't add tests that hit real network or a developer's database file. A unique temp directory with RAII cleanup is allowed for path and file-open tests. Repository tests use `:memory:`.
- Don't enable `-Wconversion` / `-Wsign-conversion`.
- Don't use `master` for FetchContent tags.
- Don't create multiple top-level triggers for the same CI intent. One orchestrator; `workflow_call` for OS splits.

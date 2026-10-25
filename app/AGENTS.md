# app/AGENTS.md

The `time-tracker` executable — composition root. Read the root `AGENTS.md` first.

## File pointers

- `main.cpp` — creates the process adapters, loads configuration, opens the database, and shows `MainWindow`. No business logic.
- `ProcessEnvironment.cpp` — `getenv` adapter, including the variable list used by configuration.
- `ProcessFilesystem.cpp` — creates directories and reads or replaces text files.
- `SystemClock.cpp` — operating-system UTC clock.
- `CMakeLists.txt` — links `time_tracker_ui`, `time_tracker_core`, and `sqlite3`. Does not link `time_tracker_warnings`.

## Conventions

1. Keep this target thin. More than construction and wiring belongs in `core/` or `ui/`.
2. Long-lived services are `std::unique_ptr` members of `main`, created before the window so they outlive it.
3. Do not include `sqlite3.h`. Adapter headers from `core/include/timetracker/infra/` are allowed.
4. Windows uses the GUI subsystem. POST_BUILD copies the MinGW runtime and runs `windeployqt` or `windeployqt6` when one is on PATH. CI packages that bundle; see `docs/windows-installer.md`.

## Commands

- Build: `cmake --build build --target time_tracker`
- Run: `./build/bin/time-tracker` (`.\build\bin\time-tracker.exe` on Windows)

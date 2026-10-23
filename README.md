# Time Tracker

Desktop app for logging time against categories, main topics, and subtopics. One timer can be running. Its start time is stored immediately, so the recorded time continues through an app restart or a shutdown. Data lives in a per-user SQLite file. The window has light and dark themes.

## Technical Overview

C++20 project built with CMake.

- `core/`: library (`time_tracker_core`)
- `ui/`: Qt Widgets (`time_tracker_ui`)
- `app/`: executable composition root (`time-tracker`)

On Windows the database is `%LOCALAPPDATA%\TimeTracker\time_tracker.db`. Elsewhere it is `$XDG_DATA_HOME/TimeTracker/time_tracker.db`, or `~/.local/share/TimeTracker/time_tracker.db`.

## Quick Start

### Prerequisites

- CMake 3.22+
- Clang 14+ (preferred) or MinGW-w64 GCC 11+
- Ninja (recommended) or Make
- Qt 6.5+ Widgets to build the desktop app. On Windows use the MSYS2 UCRT64 package `mingw-w64-ucrt-x86_64-qt6-base`. MSVC Qt kits are not supported.

Pass `-DTIME_TRACKER_BUILD_APP=OFF` to build the library and tests without Qt.

### Build

```bash
cmake --preset ninja
cmake --build --preset ninja
```

### Test

```bash
ctest --preset ninja
```

### Run

```bash
./build/bin/time-tracker
```

On Windows: `.\build\bin\time-tracker.exe`

## Documentation

Contributor docs live in [`docs/`](docs/README.md). Start with
[Intro For New Developers](docs/intro-to-new-developers.md) and
[Building Locally](docs/building-locally.md). How the timer behaves is in
[Time tracking](docs/time-tracking.md).


## License

This project is licensed under the [MIT License](LICENSE). Qt is linked as a shared library (LGPL). SQLite is public domain.

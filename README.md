# Time Tracker

Desktop app for logging time against categories, main topics, and subtopics. One timer can be running. Its start time is stored immediately, so the recorded time continues through an app restart or a shutdown. Data lives in a per-user SQLite file. The window has light and dark themes.

## Technical Overview

C++20 project built with CMake.

- `core/`: library (`time_tracker_core`)
- `ui/`: Qt Widgets (`time_tracker_ui`)
- `app/`: executable composition root (`time-tracker`)

The database file is `time_tracker.db`. By default it lives in `%LOCALAPPDATA%\TimeTracker` on Windows and in `$XDG_DATA_HOME/TimeTracker` (or `~/.local/share/TimeTracker`) elsewhere. **Data → Select Data Location...** chooses another folder. A missing file there is created empty: no categories, topics, or time entries. See [Configuration](#configuration).

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

## Windows download

A merged pull request to `master` publishes a GitHub release with `time-tracker-windows-<version>.zip` (portable exe and DLLs) and `time-tracker-windows-installer-<version>.exe`. Feature-branch runs upload the same artifacts on the Actions run. See [Windows installer](docs/windows-installer.md).

## Documentation

Contributor docs live in [`docs/`](docs/README.md). Start with
[Intro For New Developers](docs/intro-to-new-developers.md) and
[Building Locally](docs/building-locally.md). How the timer behaves is in
[Time tracking](docs/time-tracking.md).


## Configuration

Precedence (low to high): compiled defaults, then `config.json` in the
system directory, the user directory, and the executable directory, then
`.env` in that same directory order, then process environment variables,
then command-line flags.

| Directory | Windows | POSIX |
|---|---|---|
| System | `%ProgramData%\Time Tracker` | `/etc/time-tracker` |
| User | `%APPDATA%\Time Tracker` | `$XDG_CONFIG_HOME` or `~/.config/time-tracker` |
| Exe | directory of the executable | directory of the executable |

A missing file is skipped. If no `config.json` exists, the app creates one
from defaults next to the executable, or in the user directory when the
executable directory is not writable. `.env` is never auto-created.

The Data menu writes `config.json` only. A value set by `.env`, the process
environment, or the command line stays in effect, and the dialog names that
layer. The database file inside the chosen directory is always `time_tracker.db`.

| Key | Type | Default | Required | Env | Flag | Description |
|---|---|---|---|---|---|---|
| dataDirectory | string | per-user Time Tracker directory | no | TIME_TRACKER_DATA_DIRECTORY | --data-directory | Directory that contains `time_tracker.db` |

`dataDirectory` is an absolute path. A relative path fails startup.

## License

This project is licensed under the [MIT License](LICENSE). Qt is linked as a shared library (LGPL). SQLite is public domain. nlohmann/json is MIT.

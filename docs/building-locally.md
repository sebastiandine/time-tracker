#documentation #build #cmake

# Building Locally

How to build on Windows and Linux. Architecture:
[Intro For New Developers](intro-to-new-developers.md).

**Quick Setup:** configure, build, run tests from the same build directory.

## Build Model

- C++20
- library `time_tracker_core`
- Qt library `time_tracker_ui` and executable `time-tracker` when `TIME_TRACKER_BUILD_APP=ON`
- tests `time_tracker_core_tests`

## Dependency Model

`FetchContent` in `cmake/Dependencies.cmake`. Pin by tag or content hash. First configure
clones into `build/_deps`; later configures reuse that cache
(`FETCHCONTENT_UPDATES_DISCONNECTED`). Pinned at setup: doctest `v2.4.11`
(when tests are enabled) and the SQLite 3.53.4 amalgamation.

Qt is not fetched. `find_package(Qt6 6.5 COMPONENTS Widgets)` runs only when
the app is enabled. The search also looks next to the C++ compiler, which
matches an MSYS2 UCRT64 layout.

## Prerequisites

Windows recommended for the app: MSYS2 UCRT64 with
`mingw-w64-ucrt-x86_64-gcc`, `mingw-w64-ucrt-x86_64-cmake`,
`mingw-w64-ucrt-x86_64-ninja`, and `mingw-w64-ucrt-x86_64-qt6-base`.
Clang 14+ can build the library. MSVC is not supported, and MSVC Qt kits
cannot be linked.

Library and tests without Qt:

```bash
cmake -S . -B build -G Ninja -DTIME_TRACKER_BUILD_APP=OFF
```

Linux: CMake 3.22+, Clang (preferred) or GCC with C++20, Ninja recommended,
and `qt6-base-dev` (or the distro equivalent) when building the app.

## Build Commands

From repository root.

Ninja:

```bash
cmake --preset ninja
cmake --build --preset ninja
```

Windows MinGW:

```bash
cmake --preset mingw
cmake --build --preset mingw
```

Tests: `ctest --preset ninja` or `ctest --test-dir build --output-on-failure`.

The executable is `build/bin/time-tracker` (`time-tracker.exe` on Windows).
A post-build step copies the MinGW runtime DLLs and runs `windeployqt` so
the exe can start from Explorer.

## clangd

```bash
cmake -E copy_if_different build/compile_commands.json compile_commands.json
```

The root copy is gitignored.

## Troubleshooting

- First configure is slower (FetchContent clones).
- `Permission denied` linking an `.exe`: the process is still running.
- MinGW Explorer launches need `libstdc++-6.dll`, `libgcc_s_seh-1.dll`,
  `libwinpthread-1.dll` next to the exe (POST_BUILD copies them) or on `PATH`.
- Qt missing: install the UCRT64 Qt package, or configure with
  `-DTIME_TRACKER_BUILD_APP=OFF`.
- Force a compiler with `-DCMAKE_CXX_COMPILER=...` (Toolchain.cmake respects it).

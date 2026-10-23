---
name: cpp-project-setup
description: >-
  Scaffolds a C++20 CMake repository: Clang/MinGW toolchain, FetchContent,
  contributor docs, nested AGENTS.md, and a GitHub Actions CI skeleton. Use when
  starting a new C++ project, setting up CMake, writing initial AGENTS.md,
  contributor documentation, or a CI skeleton.
---

# C++ Project Setup

Setup for a general C++20 CMake repo. Do not assume a UI toolkit, a
desktop app, or sample domain code.

Substitute `PROJ` (cmake project / target stem), `PREFIX` (option prefix),
and `NS` (C++ namespace). Do not leave those tokens in the project.

Do **not** add MSVC. Do **not** search `clang-cl`. Do **not** invent a UI,
installer, dummy domain types, or dummy tests.

If the user did not give identity, infer from the repo name, confirm in one
short sentence, then proceed. Skip a step when the files already exist and
match these rules. Fill gaps; do not rewrite a healthy tree.

```
Setup progress:
- [ ] Identity (PROJ, PREFIX, NS, library and/or executable, binary name)
- [ ] CMake tree, presets, format, git metadata, LICENSE
- [ ] Root compile_commands.json after a successful configure
- [ ] Root and nested AGENTS.md
- [ ] docs/ index and starter guides
- [ ] CI skeleton + version scripts
- [ ] Stop
```

## Invariants

- CMake 3.22+, C++20, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF`
- Clang first; MinGW-w64 GCC on Windows if Clang is missing; GCC on Unix if
  Clang is missing. Honor an explicit `CMAKE_CXX_COMPILER`.
- FetchContent: pin `GIT_TAG` (never `master`), `GIT_SHALLOW TRUE`,
  `FETCHCONTENT_UPDATES_DISCONNECTED ON`, `CMAKE_POLICY_VERSION_MINIMUM 3.5`
- One directory per first-party target. Explicit sources; no `GLOB`.
- Public headers: `core/include/NS/`. Implementations: `core/src/` (mirror
  the include tree). Optional `app/` for an executable.
- If a UI toolkit exists, it is a separate target. Logic targets must not
  include its headers. Do not link `PROJ_warnings` on a UI target whose
  toolkit headers are noisy under `-Wpedantic` / `-Wshadow`.
- Warnings: the `PROJ_warnings` INTERFACE flags. No `-Wconversion` / `-Wsign-conversion`.
- Tests: `PREFIX_BUILD_TESTS` default ON, doctest v2.4.11 fetched only then,
  hermetic (no network; no host paths; real disk only with unique temp dir
  + RAII cleanup)
- Artifacts: `build/bin`, `build/lib`. Default single-config type `Release`.
- New docs listed in both `docs/README.md` and `docs/AGENTS.md`. Build guide
  filename is `docs/building-locally.md`.
- LICENSE is MIT unless the user named another. Check new dep licenses
  against it.

Layout:

```
cmake/
core/                 # PROJ_core; omit if executable-only
  include/NS/
  src/
app/                  # omit if library-only
tests/
docs/
scripts/
.github/workflows/
```

If `core/` has no `.cpp` yet, declare an `INTERFACE` library and switch to
`STATIC` when the first translation unit is added. Do not add dummy sources.
Add `tests/main.cpp` as the doctest harness only (no `TEST_CASE`).

# CMake and repo metadata

## `cmake/Toolchain.cmake`

Include **before** `project()`. Search `clang` / `clang++` only.

```cmake
# Preferred compiler is Clang. On Windows fall back to MinGW-w64 g++.
# MSVC is not supported. Run BEFORE project().

if(DEFINED CMAKE_CXX_COMPILER)
    return()
endif()

find_program(_TC_CLANG NAMES clang)
find_program(_TC_CLANGXX NAMES clang++)

if(_TC_CLANG AND _TC_CLANGXX)
    set(CMAKE_C_COMPILER   "${_TC_CLANG}"   CACHE FILEPATH "C compiler"   FORCE)
    set(CMAKE_CXX_COMPILER "${_TC_CLANGXX}" CACHE FILEPATH "C++ compiler" FORCE)
    message(STATUS "[PROJ] Toolchain: Clang at ${_TC_CLANGXX}")
    return()
endif()

if(WIN32)
    find_program(_TC_GCC NAMES x86_64-w64-mingw32-gcc gcc)
    find_program(_TC_GXX NAMES x86_64-w64-mingw32-g++ g++)
    if(_TC_GCC AND _TC_GXX)
        set(CMAKE_C_COMPILER   "${_TC_GCC}" CACHE FILEPATH "C compiler"   FORCE)
        set(CMAKE_CXX_COMPILER "${_TC_GXX}" CACHE FILEPATH "C++ compiler" FORCE)
        message(STATUS "[PROJ] Toolchain: MinGW-w64 GCC at ${_TC_GXX}")
        return()
    endif()
    message(WARNING
        "[PROJ] Neither Clang nor MinGW-w64 GCC was found in PATH. "
        "Install LLVM (preferred) or MSYS2 mingw-w64-ucrt-x86_64-gcc and re-run."
    )
else()
    find_program(_TC_GCC NAMES gcc cc)
    find_program(_TC_GXX NAMES g++ c++)
    if(_TC_GCC AND _TC_GXX)
        set(CMAKE_C_COMPILER   "${_TC_GCC}" CACHE FILEPATH "C compiler"   FORCE)
        set(CMAKE_CXX_COMPILER "${_TC_GXX}" CACHE FILEPATH "C++ compiler" FORCE)
        message(STATUS "[PROJ] Toolchain: GCC at ${_TC_GXX}")
        return()
    endif()
    message(WARNING "[PROJ] Neither Clang nor GCC was found in PATH.")
endif()
```

## `cmake/CompilerWarnings.cmake`

No MSVC flags.

```cmake
add_library(PROJ_warnings INTERFACE)

set(_PROJ_GCC_CLANG_FLAGS
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wnon-virtual-dtor
    -Wcast-align
    -Wunused
    -Woverloaded-virtual
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
    # -Wconversion / -Wsign-conversion are intentionally NOT enabled.
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(PROJ_warnings INTERFACE ${_PROJ_GCC_CLANG_FLAGS})
endif()
```

## `cmake/Dependencies.cmake`

Pin every dep by tag. Never `master`. Disable upstream tests/install.
C libraries with no CMake package: `URL` + `URL_HASH`, build a static target,
suppress warnings with `-w` on that target only.

Adding a later dependency: pin tag or hash, disable tests/examples/install,
`FetchContent_MakeAvailable`, link from the smallest target (`PRIVATE`
unless a public header needs it), update the root `AGENTS.md` table, re-check
license.

```cmake
include(FetchContent)

set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)

if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

if(PREFIX_BUILD_TESTS)
    message(STATUS "[PROJ] Fetching doctest ...")
    FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG        v2.4.11
        GIT_SHALLOW    TRUE
    )
    set(DOCTEST_WITH_TESTS OFF CACHE INTERNAL "")
    set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE INTERNAL "")
    FetchContent_MakeAvailable(doctest)
endif()
```

## Top-level `CMakeLists.txt`

`LANGUAGES CXX` only. Enable `C` later if a C amalgamation needs it. Do not
enable `RC` or `WIN32_EXECUTABLE` unless the project has a Windows resource
or a GUI subsystem. Omit `PREFIX_APP_VERSION` for a library-only repo.

```cmake
cmake_minimum_required(VERSION 3.22)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Toolchain.cmake)

project(PROJ
    VERSION 0.1.0
    DESCRIPTION "..."
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

option(PREFIX_BUILD_TESTS "Build unit tests" ON)
set(PREFIX_APP_VERSION "${PROJECT_VERSION} (localbuild)" CACHE STRING
    "Version string embedded in the build")

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilerWarnings.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Dependencies.cmake)

add_subdirectory(core)
# add_subdirectory(app)

if(PREFIX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "")
message(STATUS "============================================================")
message(STATUS "  ${PROJECT_NAME} ${PROJECT_VERSION}")
message(STATUS "  Compiler:        ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Build type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "  Build tests:     ${PREFIX_BUILD_TESTS}")
message(STATUS "============================================================")
message(STATUS "")
```

## `core/CMakeLists.txt`

```cmake
add_library(PROJ_core STATIC
    # src/....cpp   — add each file explicitly
)

target_include_directories(PROJ_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(PROJ_core
    PRIVATE
        PROJ_warnings
)

target_compile_features(PROJ_core PUBLIC cxx_std_20)
```

No `.cpp` yet: `add_library(PROJ_core INTERFACE)` and INTERFACE includes.
Header-only templates stay in `include/NS/`; do not add empty `.cpp` files.

## `app/CMakeLists.txt`

Only if there is an executable. Composition root: wiring, no business logic.

```cmake
add_executable(PROJ
    main.cpp
)
# set_target_properties(PROJ PROPERTIES OUTPUT_NAME "binary-name")

target_link_libraries(PROJ
    PRIVATE
        PROJ_core
        PROJ_warnings
)

if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    get_filename_component(_mingw_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
    foreach(_rt_dll IN ITEMS libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll)
        if(EXISTS "${_mingw_bin}/${_rt_dll}")
            add_custom_command(TARGET PROJ POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_mingw_bin}/${_rt_dll}"
                        "$<TARGET_FILE_DIR:PROJ>/${_rt_dll}"
                VERBATIM)
        endif()
    endforeach()
endif()
```

The POST_BUILD DLL copy is MinGW-w64 only so Explorer does not pick a
mismatched runtime off `PATH`. Skip the whole `app/` directory if library-only.

## `tests/CMakeLists.txt`

```cmake
add_executable(PROJ_core_tests
    main.cpp
    # additional *_tests.cpp files; no GLOB
)

target_include_directories(PROJ_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(PROJ_core_tests
    PRIVATE
        PROJ_core
        doctest::doctest
        PROJ_warnings
)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(PROJ_core_tests)
```

`tests/main.cpp`:

```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
```

## `CMakePresets.json`

Schema version 3. Substitute `PREFIX`.

```json
{
  "version": 3,
  "cmakeMinimumRequired": { "major": 3, "minor": 22, "patch": 0 },
  "configurePresets": [
    {
      "name": "base",
      "hidden": true,
      "binaryDir": "${sourceDir}/build",
      "cacheVariables": { "CMAKE_EXPORT_COMPILE_COMMANDS": "ON" }
    },
    {
      "name": "ninja",
      "displayName": "Ninja Release",
      "inherits": "base",
      "generator": "Ninja",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Release", "PREFIX_BUILD_TESTS": "ON" }
    },
    {
      "name": "ninja-debug",
      "displayName": "Ninja Debug",
      "inherits": "base",
      "generator": "Ninja",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug", "PREFIX_BUILD_TESTS": "ON" }
    },
    {
      "name": "mingw",
      "displayName": "MinGW Makefiles Release",
      "inherits": "base",
      "generator": "MinGW Makefiles",
      "condition": { "type": "equals", "lhs": "${hostSystemName}", "rhs": "Windows" },
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Release", "PREFIX_BUILD_TESTS": "ON" }
    }
  ],
  "buildPresets": [
    { "name": "ninja", "configurePreset": "ninja" },
    { "name": "ninja-debug", "configurePreset": "ninja-debug" },
    { "name": "mingw", "configurePreset": "mingw" }
  ],
  "testPresets": [
    { "name": "ninja", "configurePreset": "ninja", "output": { "outputOnFailure": true } },
    { "name": "ninja-debug", "configurePreset": "ninja-debug", "output": { "outputOnFailure": true } },
    { "name": "mingw", "configurePreset": "mingw", "output": { "outputOnFailure": true } }
  ]
}
```

Commands: `cmake --preset ninja` then `cmake --build --preset ninja` then
`ctest --preset ninja`. Without presets:
`cmake -S . -B build -G Ninja`, `cmake --build build --parallel`,
`ctest --test-dir build --output-on-failure`. MinGW:
`cmake --preset mingw` or
`cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release`.

## `.clang-format`

Include order in `.cpp`: matching project header in quotes, then standard
headers in angle brackets.

```
BasedOnStyle: LLVM
Language: Cpp
Standard: c++20
IndentWidth: 4
TabWidth: 4
UseTab: Never
ColumnLimit: 100
AccessModifierOffset: -4
BreakBeforeBraces: Attach
PointerAlignment: Left
ReferenceAlignment: Left
SortIncludes: true
IncludeBlocks: Regroup
IncludeCategories:
  - Regex: '^"'
    Priority: 1
  - Regex: '^<'
    Priority: 2
```

## `.gitignore`

Do not ignore `CMakePresets.json`, `.clang-format`, or `.gitattributes`.

```
build/
build-*/
out/
cmake-build-*/
CMakeFiles/
CMakeCache.txt
CTestTestfile.cmake
cmake_install.cmake
Makefile
*.cmake.in.user
compile_commands.json
.vs/
.vscode/
.idea/
*.swp
*.swo
*~
*.o
*.obj
*.exe
*.pdb
*.ilk
*.exp
*.lib
*.a
*.so
*.dll
*.dylib
configure.log
build.log
test.log
```

## `.gitattributes`

Keep scripts and workflows LF on Windows.

```
* text=auto
*.sh text eol=lf
*.yml text eol=lf
*.yaml text eol=lf
*.cmake text eol=lf
CMakeLists.txt text eol=lf
*.hpp text eol=lf
*.h text eol=lf
*.cpp text eol=lf
*.c text eol=lf
*.md text eol=lf
*.json text eol=lf
```

## `LICENSE`

Standard MIT text with the copyright holder the user gives (or git author).

## `compile_commands.json`

After the first successful configure:

```bash
cmake -E copy_if_different build/compile_commands.json compile_commands.json
```

The root copy is gitignored. Refresh after reconfigure. If configure cannot
run yet, skip and say so.

# AGENTS.md

Write these after the CMake tree exists. Quote real paths. Do not invent
domain rules. Every directory an agent will edit gets an `AGENTS.md`. Nested
files cover that directory only and start with "Read the root `AGENTS.md`
first." Omit files for directories that do not exist.

## Root `AGENTS.md`

```markdown
# AGENTS.md

<one-line description of what this repository is>.

## Project structure

- `core/` — `PROJ_core` static library. Logic, no UI toolkit headers. See `core/AGENTS.md`.
- `app/` — `PROJ` executable (composition root). Omit if library-only. See `app/AGENTS.md`.
- `tests/` — `PROJ_core_tests` doctest binary. Hermetic tests. See `tests/AGENTS.md`.
- `docs/` — long-form contributor documentation. See `docs/AGENTS.md`.
- `.github/workflows/` — GitHub Actions. See `.github/workflows/AGENTS.md`.
- `scripts/` — CI helper scripts. See `scripts/AGENTS.md`.
- `cmake/` — `Toolchain.cmake` (Clang first, MinGW-w64 fallback), `Dependencies.cmake` (FetchContent pins), `CompilerWarnings.cmake` (`PROJ_warnings` interface target).
- `CMakeLists.txt` — option `PREFIX_BUILD_TESTS` (default ON). `PREFIX_APP_VERSION` defaults to `${PROJECT_VERSION} (localbuild)`; omit if library-only.

Add a bullet for each extra first-party target. If a UI toolkit exists, name
the only directory allowed to include it.

## Architecture rules (do not break)

1. Dependencies point inward: `app` -> extra targets -> `core`. `core` depends on no other first-party target.
2. Logic targets must not include UI-toolkit headers. If a UI target exists, `rg "<toolkit-include>" core/` must return zero hits.
3. One directory per first-party target. Explicit source lists; no `GLOB`.
4. Public headers live under `core/include/NS/`. Implementations live under `core/src/` and mirror that tree.

Do not add product-specific contracts until that code exists. When it does,
put the rule here and in the owning nested file.

## Toolchain

- **Compilers**: Clang 14+ preferred, MinGW-w64 GCC 11+ fallback on Windows. **Do not** add MSVC support.
- **C++ standard**: C++20 (`CMAKE_CXX_STANDARD 20`, `CXX_EXTENSIONS OFF`).
- **Build system**: CMake 3.22+ with `FetchContent`. Pin every dep by tag in `cmake/Dependencies.cmake`; never use `master`.

## Key dependencies

| Library | Version | Purpose |
|---|---|---|
| doctest | v2.4.11 | Tests (only when `PREFIX_BUILD_TESTS=ON`) |

## Commands

Run from the workspace root.

- Configure: `cmake --preset ninja` (MinGW: `cmake --preset mingw`)
- Build: `cmake --build --preset ninja`
- Run: `./build/bin/<binary>` (`.\build\bin\<binary>.exe` on Windows)
- Tests: `ctest --test-dir build --output-on-failure`
- Tests only: `cmake --build build --target PROJ_core_tests`

Windows: linking fails with `Permission denied` if the exe is still running.
After configure, copy `build/compile_commands.json` to the repo root for clangd.

## Required follow-ups

- After adding a `.cpp` you **must** add it to that package's `CMakeLists.txt`. No glob.
- After adding a dependency you **must** verify license compatibility before merging.
- For new logic, add or update tests. Exercise both outcomes of meaningful conditionals.
- After adding a file under `docs/` you **must** list it in `docs/README.md` and `docs/AGENTS.md`.

## Agent collaboration (Cursor / AI)

- **Never** `git commit` or `git push` unless the user **explicitly** asked you to commit and/or push (e.g. "commit this", "push to origin"). Preparing diffs and suggesting commands is fine; performing those Git writes without explicit instruction is not.
- **Never** check out another branch **to change it** unless the user **explicitly** asked you to work on that branch. Temporarily checking out another branch **read-only** (inspect history, compare files, run `git show`) is fine without asking; switch back to the working branch before making edits unless instructed otherwise.

## Anti-patterns

- Don't include UI-toolkit headers from `core/`.
- Don't add tests that hit real network or real disk; use fakes. Narrow real-disk suites need a unique temp dir and RAII cleanup.
- Don't enable `-Wconversion` / `-Wsign-conversion`.
- Don't use `master` for FetchContent tags.
- Don't create multiple top-level triggers for the same CI intent. One orchestrator; `workflow_call` for OS splits.
```

## `core/AGENTS.md`

```markdown
# core/AGENTS.md

`PROJ_core` — logic library. **No UI-toolkit headers.** Read the root `AGENTS.md` first.

## Layer pointers

Fill as code appears. Typical split: `include/NS/` public headers; `src/`
non-template implementations mirroring that tree. Do not list types that do
not exist yet.

## Conventions

1. Public headers under `include/NS/`. Implementations under `src/`.
2. Templates visible to callers stay header-only. No empty `.cpp` for them.
3. This target links `PROJ_warnings` `PRIVATE`.
4. No UI-toolkit includes. Verify with a search of `core/`.
5. Add each new `.cpp` to `CMakeLists.txt`. No glob.

## Commands

`cmake --build build --target PROJ_core`
```

## `app/AGENTS.md`

Omit if library-only.

```markdown
# app/AGENTS.md

The `PROJ` executable — composition root. Read the root `AGENTS.md` first.

## File pointers

- `main.cpp` — program entry. Wires libraries. No business logic.
- `CMakeLists.txt` — links `PROJ_core` + `PROJ_warnings`.

## Conventions

1. Keep this target thin. More than construction and wiring belongs in `core/`.
2. Do not construct long-lived services on the stack inside startup.

## Commands

- Build: `cmake --build build --target PROJ`
- Run: `./build/bin/<binary>` (`.\build\bin\<binary>.exe` on Windows)
```

## `tests/AGENTS.md`

```markdown
# tests/AGENTS.md

`PROJ_core_tests` — doctest for `PROJ_core`. Hermetic, no real network. Read the root `AGENTS.md` first.

## File pointers

- `main.cpp` — `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN`. Do not put tests here.
- `CMakeLists.txt` — explicit `.cpp` list (no glob).
- Add one `*_tests.cpp` per area as tests appear. List each file here when added.

## Conventions

1. doctest. `#include <doctest/doctest.h>`, `TEST_SUITE` + `TEST_CASE`. `CHECK`, `REQUIRE`.
2. No real I/O unless a unique temp dir with RAII cleanup is required.
3. Single-file fakes stay in that file. Promote to `tests/fakes/` only when a second file needs them.
4. Names describe behavior, not implementation.
5. Add each `.cpp` to `tests/CMakeLists.txt`. No glob.

## Commands

- Build: `cmake --build build --target PROJ_core_tests`
- All: `ctest --test-dir build --output-on-failure`
- One case: `./build/bin/PROJ_core_tests --test-case="*pattern*"`

## Anti-patterns

- Don't depend on a UI target. Don't hard-code host paths. Don't require network.
```

## `docs/AGENTS.md`

```markdown
# AGENTS.md

Long-form contributor documentation outside the source tree.

## Files

- `README.md` — index.
- `intro-to-new-developers.md` — onboarding.
- `building-locally.md` — local build for Windows and Linux.
- `testing-and-test-code-of-conduct.md` — test workflow and standards.
- `ci-cd-guide.md` — CI workflows, merge guards, versioning in CI.
- `versioning.md` — feature-build version format and master semver rules.

## Conventions

- Contributor reference, not release notes. Precise. C++20 examples with real paths.
- Do not describe hypothetical features or unmerged branches as current behavior.

## Required follow-ups

- After adding a file under `docs/` add it to the list above **and** to `README.md`.
- After changing workflow topology or version policy, update `ci-cd-guide.md` and `versioning.md`.
- Do not rename this file without updating every other `AGENTS.md` that points at it.

## Anti-patterns

- Don't duplicate per-package `AGENTS.md` content; cross-link instead.
```

## `scripts/AGENTS.md`

```markdown
# AGENTS.md

- `compute_feature_version.sh` — branch+sha for feature CI.
- `compute_master_semver.sh` — semver bump for merged PRs to `master`.

Prefer script files over large inline workflow scripts. Deterministic,
non-interactive, fail-fast on bad titles. Shell scripts stay LF.
```

## `.github/workflows/AGENTS.md`

```markdown
# AGENTS.md

Governs `.github/workflows/*.yml`. One top-level trigger per intent:
`feature-ci.yml` (non-`master` push), `master-ci.yml` (merged PR to `master`).
OS splits via `workflow_call`.

## Current workflow map

- `feature-ci.yml` — feature orchestrator
- `feature-linux.yml` / `feature-windows.yml` — reusable build/test
- `master-pr-title-guard.yml` — semver prefix on PRs to `master`
- `master-ci.yml` — version, build/test, tag
- `master-linux.yml` / `master-windows.yml` — reusable build/test for master

## CI invariants (do not break)

1. Feature: branch+sha versions. Master: semver tags.
2. Do not duplicate top-level triggers for the same intent.
3. Master tagging gated to merged PRs only.
4. PR title prefixes stay aligned with `scripts/compute_master_semver.sh`.
5. No MSVC-specific build commands.

## Editing rules

Reusable workflows: `workflow_call`, `permissions: contents: read`.
Master orchestrator: `contents: write` for tagging. Linux: bash. Windows:
`shell: msys2 {0}` after `msys2/setup-msys2`.

After topology changes, update `docs/ci-cd-guide.md` and this map. After
prefix policy changes, update the guard, `compute_master_semver.sh`, and
`docs/versioning.md`. Do not add a second feature or master trigger. Do not
add packaging, installers, or extra scanners unless the user asked.
```

# Documentation

Substitute the project title, binary name, and real paths. Do not describe
a UI, installer, or domain feature that does not exist. Drop bullets for
directories that do not exist. Do not add `windows-installer.md` unless the
project ships an installer.

Root `README.md` stays short: overview, copy-paste commands, link to
`docs/`. Architecture belongs in `intro-to-new-developers.md`.

## Root `README.md`

````markdown
# {Project Title}

{One paragraph: what this repository is.}

## Technical Overview

C++20 project built with CMake.

- `core/`: library (`PROJ_core`)
- `app/`: executable composition root

## Quick Start

### Prerequisites

- CMake 3.22+
- Clang 14+ (preferred) or MinGW-w64 GCC 11+
- Ninja (recommended) or Make

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
./build/bin/{binary}
```

On Windows: `.\build\bin\{binary}.exe`

## Documentation

Contributor docs live in [`docs/`](docs/README.md). Start with
[Intro For New Developers](docs/intro-to-new-developers.md) and
[Building Locally](docs/building-locally.md).

## License

This project is licensed under the [MIT License](LICENSE) unless a different
license was specified.
````

## `docs/README.md`

```markdown
#documentation #contributors

# Documentation Index

This folder contains contributor documentation. Start with
[Intro for New Developers](intro-to-new-developers.md).

## CI/CD And Release

- [ci-cd-guide.md](ci-cd-guide.md): CI workflows, merge guards, and tagging.
- [versioning.md](versioning.md): feature-build version format and master semver.

## Build And Local Setup

- [building-locally.md](building-locally.md): Windows/Linux build, deps, troubleshooting.

## Onboarding And Development Workflows

- [intro-to-new-developers.md](intro-to-new-developers.md): architecture, pitfalls, first-week workflow.
- [testing-and-test-code-of-conduct.md](testing-and-test-code-of-conduct.md): hermetic, behavior-focused tests.
```

## `docs/intro-to-new-developers.md`

```markdown
#documentation #onboarding #architecture

# Intro For New Developers

For local setup, start with [Building Locally](building-locally.md).

**Quick Setup:** read `AGENTS.md` files first, build once, run tests once,
then make one small layer-scoped change.

## Toolchain Snapshot

- CMake 3.22+ with `FetchContent`
- C++20, Clang preferred, MinGW-w64 GCC fallback on Windows
- doctest (`PREFIX_BUILD_TESTS`, default ON)

## Project Shape

- `core/`: `PROJ_core` logic library
- `app/`: executable composition root
- `tests/`: hermetic unit tests for `core/`

Extra targets depend on `core`. `core` depends on no other first-party target.
If a UI toolkit is added later, it gets its own directory; `core/` must not
include its headers.

## Architecture Rules

- One directory per first-party target. Explicit sources; no glob.
- Public headers under `core/include/NS/`.
- Executable wiring belongs in `app/`, not in `core/`.

## First-Day Checklist

1. Read repository `AGENTS.md` files.
2. Build with [Building Locally](building-locally.md).
3. Run the test suite once.
4. Make one small layer-contained change.
5. Rebuild and rerun relevant tests.

## Avoid These Pitfalls

- putting business logic in `app/`
- using unpinned FetchContent tags
- enabling `-Wconversion` / `-Wsign-conversion`
- adding tests that need network or host-specific paths

## Deeper Docs

- [Building Locally](building-locally.md)
- [Testing Guide](testing-and-test-code-of-conduct.md)
- [CI/CD Guide](ci-cd-guide.md)
- [Versioning Guide](versioning.md)
```

## `docs/building-locally.md`

Filename is `building-locally.md`, not `dow-doc-build-locally.md`.

````markdown
#documentation #build #cmake

# Building Locally

How to build on Windows and Linux. Architecture:
[Intro For New Developers](intro-to-new-developers.md).

**Quick Setup:** configure, build, run tests from the same build directory.

## Build Model

- C++20; library `PROJ_core`; executable `PROJ` (omit if library-only);
  tests `PROJ_core_tests`

## Dependency Model

`FetchContent` in `cmake/Dependencies.cmake`. Pin by tag. First configure
clones into `build/_deps`; later configures reuse that cache
(`FETCHCONTENT_UPDATES_DISCONNECTED`). Pinned at setup: doctest `v2.4.11`
(when tests are enabled).

## Prerequisites

Windows recommended: LLVM/Clang 14+, CMake 3.22+, Ninja.
Fallback: MSYS2 UCRT64 (`mingw-w64-ucrt-x86_64-gcc`, CMake, Ninja or Make).
MSVC is not supported.

Linux: CMake 3.22+, Clang (preferred) or GCC with C++20, Ninja recommended.

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
- Force a compiler with `-DCMAKE_CXX_COMPILER=...` (Toolchain.cmake respects it).
````

## `docs/testing-and-test-code-of-conduct.md`

````markdown
#documentation #testing #quality

# Testing Guide And Test Code Of Conduct

Toolchain: [Building Locally](building-locally.md).

**Quick Setup:** run `PROJ_core_tests` from a clean build. Keep tests
hermetic. Update contract tests in the same change when contracts move.

Deterministic, fast, behavior-oriented tests of `core/`. UI, if any, is
manual until a UI harness exists.

- framework: doctest; target: `PROJ_core_tests`; dir: `tests/`;
  default `PREFIX_BUILD_TESTS=ON`

```bash
cmake --preset ninja
cmake --build build --target PROJ_core_tests
ctest --test-dir build --output-on-failure
```

### Keep Tests Hermetic

No real network. No local machine files. Fakes where possible. Real disk
only with a unique temp dir and RAII cleanup.

### Test Behavior, Not Internals

Assert externally visible outcomes. Avoid brittle implementation details.

### Keep Tests Deterministic

No unseeded randomness, timing-sensitive asserts, or ordering assumptions
unless ordering is the contract.

### Keep Tests Readable

One intent per case. Descriptive names ("missing file is created with
defaults", not `test_init_no_file`). Arrange / act / assert.
````

## `docs/ci-cd-guide.md`

```markdown
#documentation #ci-cd #github-actions

# CI/CD Guide

Versioning policy: [Versioning Guide](versioning.md).

**Quick Setup:** protect `master` with required checks
`Require semver prefix in PR title`, Linux build + tests, Windows build + tests.

## Workflow Map

- `feature-ci.yml`: non-`master` pushes; Linux and Windows feature builds.
- `feature-linux.yml` / `feature-windows.yml`: reusable build/test.
- `master-pr-title-guard.yml`: PR title prefixes for PRs targeting `master`.
- `master-ci.yml`: merged PRs to `master`; computes semver, builds, tags.
- `master-linux.yml` / `master-windows.yml`: reusable build/test for master.

This skeleton stops at build and test. Packaging jobs are added when the
project produces a distributable artifact.

## Version Flow

Feature: `<branch-name>-<short-sha>` via `scripts/compute_feature_version.sh`,
passed as `-DPREFIX_APP_VERSION=...` when that option exists.

Master: next semver from latest `vMAJOR.MINOR.PATCH` tag and merged PR title
via `scripts/compute_master_semver.sh`. CI tags `v<version>`.
Prefixes: `major`, `minor`, `fix`, `patch`, `path` (patch alias).

## Master Merge Guard

PRs targeting `master` must start with an accepted prefix or
`master-pr-title-guard.yml` fails.

## Release Procedure

1. Open a PR to `master` with a valid prefix (`major:`, `minor:`, `fix:`, `patch:`).
2. Merge. Master CI computes the version, builds, tests, and tags.
```

## `docs/versioning.md`

```markdown
#documentation #versioning #releases

# Versioning Guide

Workflow wiring: [CI/CD Guide](ci-cd-guide.md).

**Quick Setup:** choose a valid PR title prefix before opening a `master` PR.

- Feature (non-`master`): `<branch-name>-<short-sha>` (sanitized lowercase
  branch, 7-char SHA). `scripts/compute_feature_version.sh`. Used as
  `PREFIX_APP_VERSION` when that option exists.
- Master: `MAJOR.MINOR.PATCH` from latest semver tag and PR title.
  `major...` bumps MAJOR; `minor...` bumps MINOR; `fix` / `patch` / `path`
  bump PATCH. `scripts/compute_master_semver.sh`. First release starts at
  `0.0.0`. Tag format `v<semantic-version>`.
- Embedded `PREFIX_APP_VERSION` when it exists: feature CI uses
  `<branch>-<sha>`; master CI uses semver; local is
  `${PROJECT_VERSION} (localbuild)` unless overridden.

PR titles: `major: <summary>`, `minor: <summary>`, `fix: <summary>`,
`patch: <summary>`.
```

# CI skeleton

Build and test only. No packaging, no extra scanners, no UI-toolkit packages.
Linux: Clang + Ninja + ccache. Windows: MSYS2 UCRT64 GCC + Ninja + ccache.
Substitute `PREFIX`. Pass `-DPREFIX_APP_VERSION` only when that option exists.

`scripts/*.sh` must be LF and executable (`git update-index --chmod=+x`).

Add packaging later, only when the project produces a distributable artifact.

## `scripts/compute_feature_version.sh`

```bash
#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <branch-name> <commit-sha>" >&2
  exit 1
fi

branch_name="$1"
commit_sha="$2"

branch_safe="$(echo "${branch_name}" | tr '[:upper:]' '[:lower:]' | sed -E 's/[^a-z0-9._-]+/-/g; s/^-+|-+$//g')"
short_sha="$(echo "${commit_sha}" | cut -c1-7)"

if [[ -z "${branch_safe}" ]]; then
  branch_safe="branch"
fi

echo "${branch_safe}-${short_sha}"
```

## `scripts/compute_master_semver.sh`

```bash
#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <pr-title>" >&2
  exit 1
fi

pr_title="$(echo "$1" | tr '[:upper:]' '[:lower:]')"

if [[ "${pr_title}" == major* ]]; then
  bump="major"
elif [[ "${pr_title}" == minor* ]]; then
  bump="minor"
elif [[ "${pr_title}" == fix* || "${pr_title}" == patch* || "${pr_title}" == path* ]]; then
  bump="patch"
else
  echo "Invalid PR title prefix. Must start with: major, minor, fix, patch, or path." >&2
  exit 1
fi

latest="$(git tag --list | sed -E 's/^v//' | grep -E '^[0-9]+\.[0-9]+\.[0-9]+$' | sort -t. -k1,1nr -k2,2nr -k3,3nr | head -n1 || true)"
if [[ -z "${latest}" ]]; then
  latest="0.0.0"
fi

IFS='.' read -r major minor patch <<< "${latest}"

case "${bump}" in
  major) major=$((major + 1)); minor=0; patch=0 ;;
  minor) minor=$((minor + 1)); patch=0 ;;
  patch) patch=$((patch + 1)) ;;
esac

version="${major}.${minor}.${patch}"
release_tag="v${version}"

if [[ -n "${GITHUB_OUTPUT:-}" ]]; then
  {
    echo "version=${version}"
    echo "release_tag=${release_tag}"
  } >> "${GITHUB_OUTPUT}"
else
  echo "${version}"
fi
```

## `.github/workflows/feature-ci.yml`

```yaml
name: Feature CI

on:
  push:
    branches-ignore:
      - master

permissions:
  contents: read

jobs:
  linux:
    name: Linux build + tests
    uses: ./.github/workflows/feature-linux.yml

  windows:
    name: Windows build + tests
    uses: ./.github/workflows/feature-windows.yml
```

## `.github/workflows/feature-linux.yml`

```yaml
name: Feature Linux Build and Test

on:
  workflow_call:

permissions:
  contents: read

jobs:
  build-linux:
    name: Linux build + tests
    runs-on: ubuntu-latest
    env:
      CCACHE_DIR: ${{ github.workspace }}/.ccache
    steps:
      - uses: actions/checkout@v4
      - name: Compute feature version
        run: |
          VERSION="$(bash scripts/compute_feature_version.sh "${GITHUB_REF_NAME}" "${GITHUB_SHA}")"
          echo "VERSION=${VERSION}" >> "$GITHUB_ENV"
      - name: Install build tools
        run: sudo apt-get update && sudo apt-get install -y clang ninja-build ccache cmake
      - uses: actions/cache@v4
        with:
          path: ${{ env.CCACHE_DIR }}
          key: ccache-linux-${{ github.ref_name }}-v1
          restore-keys: |
            ccache-linux-${{ github.ref_name }}-
            ccache-linux-
      - name: Configure
        run: >
          cmake -S . -B build -G Ninja
          -DCMAKE_C_COMPILER=clang
          -DCMAKE_CXX_COMPILER=clang++
          -DCMAKE_BUILD_TYPE=Release
          -DPREFIX_BUILD_TESTS=ON
          -DPREFIX_APP_VERSION="${VERSION}"
          -DCMAKE_C_COMPILER_LAUNCHER=ccache
          -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
      - run: cmake --build build --parallel
      - run: ctest --test-dir build --output-on-failure
```

## `.github/workflows/feature-windows.yml`

```yaml
name: Feature Windows Build and Test

on:
  workflow_call:

permissions:
  contents: read

jobs:
  build-windows:
    name: Windows build + tests
    runs-on: windows-latest
    env:
      CCACHE_DIR_WIN: ${{ github.workspace }}\.ccache
    defaults:
      run:
        shell: msys2 {0}
    steps:
      - uses: actions/checkout@v4
      - uses: msys2/setup-msys2@v2
        with:
          msystem: UCRT64
          update: true
          cache: true
          install: >-
            mingw-w64-ucrt-x86_64-gcc
            mingw-w64-ucrt-x86_64-cmake
            mingw-w64-ucrt-x86_64-ninja
            mingw-w64-ucrt-x86_64-ccache
      - name: Compute feature version
        run: |
          VERSION="$(bash scripts/compute_feature_version.sh "${GITHUB_REF_NAME}" "${GITHUB_SHA}")"
          echo "VERSION=${VERSION}" >> "$GITHUB_ENV"
      - uses: actions/cache@v4
        with:
          path: ${{ env.CCACHE_DIR_WIN }}
          key: ccache-windows-${{ github.ref_name }}-v1
          restore-keys: |
            ccache-windows-${{ github.ref_name }}-
            ccache-windows-
      - name: Configure MSYS2 ccache directory
        run: |
          CCACHE_DIR_POSIX="$(cygpath -u "$CCACHE_DIR_WIN")"
          echo "CCACHE_DIR=${CCACHE_DIR_POSIX}" >> "$GITHUB_ENV"
          mkdir -p "$CCACHE_DIR_POSIX"
      - name: Configure
        run: >
          cmake -S . -B build -G Ninja
          -DCMAKE_BUILD_TYPE=Release
          -DPREFIX_BUILD_TESTS=ON
          -DPREFIX_APP_VERSION="${VERSION}"
          -DCMAKE_C_COMPILER_LAUNCHER=ccache
          -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
      - run: cmake --build build --parallel
      - run: ctest --test-dir build --output-on-failure
```

## `.github/workflows/master-pr-title-guard.yml`

```yaml
name: Master PR Title Guard

on:
  pull_request:
    branches: [master]
    types: [opened, edited, synchronize, reopened, ready_for_review]

permissions:
  contents: read

jobs:
  validate-title:
    name: Require semver prefix in PR title
    runs-on: ubuntu-latest
    steps:
      - env:
          PR_TITLE: ${{ github.event.pull_request.title }}
        run: |
          title_lower="$(echo "${PR_TITLE}" | tr '[:upper:]' '[:lower:]')"
          if [[ "${title_lower}" == major* || "${title_lower}" == minor* || "${title_lower}" == fix* || "${title_lower}" == patch* || "${title_lower}" == path* ]]; then
            echo "PR title is valid: ${PR_TITLE}"
            exit 0
          fi
          echo "Invalid PR title: '${PR_TITLE}'"
          echo "Title must start with one of: major, minor, fix, patch, or path."
          exit 1
```

## `.github/workflows/master-ci.yml`

Reusable jobs stay `contents: read`. This file needs `contents: write`.

```yaml
name: Master CI

on:
  push:
    branches: [master]

permissions:
  contents: write

jobs:
  compute-version:
    name: Determine semantic version
    runs-on: ubuntu-latest
    outputs:
      version: ${{ steps.version.outputs.version }}
      release_tag: ${{ steps.version.outputs.release_tag }}
    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0
      - id: pr
        uses: actions/github-script@v7
        with:
          script: |
            const { owner, repo } = context.repo;
            const pulls = await github.rest.repos.listPullRequestsAssociatedWithCommit({
              owner, repo, commit_sha: context.sha,
            });
            if (!pulls.data.length) {
              core.setFailed(`No PR found for commit ${context.sha}. Release flow expects merges into master through PRs.`);
              return;
            }
            const mergedPr = pulls.data.find((pr) => pr.merged_at) ?? pulls.data[0];
            core.setOutput("pr_title", mergedPr.title);
      - id: version
        run: bash scripts/compute_master_semver.sh "${{ steps.pr.outputs.pr_title }}"

  linux:
    name: Linux build + tests
    needs: compute-version
    uses: ./.github/workflows/master-linux.yml
    with:
      version: ${{ needs.compute-version.outputs.version }}

  windows:
    name: Windows build + tests
    needs: compute-version
    uses: ./.github/workflows/master-windows.yml
    with:
      version: ${{ needs.compute-version.outputs.version }}

  tag-master:
    name: Tag master
    needs: [compute-version, linux, windows]
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: softprops/action-gh-release@v2
        with:
          tag_name: ${{ needs.compute-version.outputs.release_tag }}
          target_commitish: ${{ github.sha }}
          generate_release_notes: true
```

## `master-linux.yml` and `master-windows.yml`

Same as the feature reusable workflows except:

- `on.workflow_call.inputs.version` (`required: true`, `type: string`)
- no "Compute feature version" step
- cache key `ccache-linux-master-v1` / `ccache-windows-master-v1`
- `-DPREFIX_APP_VERSION="${{ inputs.version }}"`

# Stop

Do not invent sample sources, a UI, or an installer. Do not `git commit`
or `git push` unless the user explicitly asked.

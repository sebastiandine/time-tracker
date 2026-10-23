#documentation #onboarding #architecture

# Intro For New Developers

For local setup, start with [Building Locally](building-locally.md).
Timer behavior is [Time tracking](time-tracking.md). The tables are
[the schema](database/schema.md).

**Quick Setup:** read `AGENTS.md` files first, build once, run tests once,
then make one small layer-scoped change.

## Toolchain Snapshot

- CMake 3.22+ with `FetchContent`
- C++20, Clang preferred, MinGW-w64 GCC fallback on Windows
- doctest (`TIME_TRACKER_BUILD_TESTS`, default ON)
- Qt 6.5+ Widgets when `TIME_TRACKER_BUILD_APP` is ON

## Project Shape

- `core/`: `time_tracker_core` logic library
- `ui/`: Qt Widgets
- `app/`: executable composition root
- `tests/`: hermetic unit tests for `core/`

`ui` depends on `core`. `core` depends on no other first-party target and
does not include Qt headers.

## Architecture Rules

- One directory per first-party target. Explicit sources; no glob.
- Public headers under `core/include/timetracker/`.
- Executable wiring belongs in `app/`, not in `core/`.
- One running time entry. Its start timestamp is the recovery source.

## First-Day Checklist

1. Read repository `AGENTS.md` files.
2. Build with [Building Locally](building-locally.md).
3. Run the test suite once.
4. Make one small layer-contained change.
5. Rebuild and rerun relevant tests.

## Avoid These Pitfalls

- putting business logic in `app/` or `ui/`
- including Qt from `core/`
- using unpinned FetchContent tags
- enabling `-Wconversion` / `-Wsign-conversion`
- adding tests that need network or host-specific paths

## Deeper Docs

- [Building Locally](building-locally.md)
- [Testing Guide](testing-and-test-code-of-conduct.md)
- [CI/CD Guide](ci-cd-guide.md)
- [Versioning Guide](versioning.md)
- [Time tracking](time-tracking.md)

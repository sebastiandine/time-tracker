# C++ Unit Testing

Cursor Agent Skill for writing **hermetic doctest** tests against C++20
logic, keeping them tied to business rules, and holding **line coverage
at or above 80%** and **branch coverage at or above 70%** on `core/`
(and peer non-UI libraries).

This is the human-readable companion to [`SKILL.md`](SKILL.md). Agents follow
`SKILL.md` only; every template lives in that file.

## Scope

The skill is the ongoing testing standard. It assumes the
`cpp-project-setup` layout (`PREFIX_BUILD_TESTS`, doctest 2.4.11,
`tests/main.cpp` harness, `PROJ_core_tests`). It fills gaps in that
harness. It does **not** re-scaffold the repo, add a second test
framework, or invent UI automation.

It does **not** require tests for trivial getters, defaulted special
members, or third-party code. Coverage padding is not a business rule.

The UI toolkit target and the `app/` composition root stay out of the
automated denominator. Those surfaces are checked by hand.

## When to apply

Use this skill when writing or changing unit tests, or when adding or
changing logic that must ship with tests in the same change.

Trigger phrases: unit tests, `TEST_CASE`, doctest, coverage, `gcovr`,
`ctest`, fakes, core logic.

If the repo is not yet the setup layout, apply `cpp-project-setup` first,
then this skill.

## Conventions

| Area | Rule |
|---|---|
| Framework | doctest 2.4.11. `TEST_SUITE` + `TEST_CASE`. `CHECK` / `REQUIRE`. Harness only in `tests/main.cpp`. |
| Same change | New or changed logic ships with tests. A bug fix adds a case that failed on the old behavior and passes after the fix. |
| Coverage | Line >= 80% and branch >= 70% on first-party logic (`core/` and peer non-UI libraries). Exclude the UI toolkit target, `app/`, `tests/`, and FetchContent trees. |
| Branches | The percentages are a floor. Every meaningful branch still needs both outcomes asserted (success/error, empty/non-empty, hit/miss, accept/reject). |
| Measurement | Separate `build-cov` with **GCC** and `--coverage`. `gcovr --fail-under-line 80 --fail-under-branch 70`, `--filter "core/"`, `--exclude-directories` on `_deps`. Clang notes are not compatible with default `gcov`. |
| Oracle | False positive: fails while the behavior is correct (flake, host path, call-order trivia). False negative: passes while the behavior is wrong (happy path only, assert only `isOk()`, tautology). |
| Names | Describe the business rule, not the method. One behavior per `TEST_CASE`. |
| Hermetic | In-memory fakes. No network. No host paths. Real disk only with a unique temp dir and RAII cleanup. In-process databases are allowed. |
| Fakes | Stay in the test file until a second file needs them, then `tests/fakes/`. Prefer a realistic fake over a call-recording mock. |
| Sources | Explicit list in `tests/CMakeLists.txt`. No `GLOB`. Do not link a UI target. |
| Property checks | Table or fixed-seed loop inside doctest, plus one readable example. No extra library. No unseeded `rand()`. |
| Sanitizers | Optional ASan + UBSan extra build. Not a gate. Skip on toolchains that cannot link them (typical MinGW-w64). |
| CI | When workflows exist, add a Linux GCC coverage job that fails under the bars. Leave existing Clang/MinGW test jobs unchanged. |
| Git (agents) | No commit, push, or branch-switch-to-edit unless the user explicitly asked. |

## Files

| File | Audience |
|---|---|
| [`SKILL.md`](SKILL.md) | Agent procedure and all examples |
| [`README.md`](README.md) | This human explanation |

`NS`, `PROJ`, and `PREFIX` in `SKILL.md` examples are stand-ins for the
project's namespace, target stem, and CMake option prefix. Agents must
use real names.

## Using the skill in another repo

This copy lives only in this skills repository
(`.cursor/skills/cpp-unit-testing/`). Cursor loads **project** skills from
the repo you have open, and **personal** skills from `~/.cursor/skills/`.

To use it while working in other C++ projects, copy the directory:

```text
.cursor/skills/cpp-unit-testing/  →  ~/.cursor/skills/cpp-unit-testing/
```

On Windows the personal skills directory is typically
`C:\Users\<you>\.cursor\skills\`. Do not copy it into
`~/.cursor/skills-cursor/` (that tree is reserved for Cursor's built-in
skills).

After copying, a later agent session can pick the skill up from the
description (unit tests, doctest, coverage, `gcovr`, `ctest`, fakes) or
you can mention it by name.

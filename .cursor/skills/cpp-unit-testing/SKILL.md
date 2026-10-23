---
name: cpp-unit-testing
description: >-
  Writes hermetic doctest unit tests for C++20 logic, enforces 80%
  line and 70% branch coverage on core, and keeps tests tied to business
  rules rather than coverage padding. Use when writing or updating unit
  tests, TEST_CASE, doctest, coverage, gcovr, ctest, fakes, or when adding
  or changing core logic.
---

# C++ Unit Testing

Apply whenever you add or change logic, tests, coverage measurement, or the
test harness. Testing is part of the change, not a follow-up. Do not
re-scaffold CMake, doctest, or CI. Do not add a second test framework.

This skill assumes the `cpp-project-setup` layout: `PREFIX_BUILD_TESTS`
(default ON), doctest 2.4.11 fetched only then, `tests/main.cpp` as the
harness (no `TEST_CASE` there), target `PROJ_core_tests`, explicit sources
(no `GLOB`). If that harness is missing, add only the missing pieces. Do
not rewrite a healthy tree. Substitute `PROJ`, `PREFIX`, and `NS`. Do not
leave those tokens in the project.

```
Test progress (copy into the task):
- [ ] Tests in the same change as the logic (or a regression case for a fix)
- [ ] Names and asserts state the business rule, both outcomes of each
      meaningful branch
- [ ] Hermetic: fakes, no network, no host paths
- [ ] New `*_tests.cpp` listed in `tests/CMakeLists.txt` (no glob)
- [ ] `ctest --test-dir <build> --output-on-failure` green
- [ ] `gcovr` on logic: line >= 80%, branch >= 70%
- [ ] CI coverage job fails under those bars (when workflows exist)
```

Do not finish non-trivial logic work while a box is unchecked.

# When tests are required

Ship tests in the **same change** as:

- New or changed behavior in `core/` (or any peer non-UI library)
- A new service, parser, mapper, repository, or pure function
- A changed contract (JSON shape, validation, error identity, sort/filter
  key, aggregate rule)
- A bug fix — the new case must fail on the old behavior and pass after
  the fix

The UI toolkit target and the `app/` composition root have no automated
suite. Exercise them by hand. Do not link a UI target from tests.

Do not add tests for trivial getters, defaulted special members, or
third-party code. That is coverage padding, not a business rule.

# Coverage bar

Measure first-party **logic** only: `core/` and peer non-UI libraries.

| Gate | Threshold |
|---|---|
| Line coverage | **>= 80%** |
| Branch coverage | **>= 70%** |

Exclude from the denominator: the UI toolkit target, `app/`, `tests/`,
FetchContent trees (`build-cov/_deps`). A hosted scanner, if the repo has
one, must use the same surface.

The percentages are a floor, not a substitute for both outcomes of a
meaningful branch (success/error, empty/non-empty, hit/miss, accept/reject).
A green 80% with only happy paths is unfinished.

Do not finish under either bar. Do not lower a bar, exclude production
logic to inflate the number, or add empty tests to chase a percentage.

# Measure coverage

The default toolchain prefers Clang. `gcovr`'s default `gcov` does not
read Clang notes. Coverage configure is **GCC**, in a **separate**
`build-cov` directory:

```bash
cmake -S . -B build-cov -G Ninja \
  -DCMAKE_C_COMPILER=gcc \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_BUILD_TYPE=Debug \
  -DPREFIX_BUILD_TESTS=ON \
  -DCMAKE_C_FLAGS=--coverage \
  -DCMAKE_CXX_FLAGS=--coverage

cmake --build build-cov --target PROJ_core_tests --parallel
ctest --test-dir build-cov --output-on-failure

gcovr -r . --object-directory build-cov \
  --filter "core/" \
  --exclude "build-cov/_deps/" \
  --exclude-directories "build-cov/_deps" \
  --fail-under-line 80 \
  --fail-under-branch 70 \
  --print-summary
```

`--exclude-directories` is required: output-path excludes miss third-party
`.gcda` files. Add another `--filter` for each extra logic library. On
Windows MinGW, the same flags work with `-G "MinGW Makefiles"`.

Day-to-day (no coverage): `ctest --test-dir build --output-on-failure`.
One case: `./build/bin/PROJ_core_tests --test-case="*pattern*"`.

# Oracle: false positives and false negatives

A test is an oracle for a **business rule**. Two failure modes:

| Kind | Symptom | Typical cause |
|---|---|---|
| **False positive** | Fails while the behavior is correct | Flake, host path, clock, unseeded RNG, call-order trivia, incidental wording, separator `\` vs `/` |
| **False negative** | Passes while the behavior is wrong | Happy path only, `CHECK(r.isOk())` and stop, assert the value the fake was scripted to return, tautology (`CHECK(x == x)`), no assert on the field the caller depends on |

Prefer domain-level outcomes over internals. Assert quantities, error
identity the caller depends on, sort/filter/aggregate results, and
round-trip equality. Do not assert private members, incidental log text,
or how many times a fake was called unless that count **is** the contract
(for example a cache must not re-hit the source).

## False negative (do not)

```cpp
TEST_CASE("create succeeds") {
    auto r = svc.create(1, "2026-09-13", "  notes  ");
    CHECK(r.isOk());
}
```

Wrong notes, a bad date that still returns `ok`, or a missing trim would
all pass.

## False negative (do)

```cpp
TEST_CASE("create trims notes") {
    auto r = svc.create(1, "2026-09-13", "  notes  ");
    REQUIRE(r.isOk());
    CHECK(r.value().notes == "notes");
}

TEST_CASE("create rejects empty date") {
    auto r = svc.create(1, "", "notes");
    REQUIRE(r.isErr());
    CHECK(r.error().find("YYYY-MM-DD") != std::string::npos);
}
```

`REQUIRE` only for a precondition later asserts depend on. Several `CHECK`s
on the same outcome are fine.

## False positive (do not)

```cpp
TEST_CASE("loads config from disk") {
    CHECK(svc.load("/home/alice/.config/app.json").isOk());
}
```

Host-dependent. Also a false positive: asserting `calls == 3` on a fake
when the contract is "missing file is created with defaults", not a call
count.

## False positive (do)

```cpp
TEST_CASE("missing file is created with defaults") {
    testing::InMemoryFileSystem fs;
    ConfigService svc(fs);
    auto r = svc.load("config.json");
    REQUIRE(r.isOk());
    CHECK(r.value().theme == Theme::System);
    REQUIRE(fs.readText("config.json").isOk());
}
```

# File and doctest conventions

- One `tests/<area>_tests.cpp` per service, parser, or rule set. Name the
  file after the unit, not `test1.cpp`.
- `#include <doctest/doctest.h>`. `TEST_SUITE("TypeOrFunction")` +
  `TEST_CASE("behavior in words")`. Names describe the rule ("missing file
  is created with defaults"), not the method (`"test_init_no_file"`).
- One behavior per `TEST_CASE`. Do not pack unrelated rules into one case
  so a single failure is ambiguous.
- Asserts: `CHECK`, `REQUIRE`, `CHECK_FALSE`, `REQUIRE_FALSE`,
  `CHECK_THROWS` / `CHECK_THROWS_AS` when the API throws. Prefer
  `doctest::Approx` for floating ratios, not exact binary equality.
- Path strings in expectations use `/`. Fakes normalize with
  `lexically_normal().generic_string()`. Do not hard-code `\`.
- Add every new `.cpp` to `tests/CMakeLists.txt`. No glob. Do not put
  cases in `tests/main.cpp`.
- Link `PROJ_core`, `doctest::doctest`, and `PROJ_warnings`. Do not link
  a UI library.

After a new service in `core/`, add `<name>_tests.cpp` with at least the
happy path and one error path. After a domain JSON field or alias moves,
extend the round-trip file in the same change.

# Fakes

Hermetic: no real network, no host home/temp paths, no wall clock as an
oracle. Drive ports with in-memory fakes.

- A fake used by one file stays in that file, anonymous namespace.
- Promote to `tests/fakes/` only when a **second** file needs it.
- Prefer a small realistic fake (in-memory map, scripted HTTP body) over
  a mock that records every call.
- Narrow exceptions: unique directory under `temp_directory_path()` named
  `PROJ_*_test_*` plus RAII cleanup; in-process database. Do not add
  further real I/O without the same cleanup.

# What to assert

**Services.** Arrange through fakes. Act on the public method. Assert the
value or error the caller uses (trimmed fields, rejected empty ids, not
merely `isOk()`). When the service writes, read back through the fake.

**Pure rules** (parse, sort, filter, aggregate). Input table → exact
output. Cover empty input, duplicates, case folding, and the documented
skip rules. Example names pin the rule: "zero owned is missing only",
"win percentage is empty when there are no games".

**Persistence / JSON.** Round-trip every field and alias. A schema change
without an updated round-trip is incomplete.

**Adapters at a boundary** (HTTP parse, file format). Script the body on
a fake client. Assert mapped domain fields and the error **kind**
(`NotFound` vs `Transient`, missing vs malformed) when that kind changes
caller behavior.

# Bug-fix regression

1. Write the case against current code. It must **fail**.
2. Fix the production code.
3. The same case must **pass**.
4. Keep the case. Do not delete it once the bug is gone.

If the case cannot fail on the old code, it is not a regression test.

# Property checks

Stay inside doctest. No extra library.

Use a table or a fixed-seed loop that states an invariant, **and** one
readable example so a failure is diagnosable:

```cpp
TEST_CASE("parseQuantity accepts 1..99 and rejects the rest") {
    struct Row {
        const char* in;
        bool ok;
        int value;
    };
    const Row rows[] = {
        {"1", true, 1},
        {"99", true, 99},
        {"0", false, 0},
        {"100", false, 0},
        {"", false, 0},
        {"x", false, 0},
    };
    for (const auto& row : rows) {
        CAPTURE(row.in);
        auto r = parseQuantity(row.in);
        REQUIRE(r.isOk() == row.ok);
        if (row.ok) {
            CHECK(r.value() == row.value);
        }
    }
}
```

Unseeded `rand()` is a false-positive risk. If you loop, pin the seed and
keep the iteration count small.

# Sanitizers

Optional extra build when the toolchain can link
`-fsanitize=address,undefined` (Clang or GCC on Linux; Clang on recent
Windows). Not a gate. Skip on toolchains that cannot — typical MinGW-w64
is one of those. Do not add sanitizer flags to the default `build/` or to
`build-cov`.

```bash
cmake -S . -B build-asan -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DPREFIX_BUILD_TESTS=ON \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined" \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-asan --target PROJ_core_tests --parallel
ctest --test-dir build-asan --output-on-failure
```

# CI coverage job

When `.github/workflows/` already exists, add a Linux **GCC** coverage
job that fails under the bars. Leave existing Clang/MinGW test jobs
unchanged. Install `gcovr`. Do not point this job at Clang.

```yaml
  coverage:
    name: Linux coverage gate
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install build tools
        run: sudo apt-get update && sudo apt-get install -y gcc g++ ninja-build cmake gcovr
      - name: Configure coverage
        run: >
          cmake -S . -B build-cov -G Ninja
          -DCMAKE_C_COMPILER=gcc
          -DCMAKE_CXX_COMPILER=g++
          -DCMAKE_BUILD_TYPE=Debug
          -DPREFIX_BUILD_TESTS=ON
          -DCMAKE_C_FLAGS=--coverage
          -DCMAKE_CXX_FLAGS=--coverage
      - name: Build and test
        run: >
          cmake --build build-cov --target PROJ_core_tests --parallel &&
          ctest --test-dir build-cov --output-on-failure
      - name: Enforce coverage bars
        run: >
          gcovr -r . --object-directory build-cov
          --filter "core/"
          --exclude "build-cov/_deps/"
          --exclude-directories "build-cov/_deps"
          --fail-under-line 80
          --fail-under-branch 70
          --print-summary
```

Wire it into the existing orchestrator (`needs` / extra job). Do not
replace the Clang Linux job with this one. If the repo has extra
apt packages for a UI, add them here only when `PREFIX_BUILD_TESTS`
configure requires them.

# Stop

When the checklist is green, stop. Do not add UI automation, a second
framework, coverage of the UI toolkit target or `app/`, or sanitizer
flags on the default build.

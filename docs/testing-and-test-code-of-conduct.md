#documentation #testing #quality

# Testing Guide And Test Code Of Conduct

Toolchain: [Building Locally](building-locally.md).

**Quick Setup:** run `time_tracker_core_tests` from a clean build. Keep tests
hermetic. Update contract tests in the same change when contracts move.

Deterministic, fast, behavior-oriented tests of `core/`. The Qt UI is
manual until a UI harness exists.

- framework: doctest
- target: `time_tracker_core_tests`
- dir: `tests/`
- default `TIME_TRACKER_BUILD_TESTS=ON`

```bash
cmake --preset ninja
cmake --build build --target time_tracker_core_tests
ctest --test-dir build --output-on-failure
```

Coverage is a separate GCC build so it does not need Qt:

```bash
cmake -S . -B build-cov -G Ninja \
  -DCMAKE_C_COMPILER=gcc \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_BUILD_TYPE=Debug \
  -DTIME_TRACKER_BUILD_TESTS=ON \
  -DTIME_TRACKER_BUILD_APP=OFF \
  -DCMAKE_C_FLAGS=--coverage \
  -DCMAKE_CXX_FLAGS=--coverage

cmake --build build-cov --target time_tracker_core_tests --parallel
ctest --test-dir build-cov --output-on-failure
```

`--exclude-throw-branches` drops compiler exception edges. Services return
`Result` and do not throw, so those edges are not decisions in the source.
The 80% line and 70% branch floors are unchanged.

```bash
gcovr -r . --object-directory build-cov \
  --filter "core/" \
  --exclude "build-cov/_deps/" \
  --exclude-directories "build-cov/_deps" \
  --exclude-throw-branches \
  --fail-under-line 80 \
  --fail-under-branch 70 \
  --print-summary
```

### Keep Tests Hermetic

No real network. No local machine files. Fakes where possible. Real disk
only with a unique temp dir and RAII cleanup. SQLite behavior tests use
`openMemory()`.

### Test Behavior, Not Internals

Assert externally visible outcomes. Avoid brittle implementation details.

### Keep Tests Deterministic

No unseeded randomness, timing-sensitive asserts, or ordering assumptions
unless ordering is the contract. Move `FakeClock` instead of sleeping.

### Keep Tests Readable

One intent per case. Descriptive names ("a clock jump is included while the entry stays open",
not `test_timer_2`). Arrange / act / assert.

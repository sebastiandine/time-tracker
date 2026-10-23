# AGENTS.md

Governs `.github/workflows/*.yml`. One top-level trigger per intent:
`feature-ci.yml` (non-`master` push), `master-ci.yml` (merged PR to `master`).
OS splits via `workflow_call`.

## Current workflow map

- `feature-ci.yml` — feature orchestrator. Linux, coverage, and ASan jobs are paused (`if: false`).
- `feature-linux.yml` — reusable Linux build/test. Caller is paused. `TIME_TRACKER_BUILD_APP=OFF`.
- `feature-windows.yml` — reusable Windows build/test, portable zip, and NSIS installer.
- `coverage.yml` — Linux GCC coverage gate (paused). `TIME_TRACKER_BUILD_APP=OFF`.
- `asan.yml` — Linux Clang ASan and UBSan (paused). `TIME_TRACKER_BUILD_APP=OFF`.
- `master-pr-title-guard.yml` — semver prefix on PRs to `master`
- `master-ci.yml` — version, Windows build/test/package, GitHub release. Linux, coverage, and ASan jobs are paused (`if: false`) and are not release gates.
- `master-linux.yml` — reusable Linux build/test for master. Caller is paused. `TIME_TRACKER_BUILD_APP=OFF`.
- `master-windows.yml` — reusable Windows build/test, portable zip, and NSIS installer for the release.

## CI invariants (do not break)

1. Feature: branch+sha versions. Master: semver tags.
2. Do not duplicate top-level triggers for the same intent.
3. Master tagging gated to merged PRs only.
4. PR title prefixes stay aligned with `scripts/compute_master_semver.sh`.
5. No MSVC-specific build commands.
6. Windows CI installs MSYS2 UCRT64 Qt 6 and NSIS, configures with `-DTIME_TRACKER_BUILD_APP=ON`, and uploads `time-tracker-windows-<version>.zip` plus `time-tracker-windows-installer-<version>`. Master releases attach the zip and `time-tracker-windows-installer-<version>.exe`.
7. Linux CI configures with `-DTIME_TRACKER_BUILD_APP=OFF`.

## Editing rules

Reusable workflows: `workflow_call`, `permissions: contents: read`.
Master orchestrator: `contents: write` for tagging. Linux: bash. Windows:
`shell: msys2 {0}` after `msys2/setup-msys2`.

After topology changes, update `docs/ci-cd-guide.md` and this map. After
prefix policy changes, update the guard, `compute_master_semver.sh`, and
`docs/versioning.md`. After artifact or installer changes, update
`docs/windows-installer.md` and `docs/versioning.md`. Do not add a second
feature or master trigger. Do not add a second packaging path; the Windows
bundle is `scripts/installer.nsi` plus the portable zip from `build/bin`.

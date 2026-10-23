#documentation #ci-cd #github-actions

# CI/CD Guide

Versioning policy: [Versioning Guide](versioning.md).

**Quick Setup:** protect `master` with required checks
`Require semver prefix in PR title` and Windows build + tests.
Linux build + tests, the Linux coverage gate, and Linux ASan + UBSan are
paused (`if: false` on the orchestrator jobs). Drop those names from required
checks while they are paused, or merges wait on checks that never run.

## Workflow Map

- `feature-ci.yml`: non-`master` pushes. Windows feature build runs. Linux build, coverage, and ASan are paused.
- `feature-linux.yml`: reusable Linux build/test. Not called while that job is paused. Configures with `-DTIME_TRACKER_BUILD_APP=OFF`.
- `feature-windows.yml`: reusable Windows build, test, portable zip, and NSIS installer.
- `coverage.yml`: GCC line and branch gate for `core/`. Paused. Configures with `-DTIME_TRACKER_BUILD_APP=OFF`.
- `asan.yml`: Linux Clang address and undefined-behavior sanitizers. Paused. Configures with `-DTIME_TRACKER_BUILD_APP=OFF`.
- `master-pr-title-guard.yml`: PR title prefixes for PRs targeting `master`.
- `master-ci.yml`: merged PRs to `master`; computes semver, runs the Windows build, and publishes a GitHub release. Linux build, coverage, and ASan are paused and are not release gates.
- `master-linux.yml`: reusable Linux build/test for master. Not called. Configures with `-DTIME_TRACKER_BUILD_APP=OFF`.
- `master-windows.yml`: reusable Windows build, test, portable zip, and NSIS installer for the release.

Windows CI installs MSYS2 UCRT64 Qt 6 (`mingw-w64-ucrt-x86_64-qt6-base`) and NSIS, and configures with `-DTIME_TRACKER_BUILD_APP=ON`. Installer details: [Windows Installer](windows-installer.md).

## Artifacts

| Artifact | Format |
|---|---|
| `time-tracker-windows-<version>.zip` | Portable Windows bundle (`time-tracker.exe`, Qt, MinGW runtime) |
| `time-tracker-windows-installer-<version>` | NSIS installer on the Actions run |
| `time-tracker-windows-installer-<version>.exe` | NSIS installer attached to the master GitHub release |

## Version Flow

Feature: `<branch-name>-<short-sha>` via `scripts/compute_feature_version.sh`,
passed as `-DTIME_TRACKER_APP_VERSION=...`.

Master: next semver from latest `vMAJOR.MINOR.PATCH` tag and merged PR title
via `scripts/compute_master_semver.sh`. CI tags `v<version>`.
Prefixes: `major`, `minor`, `fix`, `patch`, `path` (patch alias).

## Master Merge Guard

PRs targeting `master` must start with an accepted prefix or
`master-pr-title-guard.yml` fails.

## Release Procedure

1. Open a PR to `master` with a valid prefix (`major:`, `minor:`, `fix:`, `patch:`).
2. Merge. Master CI computes the version, builds and tests on Windows, and tags a GitHub release that includes the portable zip and the NSIS installer.

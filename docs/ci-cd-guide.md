#documentation #ci-cd #github-actions

# CI/CD Guide

Versioning policy: [Versioning Guide](versioning.md).

**Quick Setup:** protect `master` with required checks
`Require semver prefix in PR title`, Linux build + tests, Windows build + tests,
Linux coverage gate, and Linux ASan + UBSan.

## Workflow Map

- `feature-ci.yml`: non-`master` pushes; Linux and Windows feature builds, coverage, and ASan.
- `feature-linux.yml` / `feature-windows.yml`: reusable build/test.
- `coverage.yml`: GCC line and branch gate for `core/`.
- `asan.yml`: Linux Clang address and undefined-behavior sanitizers.
- `master-pr-title-guard.yml`: PR title prefixes for PRs targeting `master`.
- `master-ci.yml`: merged PRs to `master`; computes semver, builds, tags.
- `master-linux.yml` / `master-windows.yml`: reusable build/test for master.

CI configures with `-DTIME_TRACKER_BUILD_APP=OFF`. The desktop app is not
built in CI. This skeleton stops at build and test.

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
2. Merge. Master CI computes the version, builds, tests, and tags.

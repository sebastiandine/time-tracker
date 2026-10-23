# AGENTS.md

Governs `.github/workflows/*.yml`. One top-level trigger per intent:
`feature-ci.yml` (non-`master` push), `master-ci.yml` (merged PR to `master`).
OS splits via `workflow_call`.

## Current workflow map

- `feature-ci.yml` — feature orchestrator
- `feature-linux.yml` / `feature-windows.yml` — reusable build/test
- `coverage.yml` — Linux GCC coverage gate
- `asan.yml` — Linux Clang ASan and UBSan
- `master-pr-title-guard.yml` — semver prefix on PRs to `master`
- `master-ci.yml` — version, build/test, tag
- `master-linux.yml` / `master-windows.yml` — reusable build/test for master

## CI invariants (do not break)

1. Feature: branch+sha versions. Master: semver tags.
2. Do not duplicate top-level triggers for the same intent.
3. Master tagging gated to merged PRs only.
4. PR title prefixes stay aligned with `scripts/compute_master_semver.sh`.
5. No MSVC-specific build commands.
6. Configure with `-DTIME_TRACKER_BUILD_APP=OFF`. CI does not install Qt.

## Editing rules

Reusable workflows: `workflow_call`, `permissions: contents: read`.
Master orchestrator: `contents: write` for tagging. Linux: bash. Windows:
`shell: msys2 {0}` after `msys2/setup-msys2`.

After topology changes, update `docs/ci-cd-guide.md` and this map. After
prefix policy changes, update the guard, `compute_master_semver.sh`, and
`docs/versioning.md`. Do not add a second feature or master trigger. Do not
add packaging, installers, or extra scanners unless the user asked.

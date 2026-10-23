#documentation #versioning #releases

# Versioning Guide

Workflow wiring: [CI/CD Guide](ci-cd-guide.md).

**Quick Setup:** choose a valid PR title prefix before opening a `master` PR.

- Feature (non-`master`): `<branch-name>-<short-sha>` (sanitized lowercase
  branch, 7-char SHA). `scripts/compute_feature_version.sh`. Used as
  `TIME_TRACKER_APP_VERSION`.
- Master: `MAJOR.MINOR.PATCH` from latest semver tag and PR title.
  `major...` bumps MAJOR; `minor...` bumps MINOR; `fix` / `patch` / `path`
  bump PATCH. `scripts/compute_master_semver.sh`. First release starts at
  `0.0.0`. Tag format `v<semantic-version>`.
- Embedded `TIME_TRACKER_APP_VERSION`: feature CI uses
  `<branch>-<sha>`; master CI uses semver; local is
  `${PROJECT_VERSION} (localbuild)` unless overridden.
- That same string is the `<version>` in Windows artifact names. Feature
  runs upload `time-tracker-windows-<version>.zip` and
  `time-tracker-windows-installer-<version>`. Master releases attach
  `time-tracker-windows-<version>.zip` and
  `time-tracker-windows-installer-<version>.exe`. See
  [Windows Installer](windows-installer.md).

PR titles: `major: <summary>`, `minor: <summary>`, `fix: <summary>`,
`patch: <summary>`.

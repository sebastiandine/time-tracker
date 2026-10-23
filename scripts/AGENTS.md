# AGENTS.md

- `compute_feature_version.sh` — branch+sha for feature CI.
- `compute_master_semver.sh` — semver bump for merged PRs to `master`.
- `collect_windows_runtime.sh` — copies UCRT64 DLLs next to `time-tracker.exe` before packaging.
- `verify_windows_runtime.sh` — fails the Windows job when the portable bundle cannot load.
- `installer.nsi` — NSIS installer. CI runs `makensis -DAPP_VERSION="${VERSION}" scripts/installer.nsi` from both Windows workflows.

Prefer script files over large inline workflow scripts. Deterministic,
non-interactive, fail-fast on bad titles. Shell scripts stay LF.
When installer behavior changes, keep `feature-windows.yml` and `master-windows.yml` on that `makensis` invocation.

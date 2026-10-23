# AGENTS.md

- `compute_feature_version.sh` — branch+sha for feature CI.
- `compute_master_semver.sh` — semver bump for merged PRs to `master`.

Prefer script files over large inline workflow scripts. Deterministic,
non-interactive, fail-fast on bad titles. Shell scripts stay LF.

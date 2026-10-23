# AGENTS.md

Long-form contributor documentation outside the source tree.

## Files

- `README.md` — index.
- `intro-to-new-developers.md` — onboarding.
- `building-locally.md` — local build for Windows and Linux.
- `testing-and-test-code-of-conduct.md` — test workflow and standards.
- `ci-cd-guide.md` — CI workflows, merge guards, versioning in CI.
- `versioning.md` — feature-build version format and master semver rules.
- `windows-installer.md` — NSIS installer and the portable Windows zip.
- `time-tracking.md` — timer survival, notes, and statistics.
- `database/schema.md` — tables, checks, and indexes.

## Conventions

- Contributor reference, not release notes. Precise. C++20 examples with real paths.
- Do not describe hypothetical features or unmerged branches as current behavior.

## Required follow-ups

- After adding a file under `docs/` add it to the list above **and** to `README.md`.
- After changing workflow topology or version policy, update `ci-cd-guide.md` and `versioning.md`.
- Do not rename this file without updating every other `AGENTS.md` that points at it.

## Anti-patterns

- Don't duplicate per-package `AGENTS.md` content; cross-link instead.

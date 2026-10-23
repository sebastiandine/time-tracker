# C++ Project Setup

Cursor Agent Skill for scaffolding a **general C++20 CMake repository**:
toolchain, FetchContent, contributor docs, nested `AGENTS.md`, and a GitHub
Actions CI skeleton.

This is the human-readable companion to [`SKILL.md`](SKILL.md). Agents follow
`SKILL.md` only; every template lives in that file.

## Scope

The skill sets up a library, an executable, or both. It does **not** assume a
UI toolkit. If a later project has a UI, that toolkit is its own target and
logic code must not include its headers.

It does **not** generate sample domain types, a sample window, or an installer.

## When to apply

Use this skill when starting a new C++ repo, or when an existing repo needs
this CMake / docs / AGENTS / CI layout and is missing those pieces.

Trigger phrases: C++ project setup, CMake, AGENTS.md, contributor docs, CI
skeleton.

## Conventions

| Area | Rule |
|---|---|
| Language | C++20, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF` |
| CMake | 3.22+, `FetchContent`, explicit source lists (no `GLOB`) |
| Toolchain | Clang first. MinGW-w64 GCC on Windows if Clang is missing. GCC on Unix if Clang is missing. No MSVC. Search `clang` / `clang++` only (not `clang-cl`). Honor `CMAKE_CXX_COMPILER` if already set. |
| Layout | One directory per first-party target. Public headers in `core/include/<namespace>/`, implementations in `core/src/`. Optional `app/` for an executable. |
| Warnings | INTERFACE library: `-Wall -Wextra -Wpedantic -Wshadow` and the rest of the listed set. Not `-Wconversion` / `-Wsign-conversion`. No unused MSVC flags. |
| Dependencies | Pin `GIT_TAG` (never `master`), `GIT_SHALLOW TRUE`, `FETCHCONTENT_UPDATES_DISCONNECTED ON`, `CMAKE_POLICY_VERSION_MINIMUM 3.5` for CMake 4 |
| Tests | `PREFIX_BUILD_TESTS` default ON. doctest 2.4.11 fetched only then. Hermetic tests. |
| Docs | Index in `docs/README.md`. New docs listed there **and** in `docs/AGENTS.md`. Build guide is `docs/building-locally.md`. |
| AGENTS.md | Root file plus one nested file per directory agents edit. Domain rules only when the code exists. |
| Git (agents) | No commit, push, or branch-switch-to-edit unless the user explicitly asked. |
| CI | One orchestrator per intent (`feature-ci.yml`, `master-ci.yml`). OS splits via `workflow_call`. Linux: Clang + Ninja. Windows: MSYS2 UCRT64 GCC + Ninja. Feature `branch-sha` versions; master semver from PR title. Build and test only. |

Also created with the project:

- `CMakePresets.json` (schema version 3: `ninja`, `ninja-debug`, `mingw`)
- `.clang-format` (4-space indent, attached braces, pointer/reference on the left, column 100)
- root `compile_commands.json` copy for clangd (gitignored; refresh after configure)
- `.gitattributes` so shell scripts and workflows stay LF on Windows
- GitHub Actions skeleton (build and test; no packaging)

## Files

| File | Audience |
|---|---|
| [`SKILL.md`](SKILL.md) | Agent procedure and all templates |
| [`README.md`](README.md) | This human explanation |

Placeholders in `SKILL.md`:

- `PROJ` — `project()` name and target stem
- `PREFIX` — CMake option prefix (`PREFIX_BUILD_TESTS`, `PREFIX_APP_VERSION`)
- `NS` — C++ namespace

Agents must substitute real names. Do not leave tokens in the new project.

## Using the skill in another repo

This copy lives only in this skills repository
(`.cursor/skills/cpp-project-setup/`). Cursor loads **project** skills from
the repo you have open, and **personal** skills from `~/.cursor/skills/`.

To use it while working in other C++ projects, copy the directory:

```text
.cursor/skills/cpp-project-setup/  →  ~/.cursor/skills/cpp-project-setup/
```

On Windows the personal skills directory is typically
`C:\Users\<you>\.cursor\skills\`. Do not copy it into
`~/.cursor/skills-cursor/` (that tree is reserved for Cursor's built-in
skills).

After copying, a later agent session can pick the skill up from the
description (new C++ project, CMake setup, AGENTS.md, contributor docs, CI
skeleton) or you can mention it by name.

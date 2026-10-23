# C++ JSON Files

Cursor Agent Skill for **reading and writing JSON files** in a C++20
repository: nlohmann/json 3.11.3, hand-rolled `to_json` / `from_json`, a
`schemaVersion`, unknown keys kept across a save, and a durable replace
(temp file, fsync, then replace).

This is the human-readable companion to [`SKILL.md`](SKILL.md). Agents
follow `SKILL.md`, which includes the copy-paste implementation.

## Scope

The skill stores **documents on disk**. It does **not** cover HTTP or REST:
no requests, status codes, authentication, or response bodies. Parsing a
string that `readText` already returned is in scope. Fetching that string
is not.

It assumes the `cpp-project-setup` layout and a `Result` error type. It
does **not** re-scaffold CMake, the filesystem port, or the test harness.
It does **not** add a second JSON library.

## When to apply

Use this skill when adding or changing a JSON file, `to_json` / `from_json`,
a schema version, or code that loads or saves config or records.

Trigger phrases: JSON file, nlohmann, `to_json`, `from_json`,
`schemaVersion`, serialize, deserialize, `config.json`.

If the task is an HTTP client, do not use this skill.

## Conventions

| Area | Rule |
|---|---|
| Library | nlohmann/json **v3.11.3**, FetchContent, `JSON_BuildTests` and `JSON_Install` OFF. Linked PUBLIC on the logic target. |
| Serde | Hand-rolled `to_json` / `from_json` in the type's namespace. No `NLOHMANN_DEFINE_TYPE_*` and no `NLOHMANN_JSON_SERIALIZE_ENUM`. |
| Headers | Domain headers include `nlohmann/json_fwd.hpp` only. The full `json.hpp` stays in `.cpp` files and in `DocumentIo.hpp`. |
| Keys | Default `nlohmann::json` (sorted keys). Enums are strings. Required keys use `.at()`. Newer optional keys use `.value(key, default)`. |
| Aliases | The JSON key is written out when it differs from the member. A C++ keyword keeps a trailing underscore on the member only (`signed_` ↔ `"signed"`). |
| Paths | Values stored in JSON use `path::generic_string()` so separators are `/`. |
| Shape | A record is one object of named keys. An array, or an object whose keys are ids, sits under `data` next to `schemaVersion`. |
| Version | Missing `schemaVersion` means version 1, then migrations run up to the current constant. A newer number is rejected. Bump only for a breaking change. |
| Unknown keys | Each object keeps keys it does not recognize and writes them back. Migrations erase keys they consume so a rename does not come back. |
| Write | `dump(2)` plus one trailing newline. Write `*.tmp` beside the file, fsync, then replace. Never truncate the destination in place. |
| Windows | Replace with `MoveFileExW` (`MOVEFILE_REPLACE_EXISTING \| MOVEFILE_WRITE_THROUGH`). `std::filesystem::rename` will not replace an existing file there. |
| Errors | `Io`, `Malformed`, `Type`, `MissingKey`. File-not-found is `Io`. A missing required key is `MissingKey`. Callers branch on `kind`. |
| Boundary | Serde may throw. `loadJson` / `storeJson` catch and return `Result`. Domain code does not open files itself. |
| Tests | Round-trip, key spellings, unknown key still in the file text, defaults, version fixture, and one case per error kind. A failed replace leaves the previous bytes. |
| Out of scope | HTTP, REST, comment-tolerant JSON, `ordered_json`, embedding file bytes in the document. |
| Git (agents) | No commit, push, or branch-switch-to-edit unless the user explicitly asked. |

## Why the write path is a replace

Opening the destination with truncation destroys the previous document
before the new bytes are finished. A crash or a full disk then leaves a
short file that will not parse, and the old copy is gone.

`replaceText` writes a sibling temp file, flushes and fsyncs it, then
replaces the destination. Until that replace succeeds, readers still see
the old file. `loadJson` ignores a leftover `*.tmp`.

On POSIX the replace is `rename`, followed by an fsync of the parent
directory. If that directory fsync fails, the new file is already in
place; the error text says `after replace`, and saving again is safe.

## Why version and unknown keys are separate

`schemaVersion` is for changes a default cannot express: renames, removals,
retyped values, or wrapping a bare array in an envelope. Adding a key that
old files can omit, with `value(key, default)`, does not bump the number.

Unknown keys are the opposite case: something this build does not know.
They are kept so a save does not strip them. They are not a substitute for
a migration. A migration that renames a key must delete the old key before
unknown keys are collected.

## Files

| File | Audience |
|---|---|
| [`SKILL.md`](SKILL.md) | Agent procedure, invariants, record pattern, and the copy-paste implementation (`JsonFileError`, `JsonObjectBag`, `loadJson` / `storeJson`, `replaceText`, CMake pin) |
| [`README.md`](README.md) | This human explanation |

Placeholders in `SKILL.md`:

- `PROJ` — `project()` name and target stem
- `PREFIX` — CMake option prefix
- `NS` — C++ namespace

Agents must substitute real names. Do not leave tokens in the project.

## Using the skill in another repo

This copy lives only in this skills repository
(`.cursor/skills/cpp-json-files/`). Cursor loads **project** skills from
the repo you have open, and **personal** skills from `~/.cursor/skills/`.

To use it while working in other C++ projects, copy the directory:

```text
.cursor/skills/cpp-json-files/  →  ~/.cursor/skills/cpp-json-files/
```

On Windows the personal skills directory is typically
`C:\Users\<you>\.cursor\skills\`. Do not copy it into
`~/.cursor/skills-cursor/` (that tree is reserved for Cursor's built-in
skills).

After copying, a later agent session can pick the skill up from the
description (JSON file, nlohmann, `to_json`, `schemaVersion`) or you can
mention it by name.

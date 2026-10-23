# C++ Documentation

Cursor Agent Skill for documenting a **C++20** repository at three altitudes:
Doxygen on header declarations, in-body comments for complex logic,
business-logic and data-flow pages under `docs/`, and nested `AGENTS.md`
maps.

This is the human-readable companion to [`SKILL.md`](SKILL.md). Agents follow
`SKILL.md` only; every template lives in that file.

## Scope

The skill is the ongoing documentation standard. It does **not** scaffold
CMake, CI, or the starter `docs/` / `AGENTS.md` tree (that is project
setup). It does **not** generate Doxygen HTML unless you ask.

It does **not** require a comment on every line or every getter. Trivial
accessors, defaulted special members, and obvious comparisons stay silent
when the type block already states the contract.

## When to apply

Use this skill when writing or changing C++ functions, methods, classes,
header comments, `docs/` markdown, data flows, or `AGENTS.md`.

Trigger phrases: Doxygen, header comments, `docs/`, business logic, data
flow, nested `AGENTS.md`.

## Conventions

| Area | Rule |
|---|---|
| Altitudes | Header = contract of one type or function. Body = why a complex block is shaped that way. `docs/` = cross-module flow. `AGENTS.md` = map and invariants. Point across layers; do not paste the same explanation into all of them. |
| Header comments | `/** */` immediately above the declaration. `@brief`, `@param` per parameter, `@return` when non-void, `@tparam` when templated. Extra tags (`@pre`, `@post`, `@throws`, `@note`) only when ownership, lifetime, threading, error policy, units, or sentinels are not obvious. No empty tags. |
| Where | Public and protected contracts in the **header**. Do not repeat Doxygen on the `.cpp` definition. |
| Skip | Trivial accessors, defaulted special members, obvious comparisons, file banners that repeat the class block. |
| In-body | Plain `//` above a complex algorithm, non-obvious branch, hidden invariant, or fragile ordering. Explain why. Do not narrate every statement. |
| Backfill | If a change edits a declaration that should already be documented and is not, add the block in the same change. |
| `docs/` | One topic per file. Purpose, participants (real paths), flow, data shape, invariants, out of scope. Mermaid when the sequence has more than a few steps. List new files in `docs/README.md` **and** `docs/AGENTS.md`. |
| `AGENTS.md` | Root is a map (purpose, directory pointers, cross-package invariants, commands). Nested files cover that directory only and start with "read the root first." Long behavior goes in `docs/` with a pointer. |
| Same change | Update the matching comment, page, and `AGENTS.md` when the code they describe changes. Stale documentation is worse than none. |
| Git (agents) | No commit, push, or branch-switch-to-edit unless the user explicitly asked. |

## Files

| File | Audience |
|---|---|
| [`SKILL.md`](SKILL.md) | Agent procedure and all examples |
| [`README.md`](README.md) | This human explanation |

`NS` and `PROJ` in `SKILL.md` examples are stand-ins for the project's
namespace and target stem. Agents must use real names.

## Using the skill in another repo

This copy lives only in this skills repository
(`.cursor/skills/cpp-documentation/`). Cursor loads **project** skills from
the repo you have open, and **personal** skills from `~/.cursor/skills/`.

To use it while working in other C++ projects, copy the directory:

```text
.cursor/skills/cpp-documentation/  →  ~/.cursor/skills/cpp-documentation/
```

On Windows the personal skills directory is typically
`C:\Users\<you>\.cursor\skills\`. Do not copy it into
`~/.cursor/skills-cursor/` (that tree is reserved for Cursor's built-in
skills).

After copying, a later agent session can pick the skill up from the
description (Doxygen, header comments, `docs/`, data flows, nested
`AGENTS.md`) or you can mention it by name.

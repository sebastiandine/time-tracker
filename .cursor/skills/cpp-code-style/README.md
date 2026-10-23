# C++ Code Style

Cursor Agent Skill for keeping **C++20** readable and maintainable: low
cyclomatic complexity, shallow nesting, extracting a helper only when the
rule is the same, and one home for shared behavior.

This is the human-readable companion to [`SKILL.md`](SKILL.md). Agents
follow `SKILL.md` only; every example lives in that file.

## Scope

The skill is the ongoing structure standard. It does **not** set
whitespace, brace style, or include sorting; those live in
`cpp-project-setup` (`.clang-format`). It does **not** set header-comment or
in-body comment rules; those live in `cpp-documentation`.

It does **not** require splitting an untouched large function in the same
change as unrelated work. An edit that would grow a function already over
the caps must extract instead.

## When to apply

Use this skill when writing or restructuring C++ functions, methods,
classes, utilities, or shared helpers.

Trigger phrases: readability, structure, cyclomatic complexity, nesting,
duplication, DRY, shared helper, `util`, maintainable.

## Conventions

| Area | Rule |
|---|---|
| Caps | Cyclomatic complexity ≤ 10, nesting ≤ 3, body about one screen (~50 lines). Count starts at 1; add 1 for each `if`, `else if`, `for`, `while`, `case`, `catch`, `&&`, `||`, and `?:`. |
| Exception | Exceed a cap only for one cohesive dispatch (closed `switch`, tree walk, template base) whose branches stay thin. Split a function that mixes steps. |
| Guards | Invalid, empty, and failed `Result` return first. The main path stays flat. |
| Abstraction | An orchestrator only calls named steps. A step does one of validate, parse, I/O, or UI update. |
| Size by layer | Core functions follow the caps. A UI type may be longer if layout, populate, and handlers are separate functions. The constructor wires them. |
| DRY | A second copy of the **same rule** becomes one helper. Similar code with **different rules** stays separate; share only the skeleton. No utility for a single caller. |
| Variants | Keep an explicit function per variant when the rules differ. |
| Placement | One `.cpp`: anonymous namespace. Several files in one library: internal header under `src/` in a detail namespace. Another target: public header in that module's `util/` (or the owning type). Free functions, not a `Utils` class. |
| Booleans | More than one `bool`, or a `bool` that is unclear at the call site, becomes an `enum class`. |
| Parameters | More than four parameters, or a call the reader must count through, becomes a small struct. A persistence boundary that mirrors stored columns may keep the long list. |
| Magic | Numbers and stringly modes become `constexpr` or `enum class`. |
| Names | `camelCase` functions, `PascalCase` types, trailing-underscore members, `I` only on interfaces, `final` on adapters that must not be derived. |
| Class order | Public, then protected, then private. |
| Headers | `#pragma once`. One primary type per header. Matching header first in the `.cpp`. Forward-declare when only a pointer or reference is used. |
| Git (agents) | No commit, push, or branch-switch-to-edit unless the user explicitly asked. |

## Why same-rule DRY is not "no duplication"

Two functions can look alike and still encode different business rules.
Folding them into one helper means a change to one rule silently changes
the other. Extract when the copies will change together. When only the
shape matches, extract the skeleton and leave one function per variant.

A helper with a single caller is noise. Wait until a second site needs
the same rule, then place it where those callers can see it.

## Why the caps are soft

A closed `switch` on a known set of cases, a window-tree walk, or a
template base that owns shared machinery can run past the numbers and
still be the form a reader expects. The branches must stay thin. A
function that is long because it **sequences** unrelated steps is not
that case; name the steps and call them.

## Files

| File | Audience |
|---|---|
| [`SKILL.md`](SKILL.md) | Agent procedure and all examples |
| [`README.md`](README.md) | This human explanation |

## Using the skill in another repo

This copy lives only in this skills repository
(`.cursor/skills/cpp-code-style/`). Cursor loads **project** skills from
the repo you have open, and **personal** skills from `~/.cursor/skills/`.

To use it while working in other C++ projects, copy the directory:

```text
.cursor/skills/cpp-code-style/  →  ~/.cursor/skills/cpp-code-style/
```

On Windows the personal skills directory is typically
`C:\Users\<you>\.cursor\skills\`. Do not copy it into
`~/.cursor/skills-cursor/` (that tree is reserved for Cursor's built-in
skills).

After copying, a later agent session can pick the skill up from the
description (readability, cyclomatic complexity, DRY, shared helpers) or
you can mention it by name.

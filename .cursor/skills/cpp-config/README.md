# C++ Config

Cursor Agent Skill for **C++20 runtime configuration**: a cascade of
compiled defaults, JSON or properties files, `.env` files, process
environment variables, and command-line flags; nested overlay; documented
keys; secret redaction; and failure on unknown keys.

This is the human-readable companion to [`SKILL.md`](SKILL.md). Agents
follow `SKILL.md`; the copy-paste schema comment, README catalog, and
examples live in that file.

## Scope

The skill is the runtime **settings** surface: where files live, which
layer wins, how keys are named, how secrets stay out of committed files,
and how the app documents the catalog.

It is **not** domain data (collections, catalogs, or other documents the
app stores). It is **not** compile-time build options. It does **not**
open storage, talk to a GUI toolkit's own settings store, or add a second
JSON library.

JSON is the default file format. A properties file is used only when the
repository already uses properties. One format per app. `.env` is always
a cascade layer on top of that file.

## When to apply

Use this skill when adding or changing `config.json`, a properties file,
`.env`, `getenv`, environment variables, CLI flags that override
settings, a typed config struct, or config documentation.

Trigger phrases: config, configuration, settings, `config.json`,
`.properties`, `.env`, environment variable, cascade, overlay, override,
schema, secret, `APP_`, `--flag`.

## Conventions

| Area | Rule |
|---|---|
| Shape | One typed struct. Every field lists type, default or required, file key, env name, flag, and secret-or-not. No camelCase-to-snake guessing. |
| Cascade (low to high) | Compiled defaults, system file, user file, exe file, system `.env`, user `.env`, exe `.env`, process environment, CLI. Nested objects overlay per field. Every `.env` beats every config file. |
| Directories | System (`%ProgramData%/<AppName>/` or `/etc/<app-name>/`), user (`%APPDATA%/<AppName>/` or `$XDG_CONFIG_HOME` / `~/.config/<app-name>/`), executable directory. Never cwd. |
| Exe path | OS resolve (`/proc/self/exe`, `GetModuleFileNameW`, `_NSGetExecutablePath`) or the GUI toolkit's standard executable path. |
| Missing files | Skip. If no config file exists, create compiled defaults at the write location. Do not auto-create `.env`. |
| Unknown keys | Fail startup. Name the key and the layer. Process env: only the app prefix participates; other names are ignored; a prefixed unknown name fails. |
| Secrets | `.env` or process env only. A secret in a config file or on the CLI fails. Logs show `(set)` / `(missing)` and the layer, never the value. No CLI flag. |
| Save | File layer only. Exe directory if writable, else user directory, never system. Atomic temp + replace-rename (`ReplaceFileW` on Windows; `rename(2)` on POSIX). |
| Migration | Retired keys and values are mapped on load, then that file is rewritten in the current schema if it changed. |
| Load | Once at startup. No file watch. |
| Log | After load, every field and its winning layer, through the app logger or stderr. |
| Errors | Malformed, type mismatch, unknown key, forbidden secret, missing required: fail startup. Do not skip a bad file. |
| Docs | Struct comment, committed `config.example.json` and `.env.example`, gitignore for `config.json` / `.env`, README key catalog. |
| Tests | In-memory filesystem. Overlay, unknown keys, secrets, env not written back, migration, unwritable exe dir, log redaction. |
| GUI | Settings reads effective values, writes the file layer, and shows the winning layer when a higher layer still overrides. |
| Storage | A path field is handed to storage. The loader does not open a database. |
| Git (agents) | No commit, push, or branch-switch-to-edit unless the user explicitly asked. |

## Why a cascade instead of one file

A desktop app that only reads `config.json` next to the executable works
until someone needs a site-wide default, a user override that survives
reinstall, or a secret that must not sit in a JSON file. Putting those
concerns in extra ad-hoc parsers (`getenv` here, a second JSON there)
makes precedence impossible to explain.

The cascade is the whole rule. Defaults are compiled in. Files stack from
system to user to executable. Then dotenv, then the real process
environment, then the command line. Nested objects merge field by field
so a user file can change `window.height` without wiping `window.width`.

`.env` files sit above config files on purpose: they are the documented
place for secrets and for local overrides that should not be written back
into `config.json`. That also means a system `.env` beats an
exe-directory JSON file. Document that order in the app README; do not
"fix" it by reading dotenv first.

## Why unknown keys fail

Silent ignore looks convenient and hides typos (`typoTheme` never applies,
the default theme stays). Writing unknown keys back preserves forward
compatibility with newer files, but it also preserves mistakes forever.

This skill fails the process and names the key and the layer. Retired
names go through a documented migration map **before** that check, so a
rename is not reported as unknown. The process environment is the one
carve-out: only names with the app prefix are config; the rest of the
environment is not scanned.

## Why secrets never enter the config file or the CLI

A config file is copied, backed up, and often sits next to the
executable. Command-line flags appear in process lists and shell history.
`.env` (gitignored) and the process environment are the two layers that
operators already treat as secret-bearing.

The settings UI therefore writes JSON (or properties), not dotenv. The
effective log redacts: `(set)` or `(missing)`, plus the winning layer.
Example files may list the env name; they must not contain a real token.

## Why the write location falls back, and why replace is atomic

The executable directory is the natural place for a portable install.
When it is not writable (a system install, a read-only image), the user
directory is the fallback. The system directory is input only: writing
there from a user session is a privilege and a surprise.

`std::filesystem::rename` does not replace an existing file on Windows. A
`remove` then `rename` leaves a window with no config. Write a temp file
in the same directory, then `ReplaceFileW` on Windows and `rename(2)` on
POSIX.

## Why env and CLI values stay out of the file

If `APP_THEME=Dark` wins, saving a new data directory from settings must
not stamp `Dark` into `config.json`. That would freeze an override that
the operator expected to be environmental. The service keeps a file layer
and an effective snapshot. `store` writes the file layer. Env and CLI are
applied again after the write.

## Why load once and log the winner

A settings UI and the rest of the process must see the same struct.
Watching the file invites half-applied reloads (storage still open on
the old path, UI still on the old theme). External edits take effect on
the next start.

Logging every field and its layer is how an operator confirms that a
system file, a user `.env`, or a CLI flag actually won. Without it, a
cascade is undebuggable.

## Files

| File | Audience |
|---|---|
| [`SKILL.md`](SKILL.md) | Agent procedure and all templates |
| [`README.md`](README.md) | This human explanation |

## Using the skill in another repo

This copy lives only in this skills repository
(`.cursor/skills/cpp-config/`). Cursor loads **project** skills from the
repo you have open, and **personal** skills from `~/.cursor/skills/`.

To use it while working in other C++ projects, copy the directory:

```text
.cursor/skills/cpp-config/  →  ~/.cursor/skills/cpp-config/
```

On Windows the personal skills directory is typically
`C:\Users\<you>\.cursor\skills\`. Do not copy it into
`~/.cursor/skills-cursor/` (that tree is reserved for Cursor's built-in
skills).

After copying, a later agent session can pick the skill up from the
description (config, `.env`, environment, cascade, settings) or you can
mention it by name.

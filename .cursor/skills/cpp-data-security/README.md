# C++ Data Security

Cursor Agent Skill for keeping **C++20** programs honest about secrets,
user data, and access: no credentials in source, per-user record
directories, confined app-built paths, bound external SQL, HTTPS, and
untrusted-file import.

This is the human-readable companion to [`SKILL.md`](SKILL.md). Agents
follow `SKILL.md`; the copy-paste examples live in that file.

## Scope

The skill is the ongoing **secrets, data-class, and access** standard. It
does **not** set memory ownership, thread joining, or sanitizer CI. It
does **not** encrypt ordinary user records: those files and tables must
not contain secrets, so they stay readable backups.

It does **not** require a particular database driver, HTTP client, GUI
toolkit, or JSON library. Bind, confine, and escape at the boundary the
project already uses.

## When to apply

Use this skill when adding or changing secrets, credentials, API keys,
config files, user data directories, caches, filesystem joins, database
statements, import, export, or outbound HTTP.

Trigger phrases: secret, credential, API key, `getenv`, gitignore, user
data, cache sidecar, path traversal, `lexically_normal`, bind parameter,
SQL injection, zip slip, markup escape, HTTPS, redirect.

## Conventions

| Area | Rule |
|---|---|
| Secrets in source | Never. Load from the environment or a gitignored file. Missing or empty is a hard fail. No fallback key. |
| Secret leakage | Never in logs, error strings, URLs (query or userinfo), user-data files, or install-beside config. |
| CI | Inject from the platform secret store only. Do not paste values into workflow files. |
| Supply chain | Pin by git tag or URL plus content hash. No floating branch. License must match the repo before adding a dependency. |
| User records | Default to a per-user directory (home or the OS user-data folder), not beside the executable. Durable write: temp, flush, replace. Schema documented in the same change. Unencrypted. |
| Config | Beside the install only if it holds no secrets **and** that directory is writable without elevation. Otherwise the per-user directory. |
| Derived cache | Separate root outside the user-data directory. Best-effort I/O. A non-cryptographic filename hash needs an exact-key sidecar; mismatch is a miss. |
| Elevation | The process does not require it. Do not write config, cache, or records into a directory that needs elevation. |
| Path grant | A GUI file picker or an explicit settings path is the user's grant. Do not re-root it. |
| App-built paths | One root. Each fragment is a single filename component. `lexically_normal`, reject escape. `weakly_canonical` rejects a symlink that leaves the root. |
| Database | External values are bound parameters. A fixed table name or a row id the program just received may be concatenated. |
| Import | Cap **32 MiB**. Parse known entry names in memory. Never write a path taken from the file. |
| Export | Escape `&`, `<`, `>`, `"`, `'`. Drop other control characters (keep tab, newline, CR). |
| HTTP | HTTPS except loopback. Percent-encode user text in URLs. No secret in the URL. A request that carries a secret does not follow a redirect onto another host. |
| Git (agents) | No commit, push, or branch-switch-to-edit unless the user explicitly asked. |

## Why secrets fail closed

A hard-coded key is a secret that is already public: it is in git, in
every clone, and in every CI log that prints a failed URL. Loading from
the environment (or a gitignored file) keeps the value off the tree. If
it is missing, continuing with a dummy key hides a misconfiguration until
production. Naming the variable in the error, not the value, is enough
for the operator to fix it.

User-data files and install-beside config are backups and support
artifacts. Putting a token there means the next zip of the data directory
is a credential dump.

## Why three roots

User records are what the person cares about and what they back up. They
belong in a per-user directory so an install into a protected folder
(Program Files, `/usr`) does not require elevation to save, and so
replacing the binary does not replace the records.

Config that only stores paths and last-used ids can sit next to the
executable because it is not secret and not a backup of the user's work.
If writing there needs elevation, it is not a config location.

Derived caches (downloaded images, computed indexes) must not travel with
those backups and must not move when the user relocates their data
directory. A separate root gives that for free. Best-effort writes mean a
full disk cannot take down the feature that the cache only accelerates.

A short hash in a filename is not identity. Without a sidecar that stores
the exact key, a collision (or a hand-edited file) serves the wrong
payload. Treating a mismatch as a miss is the whole safety net; the hash
does not need to be cryptographic.

## Why confinement is not the file picker

`std::filesystem` will join `..` and follow a symlink. A name the program
builds from a record field is not a grant; it is an injection surface.
Normalizing under one root, rejecting separators inside a single
component, and refusing a canonical path outside that root is the check.

A path the user chose in a GUI picker, or typed as the data-directory
setting, **is** the grant. Re-rooting it would ignore where they asked to
store files.

## Why only external SQL is bound

Anything that originated outside the process (typed text, file bytes,
HTTP bodies) can carry SQL metacharacters. Bound parameters keep that
data out of the statement text.

A table name the program owns, or a row id it just read from its own
database, is not that case. Concatenating those internal values is
allowed so migrations and identifier-only pragmas stay readable. The
moment a value crossed a file, a socket, or a UI field, it is external
again.

## Why import stays in memory

An archive member named `../../somewhere` is a write to that path if the
importer uses the name as a destination. Reading only known entry names,
into memory, with a 32 MiB cap, removes that surface and bounds the
parse. Exporting the other way, user text must not close a tag or inject
an entity; escaping the five markup characters is the rule.

## Files

| File | Audience |
|---|---|
| [`SKILL.md`](SKILL.md) | Agent procedure and all examples |
| [`README.md`](README.md) | This human explanation |

## Using the skill in another repo

This copy lives only in this skills repository
(`.cursor/skills/cpp-data-security/`). Cursor loads **project** skills from
the repo you have open, and **personal** skills from `~/.cursor/skills/`.

To use it while working in other C++ projects, copy the directory:

```text
.cursor/skills/cpp-data-security/  →  ~/.cursor/skills/cpp-data-security/
```

On Windows the personal skills directory is typically
`C:\Users\<you>\.cursor\skills\`. Do not copy it into
`~/.cursor/skills-cursor/` (that tree is reserved for Cursor's built-in
skills).

After copying, a later agent session can pick the skill up from the
description (secret, credential, user data, path confinement, bind,
import, HTTPS) or you can mention it by name.

# C++ REST Client

Cursor Agent Skill for **consuming REST APIs** from a C++20 CMake
repository: libcpr behind a port, structured errors with bounded retries,
header authentication, pagination, HTTPS, payload limits, and hand-rolled
JSON parse of response bodies.

This is the human-readable companion to [`SKILL.md`](SKILL.md). Agents
follow `SKILL.md` only; every template lives in that file.

## Scope

The skill is for an HTTP **client**. The program calls someone else's API.
It does **not** serve HTTP, and it does **not** persist JSON documents
(that is `cpp-json-files`).

It assumes the `cpp-project-setup` layout (`core/`, `app/`,
`cmake/Dependencies.cmake`) and the project's `Result` type. Tests follow
`cpp-unit-testing`: unit tests, fakes, no live network. If a GUI blocks on
a request, the GUI skill owns the worker-thread / UI-thread-post pattern
(`cpp-wxwidgets-ui` is the current GUI skill); this skill only points at
it.

It does **not** re-scaffold the repo, add MSVC, or introduce a second HTTP
or JSON library.

## When to apply

Use this skill when adding or changing outbound HTTP, a REST mapping, the
cpr adapter, Bearer or API-key headers, pagination, or conditional GET.

Trigger phrases: REST, HTTP, `cpr`, JSON API, Bearer, pagination, ETag,
`IHttpClient`.

If the task is only reading or writing a JSON **file**, use
`cpp-json-files` instead.

## Conventions

| Area | Rule |
|---|---|
| Port | `IHttpClient::execute(HttpRequest)` in `core/include/NS/ports/`. Callers never include `cpr/`. |
| Adapter | `CprHttpClient` in infra. One `cpr::Session` for the process, mutex around every call, TLS verification on. |
| Methods | GET, HEAD, POST, PUT, PATCH, DELETE. GET/HEAD have no body. Filters belong in the query string. |
| Encode | Percent-encode query **values** (RFC 3986 unreserved set, space as `%20`) before `cpr::Url`. cpr does not encode the string it is given. |
| Success | 2xx and 304. Body is raw bytes. Do not JSON-parse an empty 304. |
| Errors | `HttpError`: `Transport`, `HttpStatus`, `Policy`. Status is kept. No exceptions across the port. |
| Retry | At most 3 attempts. GET/HEAD/PUT/DELETE (or `allowRetry`). Only transport, 429, and 503. Honor `Retry-After`, cap the wait at 10s. POST/PATCH retry only with `allowRetry` plus an idempotency key. |
| Auth | `Authorization: Bearer` or the documented API-key header. Never in the query, source, errors, or logs. 401/403 are not retried. |
| Redirects | At most 10. Do not resend credentials to another host. Keep method and body on 307/308. |
| Pagination | Caller follows that API's next link. Same filter/sort every page. Stop on no next, empty page, or an explicit cap. |
| Limits | Connect 5s, total 30s, body 16 MiB by default. Over the cap is `Policy`. |
| TLS | HTTPS only, except loopback `http`. No `user:pass@` in the URL. |
| Conditional | Store `ETag`, send `If-None-Match`; 304 reuses cache. `If-Match` on conflicting writes; 412 is not retried. |
| JSON | Project JSON library and hand-rolled serde (`cpp-json-files`). Parse in `try`/`catch`, static `parseX(body)` for tests. |
| Cache | Transient failures (transport, 429, 5xx) are never stored as a miss. A clean 404 may be. 401/403 are auth, not not-found. |
| CMake | FetchContent pin cpr **1.10.5**. In-tree curl, unused protocols off, Schannel on Windows. Link cpr only on the adapter's target. |
| Tests | Fake the port. Encode, map, redact, HTTPS guard, retry decisions, parse fixtures. No network. |
| Git (agents) | No commit, push, or branch-switch-to-edit unless the user explicitly asked. |

Placeholders in `SKILL.md`:

- `PROJ` — `project()` name and target stem
- `PREFIX` — CMake option prefix
- `NS` — C++ namespace

Agents must substitute real names. Do not leave those tokens in the
project.

## Why a port and one session

Core logic has to stay testable without libcpr or a socket. The port is
that seam. A fake returns a captured body; the mapper and JSON parse run
in the unit tests (`cpp-unit-testing`).

libcurl's easy handle (and therefore `cpr::Session`) is not thread-safe
and keeps its connection cache on that handle. A new session per call
throws the cache away and pays TLS again. One session, locked for each
`execute`, is the rule. `RemoveContent()` after a call stops a POST body
from riding along on the next GET.

## Why structured errors and a narrow retry

A string such as `"HTTP 404 from …"` forces every caller to parse text to
tell a miss from a timeout. `HttpError` keeps the kind and the status so
404, 401, and 429 branch explicitly.

Automatic retry is easy to get wrong. RFC 9110 treats GET, HEAD, PUT, and
DELETE as idempotent; POST and PATCH are not. Retrying a POST can create
two resources unless the caller sent an idempotency key and set
`allowRetry`. Retrying 401 or 404 does not help and can look like
credential stuffing. The adapter therefore retries only transport
failure, 429, and 503, at most three times, and waits at least
`Retry-After` when the server sent it.

## Why HTTPS, caps, and redaction

Cleartext `http` exposes tokens and bodies on the path. Loopback is the
only exception, for local stubs. Userinfo in the URL
(`https://user:pass@host`) ends up in logs, process lists, and error
strings; headers exist for credentials.

A huge or never-ending body can stall the UI thread's worker and blow
memory. The 16 MiB default is a floor the caller may raise when the API
documents a larger dump.

`Authorization` and secret query keys must not appear in `HttpError::message`.
The adapter redacts them **before** formatting the error. Tests assert the
absence, not only the status.

## Why JSON parse is separate from HTTP

URL construction, HTTP, and JSON mapping fail for different reasons.
`parseX(const std::string& body)` takes a fixture. `fetchAll` only
forwards `execute` and then parse. A broken preview or list is first
reproduced against a saved body; if that passes, the next suspect is the
encoded URL.

Hand-rolled serde (`cpp-json-files`) keeps JSON key aliases visible. A
macro hides them until a production payload disagrees with the struct.

## Files

| File | Audience |
|---|---|
| [`SKILL.md`](SKILL.md) | Agent procedure and all examples |
| [`README.md`](README.md) | This human explanation |

## Using the skill in another repo

This copy lives only in this skills repository
(`.cursor/skills/cpp-rest-client/`). Cursor loads **project** skills from
the repo you have open, and **personal** skills from `~/.cursor/skills/`.

To use it while working in other C++ projects, copy the directory:

```text
.cursor/skills/cpp-rest-client/  →  ~/.cursor/skills/cpp-rest-client/
```

On Windows the personal skills directory is typically
`C:\Users\<you>\.cursor\skills\`. Do not copy it into
`~/.cursor/skills-cursor/` (that tree is reserved for Cursor's built-in
skills).

After copying, a later agent session can pick the skill up from the
description (REST, HTTP, `cpr`, Bearer, pagination, ETag) or you can
mention it by name.

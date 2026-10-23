# C++ Code Style Security

Cursor Agent Skill for keeping **C++20** memory-safe and concurrency-safe:
RAII for every resource that needs an explicit release, joinable threads
except one UI-thread hop, mutex discipline, path confinement, no shell
process spawn, and AddressSanitizer plus UndefinedBehaviorSanitizer on
Linux tests.

This is the human-readable companion to [`SKILL.md`](SKILL.md). Agents
follow `SKILL.md`; the copy-paste examples live in that file.

## Scope

The skill is the ongoing **memory, thread, and process** standard. It
does **not** set readability caps, DRY, or naming; those live in
`cpp-code-style`. It does **not** scaffold CMake or CI; those live in
`cpp-project-setup`. It does **not** write tests or coverage gates;
those live in `cpp-unit-testing`. Durable JSON replace lives in
`cpp-json-files`.

It does **not** require a particular UI toolkit or database driver.
Widget parenting, statement bind flags, and execute APIs stay in the
project that uses them. The skill only requires: who owns the object,
which thread may touch it, and that every explicit release is a
deleter.

## When to apply

Use this skill when adding or changing ownership, `unique_ptr`, custom
deleters, leaks, threads, mutexes, atomics, database handles, UI-thread
posting, filesystem paths, child processes, or sanitizers.

Trigger phrases: memory safety, leak, RAII, deallocate, `unique_ptr`,
thread, `detach`, `jthread`, mutex, UI thread, database connection,
statement, path traversal, `std::system`, `popen`, ASan, UBSan.

## Conventions

| Area | Rule |
|---|---|
| Composition root | Long-lived services are `std::unique_ptr` members. The context object holds references. Declare dependencies first so destruction is reverse. |
| Heap | No owning raw `new` / `delete`. If a toolkit takes ownership of a heap object, do not also wrap it in `unique_ptr`. |
| C handles | Connection, statement, cursor, library buffer, fd: `unique_ptr` (or a one-purpose guard) whose deleter is the only close / finalize / free. |
| Views | Do not store `string_view` / `span`. Do not capture them in a thread. Copy first. |
| Driver-lent bytes | Copy out of a row buffer before the next step, reset, or close. |
| UI hop | Only when joining would deadlock on a post back to the UI thread. One detached worker per action. `shared_ptr` state: `alive`, generation, latest pending request. Catch inside the worker. Post to the UI thread; the callable checks `alive` and generation. |
| Other threads | `std::jthread` or a joined `std::thread`. Request stop, then join before the owner is destroyed. No `detach`. |
| Mutex | `lock_guard` / `scoped_lock`. One mutex per non-thread-safe library object. No UI, no posts, no callbacks under the lock. |
| Affinity | An object used on one thread stays on that thread. A mutex does not make a UI call from a worker legal. |
| Paths | One root per store. Join, `lexically_normal`, reject absolute-outside-root and a relative form that still begins with `..`. |
| Processes | No `std::system`, `popen`, `_popen`, or shell-string execute. A child, only if in-process is impossible, gets an argument vector and a known executable path. |
| Runtime DLLs | On Windows, POST_BUILD copies the MinGW runtime next to the executable. |
| Sanitizers | Extra Linux Clang Debug job with ASan+UBSan, `halt_on_error`, `ctest`. Do not replace the Release job. Windows CI stays unsanitized. |
| Git (agents) | No commit, push, or branch-switch-to-edit unless the user explicitly asked. |

## Why a deleter instead of a close on each return

A C API that needs `close` / `finalize` / `free` is easy to get right on
the success path and wrong on every other path. A throw, a new early
`return`, or a failed step that the author did not update leaks the
handle.

Putting the release in a `unique_ptr` deleter makes the handle's
lifetime the same as any other C++ object. The skill still allows an
explicit release **function**; it forbids scattering that function at
each `return`.

## Why most threads join and one hop may detach

Joining is how the owner knows the worker is gone before it destroys
state the worker uses. `detach` hides that.

A worker that must **post back** to the UI thread cannot be joined on
that same thread: the join waits for the worker, the worker waits for
the UI thread to run the post. That is a deadlock, so that hop is the
one case that detaches.

Detaching does not mean "start a thread per click." One in-flight
worker, a generation so a late post is a no-op after the UI object dies
or a newer request wins, and inputs copied by value. An exception in
that worker would call `std::terminate`; the `catch` stays in the
worker.

## Why a mutex does not fix UI calls

Library objects that are not thread-safe (a connection, a session, a
shared cache) need one mutex and a short critical section. Widgets and
other UI-only objects are still confined to the UI thread. Serializing
a call to them from a worker is still a data race against the toolkit's
own thread, and it is still the wrong thread for painting and events.

## Why paths are confined

`std::filesystem` will happily join `..` segments. User text or a
record field that is treated as a relative path can walk out of the
intended store. Normalizing and checking a prefix against one chosen
root is the whole rule. Each store (data, cache, and so on) has its own
root; do not write cache artifacts into a user-relocatable data
directory unless that is the documented root for that store.

## Why no shell and why DLLs sit beside the exe

A shell string is an injection surface and an extra process with an
unspecified environment. Almost all work belongs in-process. When a
child is unavoidable, an argument vector and a known path keep the
executable and the arguments separate.

On Windows, loading `libstdc++` (and friends) from `PATH` can pick a
different MinGW than the one that built the binary. Copying those DLLs
next to the executable makes the process load the matching runtime.

## Why sanitizers are an extra Linux job

The default CI from `cpp-project-setup` is Release. ASan and UBSan want
Debug, extra flags, and a separate build directory. Replacing the
Release job would drop the configuration that matches what people run
locally. Windows MinGW sanitizer support is weaker and is left off.

## Files

| File | Audience |
|---|---|
| [`SKILL.md`](SKILL.md) | Agent procedure and all examples |
| [`README.md`](README.md) | This human explanation |

## Using the skill in another repo

This copy lives only in this skills repository
(`.cursor/skills/cpp-code-style-security/`). Cursor loads **project**
skills from the repo you have open, and **personal** skills from
`~/.cursor/skills/`.

To use it while working in other C++ projects, copy the directory:

```text
.cursor/skills/cpp-code-style-security/  →  ~/.cursor/skills/cpp-code-style-security/
```

On Windows the personal skills directory is typically
`C:\Users\<you>\.cursor\skills\`. Do not copy it into
`~/.cursor/skills-cursor/` (that tree is reserved for Cursor's built-in
skills).

After copying, a later agent session can pick the skill up from the
description (memory, leak, RAII, thread, mutex, path, process, ASan) or
you can mention it by name.

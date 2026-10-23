---
name: cpp-json-files
description: >-
  Persists C++20 values as JSON files with nlohmann/json 3.11.3: hand-rolled
  to_json and from_json, schemaVersion, unknown-key retention, and durable
  replace (temp file, fsync, then replace). Use when reading or writing JSON
  files, adding serialization, versioning a JSON document, or storing config
  or records on disk. Does not cover HTTP or REST.
---

# C++ JSON Files

Apply when a C++ value is stored in a JSON file or when `to_json` /
`from_json` for that file changes. This skill does **not** fetch, send, or
authenticate HTTP. A JSON body that arrived from the network is out of scope.

Do not re-scaffold CMake, the filesystem port, or the unit-test harness.
Do not add a second JSON library. Substitute `PROJ`, `PREFIX`, and `NS`.
Do not leave those tokens in the project.

If `replaceText`, `JsonObjectBag`, `JsonFileError`, and `loadJson` /
`storeJson` already exist, use them. Add them from the implementation
sections below only when they are missing. Do not invent a second write
path.

```
JSON file progress (copy into the task):
- [ ] Confirm this is a file, not HTTP or REST
- [ ] nlohmann/json v3.11.3 linked; no second JSON library
- [ ] replaceText, JsonObjectBag, JsonFileError, loadJson, storeJson
- [ ] Root kind (record or envelope) and k<Type>Schema
- [ ] Hand-rolled to_json / from_json; migrate; then absorb unknown keys
- [ ] Load and store go through loadJson / storeJson
- [ ] Tests listed below
- [ ] Header example of the JSON shape
- [ ] Stop
```

# Invariants

- Library: **nlohmann/json v3.11.3**, header-only. FetchContent pin
  `v3.11.3` (never `master`). Before `FetchContent_MakeAvailable`:
  `JSON_BuildTests OFF`, `JSON_Install OFF`. Link
  `nlohmann_json::nlohmann_json` **PUBLIC** on the logic target whose
  headers include `nlohmann/json_fwd.hpp`. Follow the repo's existing
  FetchContent rules for shallow pin, disconnected updates, and
  `CMAKE_POLICY_VERSION_MINIMUM`.
- Default `nlohmann::json` (keys sorted by `std::map`). Do not use
  `nlohmann::ordered_json`. Do not pass `ensure_ascii = true`. Do not
  change the dump error handler. Do not enable comment parsing.
- Persisted text is `dump(2)` plus one trailing `\n`. UTF-8, no BOM.
- Serde **throws**. `loadJson` / `storeJson` catch and return
  `Result<T, JsonFileError>`. Do not let those exceptions escape the
  load or store function.
- Domain and service code does not open `ifstream` / `ofstream`. Reads
  use the filesystem port's `readText` (binary, full file). Writes of a
  JSON document use `replaceText` only.
- `loadJson` reads the destination path only. It does not read a leftover
  `*.tmp`.
- One writer at a time for a given path. `replaceText` has no internal lock.
- JSON lives in the logic library. Do not include `nlohmann/json.hpp` from
  a UI target.
- Persisted filesystem paths are strings from
  `std::filesystem::path::generic_string()` (`/` separators). Do not write
  `path::string()` into JSON. Do not store file bytes, `char`, or
  `std::vector<std::uint8_t>` in the document; store a path to the blob.
- Integers that another language may read stay inside
  ±(2^53−1), or are stored as strings.
- No commit, push, or branch switch unless the user explicitly asked.

# Document shape

Each file is one JSON document with an integer `schemaVersion`.

| Root kind | Use when | On-disk shape |
|---|---|---|
| **Record** | Named members | `{ "schemaVersion": N, "<key>": ... }` |
| **Envelope** | The payload is an array, or an object whose keys are dynamic (ids) | `{ "schemaVersion": N, "data": <payload> }` |

Freeze the kind when the file is introduced. `schemaVersion` is reserved.
On an envelope, `data` is reserved too. Do not use those strings as domain
keys.

Missing `schemaVersion` means **version 1** (the original shape), then
migrations run from 1 up to the current constant. It does not mean "already
current".

Writers always emit the current constant. Readers reject a number greater
than the constant, and reject a number less than 1.

Bump `k<Type>Schema` only for a breaking change: rename, remove, retype,
or a change of root kind. An added key with a default is **not** a bump.
On a bump: add one migrate step from the previous number, erase every key
that step consumed, add a fixture of the previous document, and update the
header example. Do not skip a step.

# Serde

Declare `to_json` and `from_json` in the type's namespace (ADL). Include
`<nlohmann/json_fwd.hpp>` in that header, not `<nlohmann/json.hpp>`.
Include the full header in the `.cpp` only.

Do not use `NLOHMANN_DEFINE_TYPE_INTRUSIVE`,
`NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE`, or `NLOHMANN_JSON_SERIALIZE_ENUM`.
Spell every key in the hand-rolled functions.

Each JSON object type has a `JsonObjectBag extras` member (see
`JsonObjectBag` below). `operator==` is defaulted so `extras`
participates. The JSON key is never the word `extras`.

Order inside `from_json`:

1. `migrate...(j)` on a copy (throws on a future or nonsense version).
2. Read known keys from the migrated value.
3. `absorbUnknown` with every key this `to_json` writes, including
   `schemaVersion` and, for an envelope, `data`.

`to_json` writes known keys, then `emitUnknown`. Known keys win. `emitUnknown`
does not overwrite them.

| Member | Read |
|---|---|
| Required | `migrated.at("key").get_to(...)` — missing key throws `out_of_range` |
| Optional, added later | `migrated.value("key", default)` — absent uses the default; present but wrong type still throws |
| C++ name differs from the key | Spell the JSON key. A keyword uses a trailing underscore on the member only (`signed_` ↔ `"signed"`) |

Enums are JSON **strings**, via that enum's `to_string` / `fromString`
(add them if missing; UI and JSON share one spelling). Unknown text throws
`std::invalid_argument` with a message that starts with
`Unknown <Enum> value: `. Do not persist the enumerator as a number.

`migrate` for a **record** keeps a JSON object. `migrate` for an
**envelope**: if `schemaVersion` is absent, the whole document (object or
array) becomes `data`; if it is present, read `data` and copy every other
key except `schemaVersion` and `data` onto the result. Set
`schemaVersion` to the current constant on the object you return.

Dynamic id keys (envelope `data`, or a nested id map) use the canonical
decimal form (`std::to_string` of the integer, base 10). Reject a key that
does not match that form, or that overflows the id type, with
`std::invalid_argument`.

Unsupported version throws `std::invalid_argument` whose message contains
`unsupported schemaVersion`. Include the number found and the supported
range `1..k<Type>Schema`.

# Load and store

`loadJson<T>` and `storeJson<T>` are the only parse and dump path for
files. Implement them as in Document I/O below.

`storeJson` builds JSON, `dump(2)`, appends `\n`, then `replaceText`.
`replaceText` writes `destination` + `.tmp` in the same directory, fsyncs
that temp file, then replaces the destination. On Windows the replace is
`MoveFileExW` with `MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH`.
Do not use `std::filesystem::rename` to replace an existing file on
Windows. Do not open the destination with `std::ios::trunc`. Full body:
`replaceText` below.

On failure before the replace, the previous destination bytes stay.
A directory fsync error **after** a POSIX replace means the new bytes are
already visible; say `after replace` in the message. Retrying `storeJson`
is safe. Do not delete the destination.

| `JsonFileErrc` | When |
|---|---|
| `Io` | Missing file, short read, directory create, temp write, fsync, replace, or any other filesystem-port failure. Message includes `path.generic_string()`. |
| `Malformed` | `json::parse_error`, or any other `std::exception` that is not listed below. The bytes are not a JSON document. |
| `Type` | `json::type_error`, `std::invalid_argument` (unknown enum, bad id key, unsupported `schemaVersion`), `std::out_of_range` that is **not** `json::out_of_range` |
| `MissingKey` | `json::out_of_range` from a required key |

Catch JSON exceptions before `std::exception`. In v3.11.3,
`json::out_of_range` does not inherit `std::out_of_range`.

File absent is `Io`, not `MissingKey`. Branch on `kind`. Do not collapse
the four kinds into one `parse error` string. `JsonFileError` stays
intact if a wider API wraps it.

| File role | When the path does not exist |
|---|---|
| Required document (config, user data) | Build the default, `storeJson`, return the default. Do not surface a first-launch open failure. |
| Cache or download | Return the feature's own not-ready error. Do not create an empty file. Do not use `MissingKey`. |

A moved path: if the new path is absent and the legacy path exists, load
the legacy path, `storeJson` the new path, and only then remove the legacy
path. A failed store leaves the legacy file in place.

# Tests

Follow `cpp-unit-testing`: same change, the unit-test harness, hermetic
in-memory filesystem, no host paths, explicit source list. Assert `kind`,
not incidental nlohmann wording, except the two stable phrases below.

For each document type:

- Round-trip: `operator==`, and the JSON text contains the spelled keys
  (aliases included) and `"schemaVersion"` equal to the current constant.
- The stored text ends with `\n` and contains `\n  "` (two-space indent).
- An unknown key survives `storeJson` and is still present in the text.
  Equality of two loaded values is not enough.
- A missing optional key gets its default.
- A document with no `schemaVersion` loads as version 1 and runs
  migrations. After a bump, a fixture of the previous shape loads, and
  the old key is not written back.
- `unsupported schemaVersion` → `Type`, and `message` contains
  `unsupported schemaVersion`.
- Unknown enumerator inside the file → `Type`, and `message` starts with
  `Unknown <Enum> value: `.
- Not JSON → `Malformed`. Wrong JSON type for a required member → `Type`.
  Absent required key → `MissingKey`, and `message` contains that key.
- `replaceText` failure → `Io`, and the previous destination bytes are
  unchanged.
- A path member round-trips with `/`.

Direct `from_json` tests may `CHECK_THROWS`. The file-level tests are
still required, because those lock `kind`.

# Header example

On the type, show the on-disk shape in the header comment (the
header-comment form is `cpp-documentation`'s job). Record:

```json
{ "schemaVersion": 1, "dataStorage": "/data", "theme": "Light" }
```

Envelope:

```json
{ "schemaVersion": 1, "data": { "17": {} } }
```

When the format crosses modules, note the filename, root kind, and
`k<Type>Schema` in the owning `docs/` page and `AGENTS.md` pointer.
Do not copy this skill into those files.

# Record (copy shape)

`kConfigurationSchema`, `migrateConfiguration`, and the free functions live
with the type. `migrateConfiguration` stays in the `.cpp` (the header must
not include `json.hpp`).

```cpp
inline constexpr int kConfigurationSchema = 1;

void to_json(nlohmann::json& j, const Configuration& c) {
    j = nlohmann::json{
        {"schemaVersion", kConfigurationSchema},
        {"dataStorage", c.dataStorage},
        {"theme", c.theme},
    };
    emitUnknown(j, c.extras);
}

void from_json(const nlohmann::json& j, Configuration& c) {
    const nlohmann::json migrated = migrateConfiguration(j);
    migrated.at("dataStorage").get_to(c.dataStorage);
    c.theme = migrated.value("theme", Theme::Light);
    absorbUnknown(c.extras, migrated,
                  {"schemaVersion", "dataStorage", "theme"});
}
```

```cpp
nlohmann::json migrateConfiguration(nlohmann::json j) {
    if (!j.is_object())
        throw std::invalid_argument("configuration root must be a JSON object");
    int version = 1;
    if (j.contains("schemaVersion"))
        version = j.at("schemaVersion").get<int>();
    if (version < 1 || version > kConfigurationSchema)
        throw std::invalid_argument(
            "unsupported schemaVersion " + std::to_string(version) +
            " (this build supports 1.." + std::to_string(kConfigurationSchema) + ")");
    // if (version < 2) { rewrite version 1; erase consumed keys; version = 2; }
    j["schemaVersion"] = kConfigurationSchema;
    return j;
}
```

Assign a path only through `generic_string()`:

```cpp
cfg.dataStorage = directory.generic_string();
```

# JSON file implementation

Copy these once per repository. Replace `NS`. Do not fork a second writer.
`loadJson` / `storeJson` are the only callers of `json::parse` and `dump`
for files.

# `JsonFileError`

`core/include/NS/json/JsonFileError.hpp`

```cpp
#pragma once

#include <string>

namespace NS {

enum class JsonFileErrc {
    Io,          // filesystem: missing file, read, write, fsync, replace
    Malformed,   // bytes are not JSON
    Type,        // wrong JSON type, unknown enum, bad id, unsupported schema
    MissingKey,  // required key absent
};

struct JsonFileError {
    JsonFileErrc kind{};
    std::string message;
};

}  // namespace NS
```

`Result<void, JsonFileError>::ok()` default-constructs `JsonFileError`.
Keep the type aggregate-friendly.

# `JsonObjectBag`

Header includes `<nlohmann/json_fwd.hpp>` only. The `.cpp` includes
`<nlohmann/json.hpp>`. References to incomplete `nlohmann::json` in the
header are intentional. A value member of `nlohmann::json` is not.

`core/include/NS/json/JsonObjectBag.hpp`

```cpp
#pragma once

#include <nlohmann/json_fwd.hpp>

#include <initializer_list>
#include <memory>
#include <string_view>

namespace NS {

class JsonObjectBag {
public:
    JsonObjectBag();
    ~JsonObjectBag();
    JsonObjectBag(const JsonObjectBag&);
    JsonObjectBag& operator=(const JsonObjectBag&);
    JsonObjectBag(JsonObjectBag&&) noexcept;
    JsonObjectBag& operator=(JsonObjectBag&&) noexcept;

    friend bool operator==(const JsonObjectBag&, const JsonObjectBag&);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    friend void absorbUnknown(JsonObjectBag&, const nlohmann::json&,
                              std::initializer_list<std::string_view>);
    friend void emitUnknown(nlohmann::json&, const JsonObjectBag&);
};

void absorbUnknown(JsonObjectBag& bag, const nlohmann::json& object,
                   std::initializer_list<std::string_view> knownKeys);
void emitUnknown(nlohmann::json& object, const JsonObjectBag& bag);

}  // namespace NS
```

`core/src/json/JsonObjectBag.cpp`

```cpp
#include "NS/json/JsonObjectBag.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>

namespace NS {

struct JsonObjectBag::Impl {
    nlohmann::json fields = nlohmann::json::object();
};

JsonObjectBag::JsonObjectBag() = default;
JsonObjectBag::~JsonObjectBag() = default;
JsonObjectBag::JsonObjectBag(JsonObjectBag&&) noexcept = default;
JsonObjectBag& JsonObjectBag::operator=(JsonObjectBag&&) noexcept = default;

JsonObjectBag::JsonObjectBag(const JsonObjectBag& other)
    : impl_(other.impl_ ? std::make_unique<Impl>(*other.impl_) : nullptr) {}

JsonObjectBag& JsonObjectBag::operator=(const JsonObjectBag& other) {
    if (this == &other) return *this;
    impl_ = other.impl_ ? std::make_unique<Impl>(*other.impl_) : nullptr;
    return *this;
}

bool operator==(const JsonObjectBag& a, const JsonObjectBag& b) {
    const nlohmann::json empty = nlohmann::json::object();
    const nlohmann::json& af = a.impl_ ? a.impl_->fields : empty;
    const nlohmann::json& bf = b.impl_ ? b.impl_->fields : empty;
    return af == bf;
}

void absorbUnknown(JsonObjectBag& bag, const nlohmann::json& object,
                   std::initializer_list<std::string_view> knownKeys) {
    nlohmann::json kept = nlohmann::json::object();
    if (object.is_object()) {
        for (auto it = object.begin(); it != object.end(); ++it) {
            const bool known =
                std::find(knownKeys.begin(), knownKeys.end(),
                          std::string_view{it.key()}) != knownKeys.end();
            if (!known) kept.emplace(it.key(), it.value());
        }
    }
    if (!bag.impl_) bag.impl_ = std::make_unique<JsonObjectBag::Impl>();
    bag.impl_->fields = std::move(kept);
}

void emitUnknown(nlohmann::json& object, const JsonObjectBag& bag) {
    if (!object.is_object() || !bag.impl_ || !bag.impl_->fields.is_object()) return;
    for (auto it = bag.impl_->fields.begin(); it != bag.impl_->fields.end(); ++it) {
        if (!object.contains(it.key())) object.emplace(it.key(), it.value());
    }
}

}  // namespace NS
```

`absorbUnknown` replaces the bag. It does not append. Call it only after
`migrate`, so a renamed key is already gone and is not stored as unknown.

# Document I/O

`core/include/NS/json/DocumentIo.hpp`

Include the domain header (so ADL sees `to_json` / `from_json`) before
calling `loadJson<T>` / `storeJson<T>`. This header includes the full
`json.hpp`. Do not include it from a domain header.

```cpp
#pragma once

#include "NS/json/JsonFileError.hpp"
#include "NS/ports/IFileSystem.hpp"
#include "NS/util/Result.hpp"

#include <nlohmann/json.hpp>

#include <exception>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

namespace NS {

template <typename T>
Result<T, JsonFileError> fromJsonException(std::exception_ptr ptr) {
    try {
        if (ptr) std::rethrow_exception(ptr);
    } catch (const nlohmann::json::parse_error& e) {
        return Result<T, JsonFileError>::err({JsonFileErrc::Malformed, e.what()});
    } catch (const nlohmann::json::type_error& e) {
        return Result<T, JsonFileError>::err({JsonFileErrc::Type, e.what()});
    } catch (const nlohmann::json::out_of_range& e) {
        return Result<T, JsonFileError>::err({JsonFileErrc::MissingKey, e.what()});
    } catch (const std::invalid_argument& e) {
        return Result<T, JsonFileError>::err({JsonFileErrc::Type, e.what()});
    } catch (const std::out_of_range& e) {
        return Result<T, JsonFileError>::err({JsonFileErrc::Type, e.what()});
    } catch (const std::exception& e) {
        return Result<T, JsonFileError>::err({JsonFileErrc::Malformed, e.what()});
    }
    return Result<T, JsonFileError>::err(
        {JsonFileErrc::Malformed, "unknown json failure"});
}

inline std::string dumpJson(const nlohmann::json& j) {
    std::string text = j.dump(2);
    text.push_back('\n');
    return text;
}

template <typename T>
Result<T, JsonFileError> loadJson(IFileSystem& fs, const std::filesystem::path& path) {
    auto text = fs.readText(path);
    if (!text)
        return Result<T, JsonFileError>::err(
            {JsonFileErrc::Io, path.generic_string() + ": " + text.error()});
    try {
        const auto j = nlohmann::json::parse(text.value());
        return Result<T, JsonFileError>::ok(j.template get<T>());
    } catch (...) {
        return fromJsonException<T>(std::current_exception());
    }
}

template <typename T>
Result<void, JsonFileError> storeJson(IFileSystem& fs,
                                      const std::filesystem::path& path,
                                      const T& value) {
    std::string text;
    try {
        text = dumpJson(nlohmann::json(value));
    } catch (...) {
        return fromJsonException<void>(std::current_exception());
    }
    auto wrote = fs.replaceText(path, text);
    if (!wrote)
        return Result<void, JsonFileError>::err(
            {JsonFileErrc::Io, path.generic_string() + ": " + wrote.error()});
    return Result<void, JsonFileError>::ok();
}

}  // namespace NS
```

`IFileSystem::readText` and `replaceText` keep the port's existing string
`Result<void>` / `Result<std::string>`. Classification to `JsonFileErrc`
happens only here. Adjust the `text.error()` / `wrote.error()` call if the
local `Result` names that accessor differently. Do not change `readText` to
return `JsonFileError`.

# Enum strings

```cpp
void to_json(nlohmann::json& j, Theme v) {
    j = std::string(to_string(v));
}

void from_json(const nlohmann::json& j, Theme& v) {
    const auto text = j.get<std::string>();
    auto parsed = themeFromString(text);
    if (!parsed)
        throw std::invalid_argument("Unknown Theme value: " + text);
    v = *parsed;
}
```

`j.get<std::string>()` on a non-string throws `json::type_error` (kind
`Type` once it crosses `loadJson`).

# Envelope migrate

```cpp
nlohmann::json migrateCatalog(nlohmann::json j) {
    nlohmann::json out = nlohmann::json::object();
    if (j.is_object() && j.contains("schemaVersion")) {
        const int version = j.at("schemaVersion").get<int>();
        if (version < 1 || version > kCatalogSchema)
            throw std::invalid_argument(
                "unsupported schemaVersion " + std::to_string(version) +
                " (this build supports 1.." + std::to_string(kCatalogSchema) + ")");
        // if (version < 2) { rewrite j.at("data"); version = 2; }
        out["schemaVersion"] = kCatalogSchema;
        out["data"] = j.at("data");
        for (auto it = j.begin(); it != j.end(); ++it) {
            if (it.key() != "schemaVersion" && it.key() != "data")
                out.emplace(it.key(), it.value());
        }
        return out;
    }
    if (!j.is_object() && !j.is_array())
        throw std::invalid_argument("catalog root must be a JSON object or array");
    out["schemaVersion"] = kCatalogSchema;
    out["data"] = std::move(j);
    return out;
}
```

`j.at("data")` throws `json::out_of_range` when the key is absent, which
`loadJson` reports as `MissingKey`.

Id keys inside `data` (`#include <cstdint>`, `<limits>`, `<string>`):

```cpp
std::uint32_t idFromKey(std::string_view key) {
    if (key.empty())
        throw std::invalid_argument("id key is not a uint32: " + std::string(key));
    std::uint64_t value = 0;
    constexpr auto kMax = std::numeric_limits<std::uint32_t>::max();
    for (char ch : key) {
        if (ch < '0' || ch > '9')
            throw std::invalid_argument("id key is not a uint32: " + std::string(key));
        const auto digit = static_cast<std::uint64_t>(ch - '0');
        if (value > (static_cast<std::uint64_t>(kMax) - digit) / 10u)
            throw std::invalid_argument("id key is not a uint32: " + std::string(key));
        value = value * 10u + digit;
    }
    const auto id = static_cast<std::uint32_t>(value);
    if (std::to_string(id) != key)
        throw std::invalid_argument("id key is not canonical: " + std::string(key));
    return id;
}
```

# `replaceText`

Declare next to `writeText`:

```cpp
virtual Result<void> replaceText(const std::filesystem::path& destination,
                                 std::string_view contents) = 0;
```

Contract:

- Success: destination bytes equal `contents`, and `destination` + `.tmp`
  does not exist.
- Failure before replace: destination unchanged (still absent if it did
  not exist), temp removed.
- POSIX failure of the parent-directory fsync after `rename`: new bytes
  are already at the destination. The error text contains `after replace`.
- Load never opens the temp path.

In-memory fake: on a scripted failure, leave the previous bytes and return
`Result<void>::err`. On success, store `contents` under
`generic_string()`. Do not require a real temp file in unit tests.

`StdFileSystem.cpp` (logic target, not the UI target):

```cpp
#include "NS/infra/StdFileSystem.hpp"

#include <algorithm>
#include <string>
#include <system_error>

#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <cerrno>
#  include <fcntl.h>
#  include <unistd.h>
#endif

namespace NS {

namespace fs = std::filesystem;

namespace {

Result<void> ioErr(const fs::path& path, std::string_view what, std::string_view detail) {
    return Result<void>::err(std::string(what) + ": " + path.generic_string() +
                             ": " + std::string(detail));
}

Result<void> writeSynced(const fs::path& temp, std::string_view contents) {
#if defined(_WIN32)
    HANDLE handle = CreateFileW(temp.wstring().c_str(), GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        return ioErr(temp, "open temp failed",
                     std::string("win32 ") + std::to_string(GetLastError()));
    const char* cursor = contents.data();
    std::size_t left = contents.size();
    while (left > 0) {
        const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(left, 1u << 20));
        DWORD written = 0;
        if (!WriteFile(handle, cursor, chunk, &written, nullptr) || written == 0) {
            const auto code = GetLastError();
            CloseHandle(handle);
            return ioErr(temp, "write temp failed",
                         std::string("win32 ") + std::to_string(code));
        }
        cursor += written;
        left -= written;
    }
    if (!FlushFileBuffers(handle)) {
        const auto code = GetLastError();
        CloseHandle(handle);
        return ioErr(temp, "fsync temp failed",
                     std::string("win32 ") + std::to_string(code));
    }
    CloseHandle(handle);
    return Result<void>::ok();
#else
    const int fd = ::open(temp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0)
        return ioErr(temp, "open temp failed", std::generic_category().message(errno));
    const char* cursor = contents.data();
    std::size_t left = contents.size();
    while (left > 0) {
        const ssize_t n = ::write(fd, cursor, left);
        if (n < 0) {
            if (errno == EINTR) continue;
            const int code = errno;
            ::close(fd);
            return ioErr(temp, "write temp failed", std::generic_category().message(code));
        }
        cursor += n;
        left -= static_cast<std::size_t>(n);
    }
    while (::fsync(fd) != 0) {
        if (errno == EINTR) continue;
        const int code = errno;
        ::close(fd);
        return ioErr(temp, "fsync temp failed", std::generic_category().message(code));
    }
    if (::close(fd) != 0)
        return ioErr(temp, "close temp failed", std::generic_category().message(errno));
    return Result<void>::ok();
#endif
}

Result<void> replaceSynced(const fs::path& temp, const fs::path& destination) {
#if defined(_WIN32)
    if (!MoveFileExW(temp.wstring().c_str(), destination.wstring().c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return ioErr(destination, "replace failed",
                     std::string("win32 ") + std::to_string(GetLastError()));
    }
    return Result<void>::ok();
#else
    if (::rename(temp.c_str(), destination.c_str()) != 0)
        return ioErr(destination, "replace failed", std::generic_category().message(errno));
    const fs::path parent = destination.parent_path().empty() ? fs::path(".")
                                                             : destination.parent_path();
    const int dfd = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY);
    if (dfd < 0)
        return ioErr(destination, "directory open failed after replace",
                     std::generic_category().message(errno));
    while (::fsync(dfd) != 0) {
        if (errno == EINTR) continue;
        const int code = errno;
        ::close(dfd);
        return ioErr(destination, "directory fsync failed after replace",
                     std::generic_category().message(code));
    }
    ::close(dfd);
    return Result<void>::ok();
#endif
}

}  // namespace

Result<void> StdFileSystem::replaceText(const fs::path& destination,
                                        std::string_view contents) {
    std::error_code ec;
    if (!destination.parent_path().empty()) {
        fs::create_directories(destination.parent_path(), ec);
        if (ec)
            return ioErr(destination, "create_directories failed", ec.message());
    }

    fs::path temp = destination;
    temp.concat(".tmp");
    {
        std::error_code removeEc;
        fs::remove(temp, removeEc);
        if (fs::exists(temp))
            return ioErr(temp, "remove leftover temp failed",
                         removeEc ? removeEc.message() : std::string("still present"));
    }

    if (auto wrote = writeSynced(temp, contents); !wrote) {
        std::error_code ignore;
        fs::remove(temp, ignore);
        return wrote;
    }
    if (auto replaced = replaceSynced(temp, destination); !replaced) {
        std::error_code ignore;
        fs::remove(temp, ignore);
        return replaced;
    }
    return Result<void>::ok();
}

}  // namespace NS
```

`temp.concat(".tmp")` appends to the filename (`collection.json` →
`collection.json.tmp`) in the same directory, so the replace stays on one
volume.

Put `ioErr`, `writeSynced`, and `replaceSynced` in the `.cpp` anonymous
namespace inside `NS`. They are not a second public API. Change the
`StdFileSystem.hpp` include if the adapter header lives elsewhere.

# CMake

In `cmake/Dependencies.cmake`, with the repo's existing FetchContent setup:

```cmake
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)
```

```cmake
target_link_libraries(PROJ_core PUBLIC nlohmann_json::nlohmann_json)
```

Do not bump the pin as a side effect of a document change.

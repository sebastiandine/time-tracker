---
name: cpp-rest-client
description: >-
  Consumes REST APIs from a C++20 CMake repo: a cpr adapter behind IHttpClient,
  structured HttpError with bounded retries, Bearer or API-key headers,
  pagination, ETag conditional GET, HTTPS, and JSON parse of response bodies.
  Use when adding or changing HTTP or REST calls, cpr, JSON APIs, Bearer
  tokens, pagination, or ETag.
---

# C++ REST Client

Apply when the program is an HTTP **consumer**: it calls someone else's REST
API. Do not re-scaffold CMake, the test harness, or the filesystem. Do not
add a second HTTP library or a second JSON library. This skill does **not**
implement a server.

Persist JSON **files** with `cpp-json-files`. This skill is the network
path. Tests follow `cpp-unit-testing` (hermetic, no live network).

Substitute `PROJ`, `PREFIX`, and `NS`. Do not leave those tokens in the
project. Skip steps that already match. Fill gaps; do not rewrite a
healthy tree.

```
REST client progress (copy into the task):
- [ ] IHttpClient + HttpRequest / HttpResponse / HttpError in core/ports
- [ ] CprHttpClient in infra/; every cpr/ include stays in the .cpp
- [ ] One process-wide session, mutex, User-Agent, TLS on
- [ ] RFC 3986 encode of query values; HTTPS except loopback; no userinfo
- [ ] Structured errors; shouldRetry; at most 3 attempts
- [ ] Auth in headers; secrets never in URLs, errors, or logs
- [ ] Pagination, body cap, connect+total timeouts, ETag when the API has it
- [ ] Hand-rolled JSON parse that does not throw across the port
- [ ] Tests listed below; no network
- [ ] Stop
```

# Invariants

- Port in `core/include/NS/ports/`. Adapter in `core/include/NS/infra/`
  plus `core/src/infra/`. Callers never include `cpr/`.
- One `IHttpClient` owned by the composition root and passed by
  reference. One `cpr::Session` for the client lifetime. Mutex around
  every call. Do not construct a session per request. Do not disable TLS
  verification.
- `execute` is the only port method. GET is a caller that sets
  `HttpMethod::Get`.
- Success is 2xx **or 304**. Body is raw bytes (`std::string` is a
  buffer, not text). 204 and 304 may be empty; do not parse an empty 304
  as JSON.
- No exceptions across the port. Return `Result<HttpResponse, HttpError>`.
- Query **values** are RFC 3986 percent-encoded before `cpr::Url`. `cpr`
  does not encode the string it is given. Space is `%20`. Do not encode
  the `https://` structure, `/`, `?`, `&`, or `=`.
- Filters go in the query string. Do not send a GET or HEAD body.
- Retry only GET, HEAD, PUT, DELETE, or a call with `allowRetry`. Retry
  only transport failure, 429, and 503. Honor `Retry-After`. Cap waits.
- `Authorization` / API-key headers only. Never hardcode a secret. Never
  put it in the query when a header exists. Never write it into an error
  or log. 401 and 403 are returned, not retried.
- Default connect timeout **5s**, total timeout **30s**, body cap
  **16 MiB**. Overridable per request. Over the cap is `Policy`; do not
  parse.
- Refuse `http` except `localhost`, `127.0.0.1`, and `::1`. Refuse
  userinfo (`user:pass@host`).
- Link `cpr` only on the target that compiles the adapter. Pin **1.10.5**.
- No commit, push, or branch switch unless the user explicitly asked.

# Types (copy)

`core/include/NS/ports/IHttpClient.hpp`. Use the project's `Result`.
Includes: `algorithm`, `cctype`, `chrono`, `ctime`, `iomanip`,
`optional`, `sstream`, `string`, `string_view`, `utility`, `vector`.

```cpp
enum class HttpMethod { Get, Head, Post, Put, Patch, Delete };

struct HttpRequest {
    HttpMethod method{HttpMethod::Get};
    std::string url;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
    bool allowRetry{false};
    std::chrono::milliseconds connectTimeout{5000};
    std::chrono::milliseconds totalTimeout{30000};
    std::size_t maxBodyBytes{16 * 1024 * 1024};
};

struct HttpResponse {
    int status{0};
    std::string body;
    std::vector<std::pair<std::string, std::string>> headers;
};

struct HttpError {
    enum class Kind { Transport, HttpStatus, Policy };
    Kind kind{Kind::Transport};
    int status{0};
    std::string message;
    std::optional<std::chrono::milliseconds> retryAfter;
};

class IHttpClient {
public:
    virtual ~IHttpClient() = default;
    virtual Result<HttpResponse, HttpError> execute(const HttpRequest& req) = 0;
};

[[nodiscard]] inline std::optional<std::string> headerValue(
    const std::vector<std::pair<std::string, std::string>>& headers,
    std::string_view name) {
    for (const auto& [k, v] : headers) {
        if (k.size() == name.size() &&
            std::equal(k.begin(), k.end(), name.begin(), name.end(),
                       [](unsigned char a, unsigned char b) {
                           return std::tolower(a) == std::tolower(b);
                       })) {
            return v;
        }
    }
    return std::nullopt;
}

[[nodiscard]] inline std::optional<std::chrono::milliseconds> parseRetryAfter(
    const std::vector<std::pair<std::string, std::string>>& headers) {
    const auto raw = headerValue(headers, "Retry-After");
    if (!raw || raw->empty()) return std::nullopt;
    const bool digits = std::all_of(raw->begin(), raw->end(), [](unsigned char c) {
        return std::isdigit(c);
    });
    if (digits) {
        const auto sec = std::stoll(*raw);
        if (sec < 0) return std::nullopt;
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::seconds{sec});
    }
    std::tm tm{};
    std::istringstream in(*raw);
    in >> std::get_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");
    if (in.fail()) return std::nullopt;
#if defined(_WIN32)
    const std::time_t t = _mkgmtime(&tm);
#else
    const std::time_t t = timegm(&tm);
#endif
    if (t == static_cast<std::time_t>(-1)) return std::nullopt;
    const auto now = std::time(nullptr);
    if (t <= now) return std::chrono::milliseconds{0};
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::seconds{t - now});
}
```

`HttpError::Policy` is a local refusal (scheme, userinfo, empty URL, body
cap) — not an upstream status.

# URL encode and redact

`core/include/NS/util/Rfc3986.hpp`. Encode each query **value** when
building the URL.

```cpp
[[nodiscard]] inline std::string rfc3986PercentEncode(std::string_view in) {
    std::ostringstream out;
    out.fill('0');
    out << std::hex << std::uppercase;
    for (unsigned char c : in) {
        const bool unreserved =
            (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '.' || c == '_' || c == '~';
        if (unreserved) out << static_cast<char>(c);
        else { out << '%'; out.width(2); out << static_cast<unsigned int>(c); }
    }
    return out.str();
}
```

Redact before any string that might be logged or put in `HttpError::message`.
Strip `Authorization` and `Proxy-Authorization`. For the URL, replace the
value of query keys named (ASCII case-insensitive) `token`, `key`, `secret`,
`password`, or `access_token` with `REDACTED`. Drop userinfo if present.

# Session and adapter

`CprHttpClient` owns one `cpr::Session`. Configure once: `User-Agent` =
`PROJ/<version>`, `SetTimeout` / `SetConnectTimeout` from the request (or
the 5s / 30s defaults), `SetRedirect` max 10, follow on, **do not**
continue credentials across hosts (`cont_send_cred=false`). Preserve
method and body on 307 and 308 (cpr `POST_ALL` / equivalent).

Session default `Accept` is `application/json` when this client only
speaks JSON. Use `Accept: */*` only if the same session also downloads
non-JSON bytes (images, files). JSON writes also send
`Content-Type: application/json`.

Per-request headers apply for that call, then are cleared. After every
call: `RemoveContent()` so a POST body is not resent on the next GET.

Inject a `RawGetExecutor`-style callback in tests so mapping and retry
run without libcpr. Production constructor builds the session lambda.

Before the first attempt: reject empty URL; reject userinfo; reject
non-HTTPS unless the host is `localhost`, `127.0.0.1`, or `::1`
(`Policy`). After a successful transport: if `body.size() > maxBodyBytes`,
return `Policy` and do not parse.

# Methods (RFC 9110 / RFC 5789)

| Method | Safe | Idempotent | Auto-retry | Notes |
|---|---|---|---|---|
| GET, HEAD | yes | yes | yes | No body. HEAD for metadata / ETag. |
| PUT | no | yes | yes | Replace the resource at that URL. |
| DELETE | no | yes | yes | A later 404 is still 404; do not rewrite to success. |
| POST | no | no | only if `allowRetry` | Caller sends `Idempotency-Key` (or the API's equivalent). Surface 201 and `Location`. |
| PATCH | no | no | only if `allowRetry` | Same as POST unless the patch document itself is idempotent. |

`allowRetry` defaults to **false**. Setting it without an idempotency key
on POST/PATCH is a bug.

# Map outcome

Pure function; tests call this, not the network.

```cpp
[[nodiscard]] inline Result<HttpResponse, HttpError> mapHttpOutcome(
    bool transportError, std::string_view transportMessage,
    int status, std::string body,
    std::vector<std::pair<std::string, std::string>> headers,
    std::string_view redactedUrl) {
    using R = Result<HttpResponse, HttpError>;
    if (transportError) {
        HttpError e{HttpError::Kind::Transport, 0,
                    std::string("HTTP error: ") + std::string(transportMessage),
                    std::nullopt};
        return R::err(std::move(e));
    }
    if (status == 304 || (status >= 200 && status < 300)) {
        return R::ok(HttpResponse{status, std::move(body), std::move(headers)});
    }
    HttpError e{HttpError::Kind::HttpStatus, status,
                "HTTP " + std::to_string(status) + " from " +
                    std::string(redactedUrl),
                parseRetryAfter(headers)};
    return R::err(std::move(e));
}
```

Do not put the raw body of an error response into `message` (it may
contain tokens).

# Retry

At most **3** attempts inside the adapter. Tests drive `shouldRetry` and
`retryWait`, not a live socket.

```cpp
[[nodiscard]] inline bool methodRetryable(HttpMethod m, bool allowRetry) {
    return allowRetry || m == HttpMethod::Get || m == HttpMethod::Head ||
           m == HttpMethod::Put || m == HttpMethod::Delete;
}

[[nodiscard]] inline bool shouldRetry(HttpMethod method, bool allowRetry,
                                      HttpError::Kind kind, int status,
                                      int attempt /* 1-based, just finished */) {
    if (attempt >= 3) return false;
    if (!methodRetryable(method, allowRetry)) return false;
    if (kind == HttpError::Kind::Transport) return true;
    if (kind == HttpError::Kind::HttpStatus &&
        (status == 429 || status == 503))
        return true;
    return false;
}

[[nodiscard]] inline std::chrono::milliseconds retryWait(
    int failedAttemptIndex, /* 0 after first failure */
    std::optional<std::chrono::milliseconds> retryAfter) {
    auto exp = std::chrono::milliseconds{200 * (1 << failedAttemptIndex)};
    if (exp > std::chrono::milliseconds{2000})
        exp = std::chrono::milliseconds{2000};
    auto wait = retryAfter ? std::max(exp, *retryAfter) : exp;
    constexpr auto kCap = std::chrono::milliseconds{10000};
    if (wait > kCap) wait = kCap;
    return wait;
}
```

The adapter may add small jitter on `retryWait` in production. Do **not**
retry 400, 401, 403, 404, 409, 412, other 4xx, or 5xx other than 503.
Stop if the remaining total timeout cannot cover another attempt.

# Auth, pagination, conditional GET

**Auth.** Caller puts `{"Authorization", "Bearer " + token}` or the
documented API-key header on `HttpRequest::headers`. Source: config or
environment, never source control. 401/403 → `HttpStatus`, no retry.

**Pagination.** Belongs in the caller, not the adapter. Follow that API's
next link (JSON field or `Link` `rel=next`). Keep the same filter and
sort on every page. Stop on a missing next, an empty page, or an explicit
item/page cap. Do not invent a universal paginator.

**Conditional GET.** Store `ETag` from a 200. Later GET sends
`If-None-Match`. 304 → reuse the cached body; do not parse the empty
payload. Updates that must not clobber a concurrent change send
`If-Match`; 412 is returned, not retried.

# JSON bodies

Parse response bodies with the project's JSON library. Hand-rolled serde
lives in `cpp-json-files`; do not restate it here. Parse in `try`/`catch
(const std::exception&)` and return a domain or `HttpError` — never let
the exception escape the source. Required missing fields are errors.
Expose `static Result<T, E> parseX(const std::string& body)` so tests
feed captured bodies with no HTTP.

```cpp
Result<std::vector<Item>> ItemSource::parseResponse(const std::string& body) {
    try {
        const auto j = parseJson(body);  // project JSON library
        if (!j.is_array())
            return Result<std::vector<Item>>::err("response is not an array");
        // map fields; required missing → error
        return Result<std::vector<Item>>::ok(std::move(out));
    } catch (const std::exception& e) {
        return Result<std::vector<Item>>::err(
            std::string("JSON parse error: ") + e.what());
    }
}

Result<std::vector<Item>> ItemSource::fetchAll() {
    HttpRequest req;
    req.method = HttpMethod::Get;
    req.url = std::string(kEndpoint);
    auto resp = http_.execute(req);
    if (!resp) return Result<std::vector<Item>>::err(resp.error().message);
    return parseResponse(resp.value().body);
}
```

# Domain outcome (above HTTP)

After the adapter has finished retrying:

| Outcome | Treat as | Cache? |
|---|---|---|
| Transport, 429, 5xx | Transient | Never |
| Clean 404, or 2xx body that says no match | Not found | Allowed |
| 401, 403 | Auth failure | Never (not a miss) |
| 409, 412 | Conflict / precondition | Never |

A transient miss must not be remembered as "does not exist".

# CMake

In `cmake/Dependencies.cmake`, pin **1.10.5**. 1.11.x `install(EXPORT
cprTargets)` references `libcurl_shared`, which is not in an export set
when curl is an in-tree subproject. Bump only after disabling that
install rule or exporting curl.

```cmake
message(STATUS "[PROJ] Fetching libcpr/cpr ...")
FetchContent_Declare(
    cpr
    GIT_REPOSITORY https://github.com/libcpr/cpr.git
    GIT_TAG        1.10.5
    GIT_SHALLOW    TRUE
)
set(CPR_USE_SYSTEM_CURL OFF CACHE INTERNAL "")
set(CPR_BUILD_TESTS OFF CACHE INTERNAL "")
set(CPR_ENABLE_SSL ON CACHE INTERNAL "")
set(CPR_FORCE_USE_SYSTEM_CURL OFF CACHE INTERNAL "")
set(BUILD_CURL_EXE         OFF CACHE INTERNAL "")
set(BUILD_TESTING          OFF CACHE INTERNAL "")
set(CURL_DISABLE_TESTS     ON  CACHE INTERNAL "")
set(CURL_DISABLE_DOCS      ON  CACHE INTERNAL "")
set(CURL_DISABLE_INSTALL   ON  CACHE INTERNAL "")
set(CURL_DISABLE_LDAP      ON  CACHE INTERNAL "")
set(CURL_DISABLE_LDAPS     ON  CACHE INTERNAL "")
set(CURL_DISABLE_FTP       ON  CACHE INTERNAL "")
set(CURL_DISABLE_TELNET    ON  CACHE INTERNAL "")
set(CURL_DISABLE_DICT      ON  CACHE INTERNAL "")
set(CURL_DISABLE_FILE      ON  CACHE INTERNAL "")
set(CURL_DISABLE_TFTP      ON  CACHE INTERNAL "")
set(CURL_DISABLE_GOPHER    ON  CACHE INTERNAL "")
set(CURL_DISABLE_IMAP      ON  CACHE INTERNAL "")
set(CURL_DISABLE_POP3      ON  CACHE INTERNAL "")
set(CURL_DISABLE_SMB       ON  CACHE INTERNAL "")
set(CURL_DISABLE_SMTP      ON  CACHE INTERNAL "")
set(CURL_DISABLE_RTSP      ON  CACHE INTERNAL "")
set(CURL_DISABLE_MQTT      ON  CACHE INTERNAL "")
if(WIN32)
    set(CURL_USE_SCHANNEL  ON  CACHE INTERNAL "")
    set(CMAKE_USE_SCHANNEL ON  CACHE INTERNAL "")
    set(CURL_USE_OPENSSL   OFF CACHE INTERNAL "")
    set(HAVE_IOCTLSOCKET_FIONBIO ON CACHE INTERNAL "")
endif()
FetchContent_MakeAvailable(cpr)
```

`target_link_libraries(PROJ_core PUBLIC cpr::cpr)` (or the imported
name the pin exports) **only** on the logic target that compiles
`CprHttpClient.cpp`. Follow the repo's existing FetchContent rules
(`FETCHCONTENT_UPDATES_DISCONNECTED`, `CMAKE_POLICY_VERSION_MINIMUM`).
On Windows, Schannel is the TLS backend so the build host does not need
OpenSSL. `HAVE_IOCTLSOCKET_FIONBIO` is required for MinGW-w64 UCRT64:
curl's compile test for `ioctlsocket(FIONBIO)` can fail on header order.

Add the new `.cpp` files to `core/CMakeLists.txt` (no `GLOB`).

# Tests

Fake `IHttpClient` in the test file until a second file needs it, then
`tests/fakes/`. No network. List each new `*_tests.cpp` in
`tests/CMakeLists.txt`.

Required cases:

- `rfc3986PercentEncode`: space → `%20`, unreserved round-trip, non-ASCII byte
- `mapHttpOutcome`: transport wins over status; 200/204/299/304 success;
  404/503 `HttpStatus` with redacted URL
- Secret redaction: `Authorization` and `?access_token=` do not appear in
  `message`
- HTTPS guard: `http://example.com` → `Policy`; `http://127.0.0.1/` allowed
- Userinfo → `Policy`
- Body over cap → `Policy`
- `shouldRetry`: GET + 503 + attempt 1 → true; attempt 3 → false;
  POST without `allowRetry` + 503 → false; POST with `allowRetry` → true;
  401/404/412 → false
- `retryWait`: `Retry-After: 5` is at least 5s and ≤ 10s cap
- `parseX`: happy captured body; malformed JSON; missing required field
- Caller builds the expected encoded URL (assert the fake's last URL)
- Real `CprHttpClient{}` against `://not-a-valid-url` returns an error
  (no network)

# UI thread

If a GUI calls the client, do not block the UI thread on `execute`. Run
the blocking call on a worker thread; deliver the result on the UI
thread; hold `shared_ptr` state with `alive` and a generation counter;
do not capture `this`. The GUI skill owns the toolkit post
(`cpp-wxwidgets-ui` is the current GUI skill). Do not restate that
skill here.

# AGENTS.md

In `core/AGENTS.md`: port vs adapter paths; percent-encode before
`execute`; one session plus mutex; no `cpr/` outside the adapter `.cpp`;
retry and secret rules in one short list. Long API-specific mapping
belongs in `docs/`. Do not paste this skill into those files.

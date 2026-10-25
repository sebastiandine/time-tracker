#include "support/TempDir.hpp"

#include "timetracker/paths/DataPaths.hpp"
#include "timetracker/ports/IDatabaseSession.hpp"
#include "timetracker/ports/IEnvironment.hpp"
#include "timetracker/ports/IFilesystem.hpp"

#include <doctest/doctest.h>

#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace timetracker;

namespace {

class MapEnvironment final : public IEnvironment {
public:
    std::map<std::string, std::string> values;

    [[nodiscard]] std::optional<std::string> get(std::string_view name) const override {
        const auto found = values.find(std::string{name});
        if (found == values.end() || found->second.empty()) {
            return std::nullopt;
        }
        return found->second;
    }

    [[nodiscard]] std::vector<std::pair<std::string, std::string>> entries() const override {
        std::vector<std::pair<std::string, std::string>> listed;
        for (const auto& [key, value] : values) {
            if (!value.empty()) {
                listed.emplace_back(key, value);
            }
        }
        return listed;
    }
};

class RecordingFiles final : public IFilesystem {
public:
    bool fail{false};
    std::filesystem::path created;

    [[nodiscard]] Result<void> ensureDirectory(const std::filesystem::path& directory) override {
        created = directory;
        if (fail) {
            return Result<void>::err("cannot create directory");
        }
        std::error_code error;
        std::filesystem::create_directories(directory, error);
        if (error) {
            return Result<void>::err("cannot create directory");
        }
        return Result<void>::ok();
    }

    [[nodiscard]] bool exists(const std::filesystem::path&) const override { return false; }

    [[nodiscard]] Result<std::string> readText(const std::filesystem::path& path) override {
        return Result<std::string>::err("not found: " + path.generic_string());
    }

    [[nodiscard]] Result<void> replaceText(const std::filesystem::path&, std::string_view) override {
        return Result<void>::ok();
    }
};

class RecordingSession final : public IDatabaseSession {
public:
    bool fail{false};
    bool opened{false};
    std::filesystem::path path;

    [[nodiscard]] Result<void> open(const std::filesystem::path& dbPath) override {
        opened = true;
        path = dbPath;
        if (fail) {
            return Result<void>::err("Failed to open database: boom");
        }
        return Result<void>::ok();
    }
};

}  // namespace

TEST_CASE("a path that climbs out of the root is rejected") {
    test::TempDir dir;
    CHECK(isConfinedPath(dir.path, "time_tracker.db"));
    CHECK_FALSE(isConfinedPath(dir.path, ".."));
    CHECK_FALSE(isConfinedPath(dir.path, "."));
    CHECK_FALSE(isConfinedPath(dir.path, {}));
    CHECK_FALSE(isConfinedPath(dir.path, dir.path / "elsewhere"));
}

TEST_CASE("the database file stays inside the data directory") {
    test::TempDir dir;
    auto file = databasePath(dir.path);
    REQUIRE(file.succeeded());
    CHECK(file.value().filename() == "time_tracker.db");
    CHECK(isConfinedPath(dir.path, "time_tracker.db"));
}

#if defined(_WIN32)

TEST_CASE("a missing LOCALAPPDATA value is an error") {
    MapEnvironment environment;
    auto directory = userDataDirectory(environment);
    CHECK_FALSE(directory.succeeded());
    CHECK(directory.error() == "LOCALAPPDATA is not set");
}

TEST_CASE("the user data directory is TimeTracker under LOCALAPPDATA") {
    test::TempDir dir;
    MapEnvironment environment;
    environment.values["LOCALAPPDATA"] = dir.path.string();
    auto directory = userDataDirectory(environment);
    REQUIRE(directory.succeeded());
    CHECK(directory.value().filename() == "TimeTracker");
    CHECK(directory.value().parent_path() == dir.path);
}

#else

TEST_CASE("a missing HOME value is an error when XDG is unset") {
    MapEnvironment environment;
    auto directory = userDataDirectory(environment);
    CHECK_FALSE(directory.succeeded());
    CHECK(directory.error() == "HOME is not set");
}

TEST_CASE("XDG_DATA_HOME is the data root") {
    test::TempDir dir;
    MapEnvironment environment;
    environment.values["XDG_DATA_HOME"] = dir.path.string();
    auto directory = userDataDirectory(environment);
    REQUIRE(directory.succeeded());
    CHECK(directory.value().filename() == "TimeTracker");
    CHECK(directory.value().parent_path() == dir.path);
}

TEST_CASE("an empty XDG value falls back to HOME/.local/share") {
    test::TempDir dir;
    MapEnvironment environment;
    environment.values["XDG_DATA_HOME"] = "";
    environment.values["HOME"] = dir.path.string();
    auto directory = userDataDirectory(environment);
    REQUIRE(directory.succeeded());
    CHECK(directory.value().filename() == "TimeTracker");
    CHECK(directory.value().parent_path().filename() == "share");
}

TEST_CASE("HOME is used when XDG is missing") {
    test::TempDir dir;
    MapEnvironment environment;
    environment.values["HOME"] = dir.path.string();
    auto directory = userDataDirectory(environment);
    REQUIRE(directory.succeeded());
    CHECK(directory.value().generic_string().find("/.local/share/TimeTracker") != std::string::npos);
}

#endif

TEST_CASE("openUserDatabase reports a missing data root") {
    MapEnvironment environment;
    RecordingFiles files;
    RecordingSession session;
    auto opened = openUserDatabase(environment, files, session);
    CHECK_FALSE(opened.succeeded());
    CHECK_FALSE(session.opened);
}

TEST_CASE("openUserDatabase creates the directory and opens the database file") {
    test::TempDir dir;
    MapEnvironment environment;
#if defined(_WIN32)
    environment.values["LOCALAPPDATA"] = dir.path.string();
#else
    environment.values["XDG_DATA_HOME"] = dir.path.string();
#endif
    RecordingFiles files;
    RecordingSession session;
    auto opened = openUserDatabase(environment, files, session);
    REQUIRE(opened.succeeded());
    CHECK(session.opened);
    CHECK(session.path.filename() == "time_tracker.db");
    CHECK(session.path.parent_path().filename() == "TimeTracker");
}

TEST_CASE("a directory failure does not open the database") {
    test::TempDir dir;
    MapEnvironment environment;
#if defined(_WIN32)
    environment.values["LOCALAPPDATA"] = dir.path.string();
#else
    environment.values["XDG_DATA_HOME"] = dir.path.string();
#endif
    RecordingFiles files;
    files.fail = true;
    RecordingSession session;
    auto opened = openUserDatabase(environment, files, session);
    CHECK_FALSE(opened.succeeded());
    CHECK(opened.error() == "cannot create directory");
    CHECK_FALSE(session.opened);
}

TEST_CASE("openDatabaseAt creates the directory and opens the database file") {
    test::TempDir dir;
    RecordingFiles files;
    RecordingSession session;
    auto opened = openDatabaseAt(dir.path / "chosen", files, session);
    REQUIRE(opened.succeeded());
    CHECK(session.opened);
    CHECK(session.path.filename() == "time_tracker.db");
    CHECK(session.path.parent_path().filename() == "chosen");
}

TEST_CASE("openDatabaseAt does not open when the directory cannot be created") {
    test::TempDir dir;
    RecordingFiles files;
    files.fail = true;
    RecordingSession session;
    auto opened = openDatabaseAt(dir.path / "chosen", files, session);
    CHECK_FALSE(opened.succeeded());
    CHECK(opened.error() == "cannot create directory");
    CHECK_FALSE(session.opened);
}

TEST_CASE("a database open failure is returned") {
    test::TempDir dir;
    MapEnvironment environment;
#if defined(_WIN32)
    environment.values["LOCALAPPDATA"] = dir.path.string();
#else
    environment.values["XDG_DATA_HOME"] = dir.path.string();
#endif
    RecordingFiles files;
    RecordingSession session;
    session.fail = true;
    auto opened = openUserDatabase(environment, files, session);
    CHECK_FALSE(opened.succeeded());
    CHECK(opened.error() == "Failed to open database: boom");
}

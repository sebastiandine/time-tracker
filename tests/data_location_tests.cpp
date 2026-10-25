#include "support/MapEnvironment.hpp"
#include "support/MemoryFilesystem.hpp"
#include "support/TempDir.hpp"

#include "timetracker/config/ConfigService.hpp"
#include "timetracker/paths/DataPaths.hpp"
#include "timetracker/ports/IDatabaseSession.hpp"
#include "timetracker/services/DataLocationService.hpp"

#include <doctest/doctest.h>

#include <sstream>
#include <string>
#include <vector>

using namespace timetracker;

namespace {

class ScriptedSession final : public IDatabaseSession {
public:
    std::vector<std::filesystem::path> opened;
    bool failOther{false};
    bool failAll{false};

    [[nodiscard]] Result<void> open(const std::filesystem::path& dbPath) override {
        opened.push_back(dbPath);
        if (failAll || (failOther && dbPath.parent_path().filename() == "other")) {
            return Result<void>::err("Failed to open database: boom");
        }
        return Result<void>::ok();
    }
};

struct Harness {
    test::TempDir home;
    test::MapEnvironment environment;
    test::MemoryFilesystem files;
    ConfigLocations locations;
    ConfigService config;
    ScriptedSession session;
    std::filesystem::path current;

    Harness() {
        files.createRealDirectories = true;
        const auto root = home.path / "local";
        std::filesystem::create_directories(root);
#if defined(_WIN32)
        environment.values["LOCALAPPDATA"] = root.string();
#else
        environment.values["XDG_DATA_HOME"] = root.string();
#endif
        locations.system = home.path / "system";
        locations.user = home.path / "user";
        locations.executable = home.path / "exe";
        std::ostringstream log;
        REQUIRE(config.load(environment, files, locations, {}, log).succeeded());
        current = config.dataDirectory();
        std::filesystem::create_directories(current);
    }
};

}  // namespace

TEST_CASE("selecting the open directory does nothing") {
    Harness harness;
    DataLocationService locations(harness.config, harness.files, harness.session);
    auto selected = locations.selectDirectory(harness.current);
    REQUIRE(selected.succeeded());
    CHECK_FALSE(selected.value().switched);
    CHECK_FALSE(selected.value().overridden);
    CHECK(harness.session.opened.empty());
}

TEST_CASE("a relative data directory is rejected before the database opens") {
    Harness harness;
    DataLocationService locations(harness.config, harness.files, harness.session);
    auto selected = locations.selectDirectory("relative");
    CHECK_FALSE(selected.succeeded());
    CHECK(selected.error() == "dataDirectory must be an absolute path");
    CHECK(harness.session.opened.empty());
    CHECK(locations.directory() == harness.current);
}

TEST_CASE("selecting an empty directory opens a new database and saves the choice") {
    Harness harness;
    const auto other = harness.home.path / "other";
    DataLocationService locations(harness.config, harness.files, harness.session);
    auto selected = locations.selectDirectory(other);
    REQUIRE(selected.succeeded());
    CHECK(selected.value().switched);
    CHECK_FALSE(selected.value().overridden);
    REQUIRE(harness.session.opened.size() == 1);
    CHECK(harness.session.opened.back().filename() == "time_tracker.db");
    CHECK(harness.session.opened.back().parent_path().filename() == "other");
    CHECK(locations.directory() == other.lexically_normal());
}

TEST_CASE("a failed open leaves the previous database in place") {
    Harness harness;
    harness.session.failOther = true;
    const auto other = harness.home.path / "other";
    DataLocationService locations(harness.config, harness.files, harness.session);
    auto selected = locations.selectDirectory(other);
    CHECK_FALSE(selected.succeeded());
    CHECK(selected.error() == "Failed to open database: boom");
    REQUIRE(harness.session.opened.size() == 2);
    CHECK(harness.session.opened.back().parent_path() == harness.current);
    CHECK(locations.directory() == harness.current);
}

TEST_CASE("a failed save reopens the previous database") {
    Harness harness;
    harness.files.failReplaceRemaining = 2;
    const auto other = harness.home.path / "other";
    DataLocationService locations(harness.config, harness.files, harness.session);
    auto selected = locations.selectDirectory(other);
    CHECK_FALSE(selected.succeeded());
    CHECK(selected.error().find("replace failed") != std::string::npos);
    REQUIRE(harness.session.opened.size() == 2);
    CHECK(harness.session.opened.back().parent_path() == harness.current);
    CHECK(locations.directory() == harness.current);
}

TEST_CASE("a higher layer keeps the open database and still saves the file") {
    Harness harness;
    const auto forced = harness.home.path / "forced";
    std::filesystem::create_directories(forced);
    harness.environment.values["TIME_TRACKER_DATA_DIRECTORY"] = forced.generic_string();
    std::ostringstream log;
    ConfigService config;
    REQUIRE(config.load(harness.environment, harness.files, harness.locations, {}, log).succeeded());
    CHECK(config.dataDirectory() == forced);

    ScriptedSession session;
    DataLocationService locations(config, harness.files, session);
    const auto other = harness.home.path / "other";
    auto selected = locations.selectDirectory(other);
    REQUIRE(selected.succeeded());
    CHECK(selected.value().overridden);
    CHECK_FALSE(selected.value().switched);
    CHECK(selected.value().notice.find("layer process-env") != std::string::npos);
    CHECK(session.opened.empty());
    CHECK(locations.directory() == forced);
    const auto text = harness.files.files.at((harness.locations.executable / "config.json").generic_string());
    CHECK(text.find(other.lexically_normal().generic_string()) != std::string::npos);
    CHECK(text.find(forced.generic_string()) == std::string::npos);
}

TEST_CASE("a failed restore reports both errors") {
    Harness harness;
    harness.session.failAll = true;
    DataLocationService locations(harness.config, harness.files, harness.session);
    auto selected = locations.selectDirectory(harness.home.path / "other");
    CHECK_FALSE(selected.succeeded());
    CHECK(selected.error().find("Failed to open database: boom") != std::string::npos);
    CHECK(harness.session.opened.size() == 2);
}

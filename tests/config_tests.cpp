#include "support/MapEnvironment.hpp"
#include "support/MemoryFilesystem.hpp"
#include "support/TempDir.hpp"

#include "timetracker/config/ConfigFile.hpp"
#include "timetracker/config/ConfigService.hpp"
#include "timetracker/json/DocumentIo.hpp"
#include "timetracker/paths/DataPaths.hpp"

#include <doctest/doctest.h>

#include <sstream>
#include <string>

using namespace timetracker;

namespace {

[[nodiscard]] std::filesystem::path absPath(std::string_view name) {
#if defined(_WIN32)
    return std::filesystem::path("C:/time-tracker-test") / std::string{name};
#else
    return std::filesystem::path("/time-tracker-test") / std::string{name};
#endif
}

[[nodiscard]] ConfigLocations testLocations() {
    ConfigLocations locations;
    locations.system = absPath("system");
    locations.user = absPath("user");
    locations.executable = absPath("exe");
    return locations;
}

void putConfig(test::MemoryFilesystem& files, const std::filesystem::path& directory,
               const std::filesystem::path& dataDirectory) {
    ConfigFile file;
    file.dataDirectory = dataDirectory.generic_string();
    REQUIRE(storeJson(files, directory / "config.json", file).succeeded());
}

[[nodiscard]] std::string fileText(const test::MemoryFilesystem& files, const std::filesystem::path& path) {
    const auto found = files.files.find(path.generic_string());
    REQUIRE(found != files.files.end());
    return found->second;
}

}  // namespace

TEST_CASE("missing config files create the default directory beside the executable") {
    test::TempDir home;
    test::MapEnvironment environment;
#if defined(_WIN32)
    environment.values["LOCALAPPDATA"] = home.path.string();
#else
    environment.values["XDG_DATA_HOME"] = home.path.string();
#endif
    test::MemoryFilesystem files;
    const auto locations = testLocations();
    ConfigService config;
    std::ostringstream log;
    REQUIRE(config.load(environment, files, locations, {}, log).succeeded());

    auto expected = userDataDirectory(environment);
    REQUIRE(expected.succeeded());
    CHECK(config.dataDirectory() == expected.value());
    CHECK(config.layer() == ConfigLayer::Default);
    CHECK(log.str() == "config dataDirectory=" + expected.value().generic_string() + " layer=default\n");
    const auto written = fileText(files, locations.executable / "config.json");
    CHECK(written.find(expected.value().generic_string()) != std::string::npos);
    CHECK_FALSE(files.exists(locations.user / "config.json"));
    CHECK_FALSE(files.exists(locations.system / "config.json"));
}

TEST_CASE("an unwritable executable directory writes the user config and not the system config") {
    test::TempDir home;
    test::MapEnvironment environment;
#if defined(_WIN32)
    environment.values["LOCALAPPDATA"] = home.path.string();
#else
    environment.values["XDG_DATA_HOME"] = home.path.string();
#endif
    test::MemoryFilesystem files;
    const auto locations = testLocations();
    files.failPrefixes.push_back(locations.executable.generic_string());
    ConfigService config;
    std::ostringstream log;
    REQUIRE(config.load(environment, files, locations, {}, log).succeeded());
    auto expected = userDataDirectory(environment);
    REQUIRE(expected.succeeded());
    CHECK(fileText(files, locations.user / "config.json").find(expected.value().generic_string()) !=
          std::string::npos);
    CHECK_FALSE(files.exists(locations.executable / "config.json"));
    CHECK_FALSE(files.exists(locations.system / "config.json"));
}

TEST_CASE("system config is overridden by user config and then executable config") {
    test::MapEnvironment environment;
    test::MemoryFilesystem files;
    const auto locations = testLocations();
    putConfig(files, locations.system, absPath("from-system"));
    putConfig(files, locations.user, absPath("from-user"));
    putConfig(files, locations.executable, absPath("from-exe"));
    ConfigService config;
    std::ostringstream log;
    REQUIRE(config.load(environment, files, locations, {}, log).succeeded());
    CHECK(config.dataDirectory() == absPath("from-exe"));
    CHECK(config.layer() == ConfigLayer::ExeFile);
    CHECK(log.str().find("layer=exe-file") != std::string::npos);
}

TEST_CASE("a system dotenv overrides an executable config file") {
    test::MapEnvironment environment;
    test::MemoryFilesystem files;
    const auto locations = testLocations();
    putConfig(files, locations.executable, absPath("from-file"));
    const auto dotenv = absPath("from-dotenv");
    files.files[(locations.system / ".env").generic_string()] =
        "TIME_TRACKER_DATA_DIRECTORY=" + dotenv.generic_string() + "\n";
    ConfigService config;
    std::ostringstream log;
    REQUIRE(config.load(environment, files, locations, {}, log).succeeded());
    CHECK(config.dataDirectory() == dotenv);
    CHECK(config.layer() == ConfigLayer::SystemDotenv);
}

TEST_CASE("an executable dotenv overrides a user dotenv") {
    test::MapEnvironment environment;
    test::MemoryFilesystem files;
    const auto locations = testLocations();
    files.files[(locations.user / ".env").generic_string()] =
        "TIME_TRACKER_DATA_DIRECTORY=" + absPath("from-user-env").generic_string() + "\n";
    files.files[(locations.executable / ".env").generic_string()] =
        "export TIME_TRACKER_DATA_DIRECTORY=\"" + absPath("from-exe-env").generic_string() + "\" # note\n";
    ConfigService config;
    std::ostringstream log;
    REQUIRE(config.load(environment, files, locations, {}, log).succeeded());
    CHECK(config.dataDirectory() == absPath("from-exe-env"));
    CHECK(config.layer() == ConfigLayer::ExeDotenv);
}

TEST_CASE("process environment overrides dotenv and the command line overrides process environment") {
    test::MapEnvironment environment;
    test::MemoryFilesystem files;
    const auto locations = testLocations();
    files.files[(locations.system / ".env").generic_string()] =
        "TIME_TRACKER_DATA_DIRECTORY=" + absPath("from-dotenv").generic_string() + "\n";
    environment.values["TIME_TRACKER_DATA_DIRECTORY"] = absPath("from-process").generic_string();
    environment.values["PATH"] = absPath("ignored").generic_string();
    ConfigService config;
    std::ostringstream log;
    REQUIRE(config.load(environment, files, locations, {}, log).succeeded());
    CHECK(config.dataDirectory() == absPath("from-process"));
    CHECK(config.layer() == ConfigLayer::ProcessEnv);

    ConfigService flagged;
    const std::vector<std::string> arguments{"notes.txt", "--help", "--data-directory",
                                             absPath("from-cli").generic_string()};
    REQUIRE(flagged.load(environment, files, locations, arguments, log).succeeded());
    CHECK(flagged.dataDirectory() == absPath("from-cli"));
    CHECK(flagged.layer() == ConfigLayer::Cli);
}

TEST_CASE("the data-directory flag accepts an equals form") {
    test::MapEnvironment environment;
    test::MemoryFilesystem files;
    ConfigService config;
    std::ostringstream log;
    const auto directory = absPath("equals");
    const std::vector<std::string> arguments{"--data-directory=" + directory.generic_string(),
                                             "--version"};
    REQUIRE(config.load(environment, files, testLocations(), arguments, log).succeeded());
    CHECK(config.dataDirectory() == directory);
    CHECK(config.layer() == ConfigLayer::Cli);
}

TEST_CASE("an unknown config key names the key and the layer") {
    test::MapEnvironment environment;
    test::MemoryFilesystem files;
    const auto locations = testLocations();
    files.files[(locations.executable / "config.json").generic_string()] =
        "{\"dataDirectory\": \"" + absPath("ok").generic_string() + "\", \"typoTheme\": \"Dark\"}\n";
    ConfigService config;
    std::ostringstream log;
    auto loaded = config.load(environment, files, locations, {}, log);
    CHECK_FALSE(loaded.succeeded());
    CHECK(loaded.error().find("unknown key 'typoTheme' in layer exe-file") != std::string::npos);
}

TEST_CASE("a malformed config file fails instead of being skipped") {
    test::MapEnvironment environment;
    test::MemoryFilesystem files;
    const auto locations = testLocations();
    files.files[(locations.user / "config.json").generic_string()] = "{";
    ConfigService config;
    std::ostringstream log;
    auto loaded = config.load(environment, files, locations, {}, log);
    CHECK_FALSE(loaded.succeeded());
    CHECK(loaded.error().find("in layer user-file") != std::string::npos);
}

TEST_CASE("a relative data directory is rejected") {
    test::MapEnvironment environment;
    test::MemoryFilesystem files;
    const auto locations = testLocations();
    files.files[(locations.executable / "config.json").generic_string()] =
        "{\"schemaVersion\": 1, \"dataDirectory\": \"relative/dir\"}\n";
    ConfigService config;
    std::ostringstream log;
    auto loaded = config.load(environment, files, locations, {}, log);
    CHECK_FALSE(loaded.succeeded());
    CHECK(loaded.error() == "dataDirectory must be an absolute path");
}

TEST_CASE("a prefixed unknown environment variable fails and an unprefixed one is ignored") {
    test::MapEnvironment environment;
    test::MemoryFilesystem files;
    const auto locations = testLocations();
    putConfig(files, locations.executable, absPath("from-file"));
    environment.values["TIME_TRACKER_THEME"] = "Dark";
    ConfigService config;
    std::ostringstream log;
    auto loaded = config.load(environment, files, locations, {}, log);
    CHECK_FALSE(loaded.succeeded());
    CHECK(loaded.error().find("unknown key 'TIME_TRACKER_THEME' in layer process-env") !=
          std::string::npos);

    environment.values.erase("TIME_TRACKER_THEME");
    environment.values["PATH"] = "C:/ignored";
    ConfigService again;
    REQUIRE(again.load(environment, files, locations, {}, log).succeeded());
    CHECK(again.dataDirectory() == absPath("from-file"));
}

TEST_CASE("an unknown flag and a missing flag value fail") {
    test::MapEnvironment environment;
    test::MemoryFilesystem files;
    const auto locations = testLocations();
    putConfig(files, locations.executable, absPath("from-file"));
    ConfigService config;
    std::ostringstream log;
    auto unknown = config.load(environment, files, locations, {"--theme"}, log);
    CHECK_FALSE(unknown.succeeded());
    CHECK(unknown.error().find("unknown flag '--theme' in layer cli") != std::string::npos);

    ConfigService missing;
    auto bare = missing.load(environment, files, locations, {"--data-directory"}, log);
    CHECK_FALSE(bare.succeeded());
    CHECK(bare.error().find("missing value for --data-directory") != std::string::npos);
}

TEST_CASE("a malformed dotenv line names its layer") {
    test::MapEnvironment environment;
    test::MemoryFilesystem files;
    const auto locations = testLocations();
    files.files[(locations.user / ".env").generic_string()] = "not a pair\n";
    ConfigService config;
    std::ostringstream log;
    auto loaded = config.load(environment, files, locations, {}, log);
    CHECK_FALSE(loaded.succeeded());
    CHECK(loaded.error().find("malformed .env line in layer user-dotenv") != std::string::npos);
}

TEST_CASE("an unknown dotenv key names the key and the layer") {
    test::MapEnvironment environment;
    test::MemoryFilesystem files;
    const auto locations = testLocations();
    files.files[(locations.executable / ".env").generic_string()] = "OTHER=1\n";
    ConfigService config;
    std::ostringstream log;
    auto loaded = config.load(environment, files, locations, {}, log);
    CHECK_FALSE(loaded.succeeded());
    CHECK(loaded.error().find("unknown key 'OTHER' in layer exe-dotenv") != std::string::npos);
}

TEST_CASE("store writes the file layer and leaves environment values out of the file") {
    test::MapEnvironment environment;
    test::MemoryFilesystem files;
    const auto locations = testLocations();
    putConfig(files, locations.executable, absPath("from-file"));
    environment.values["TIME_TRACKER_DATA_DIRECTORY"] = absPath("from-process").generic_string();
    ConfigService config;
    std::ostringstream log;
    REQUIRE(config.load(environment, files, locations, {}, log).succeeded());
    CHECK(config.dataDirectory() == absPath("from-process"));

    const auto chosen = absPath("from-menu");
    REQUIRE(config.storeDataDirectory(files, chosen).succeeded());
    CHECK(config.dataDirectory() == absPath("from-process"));
    CHECK(config.layer() == ConfigLayer::ProcessEnv);
    const auto text = fileText(files, locations.executable / "config.json");
    CHECK(text.find(chosen.generic_string()) != std::string::npos);
    CHECK(text.find(absPath("from-process").generic_string()) == std::string::npos);
}

TEST_CASE("dotenv does not get copied into a newly created config file") {
    test::TempDir home;
    test::MapEnvironment environment;
#if defined(_WIN32)
    environment.values["LOCALAPPDATA"] = home.path.string();
#else
    environment.values["XDG_DATA_HOME"] = home.path.string();
#endif
    test::MemoryFilesystem files;
    const auto locations = testLocations();
    const auto fromDotenv = absPath("from-dotenv");
    files.files[(locations.system / ".env").generic_string()] =
        "TIME_TRACKER_DATA_DIRECTORY=" + fromDotenv.generic_string() + "\n";
    ConfigService config;
    std::ostringstream log;
    REQUIRE(config.load(environment, files, locations, {}, log).succeeded());
    CHECK(config.dataDirectory() == fromDotenv);
    CHECK(config.layer() == ConfigLayer::SystemDotenv);
    auto expected = userDataDirectory(environment);
    REQUIRE(expected.succeeded());
    const auto text = fileText(files, locations.executable / "config.json");
    CHECK(text.find(expected.value().generic_string()) != std::string::npos);
    CHECK(text.find(fromDotenv.generic_string()) == std::string::npos);
}

#if defined(_WIN32)

TEST_CASE("config directories use ProgramData and APPDATA") {
    test::MapEnvironment environment;
    auto located = configLocations(environment, absPath("exe"));
    CHECK_FALSE(located.succeeded());
    CHECK(located.error() == "ProgramData is not set");

    environment.values["ProgramData"] = absPath("program").string();
    located = configLocations(environment, absPath("exe"));
    CHECK_FALSE(located.succeeded());
    CHECK(located.error() == "APPDATA is not set");

    environment.values["APPDATA"] = absPath("roaming").string();
    located = configLocations(environment, absPath("exe"));
    REQUIRE(located.succeeded());
    CHECK(located.value().system.filename() == "Time Tracker");
    CHECK(located.value().user.filename() == "Time Tracker");
    CHECK(located.value().executable == absPath("exe"));
}

#else

TEST_CASE("config directories use /etc and the XDG config home") {
    test::MapEnvironment environment;
    auto located = configLocations(environment, absPath("exe"));
    CHECK_FALSE(located.succeeded());
    CHECK(located.error() == "HOME is not set");

    environment.values["XDG_CONFIG_HOME"] = absPath("xdg").string();
    located = configLocations(environment, absPath("exe"));
    REQUIRE(located.succeeded());
    CHECK(located.value().system == "/etc/time-tracker");
    CHECK(located.value().user == absPath("xdg") / "time-tracker");

    environment.values.erase("XDG_CONFIG_HOME");
    environment.values["HOME"] = absPath("home").string();
    located = configLocations(environment, absPath("exe"));
    REQUIRE(located.succeeded());
    CHECK(located.value().user.generic_string().find("/.config/time-tracker") != std::string::npos);
}

#endif

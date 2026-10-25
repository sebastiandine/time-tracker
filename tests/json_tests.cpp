#include "support/MemoryFilesystem.hpp"

#include "timetracker/config/ConfigFile.hpp"
#include "timetracker/json/DocumentIo.hpp"
#include "timetracker/json/JsonObjectBag.hpp"

#include <doctest/doctest.h>

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>

using namespace timetracker;

namespace {

struct Probe {
    std::string mode;
};

void from_json(const nlohmann::json& json, Probe& probe) {
    probe.mode = json.at("mode").get<std::string>();
    if (probe.mode == "range") {
        throw std::out_of_range("range");
    }
    if (probe.mode == "bug") {
        throw std::runtime_error("bug");
    }
    if (probe.mode == "argument") {
        throw std::invalid_argument("Unknown Probe value: x");
    }
}

[[nodiscard]] std::filesystem::path samplePath() {
#if defined(_WIN32)
    return std::filesystem::path("C:/time-tracker-test/sample.json");
#else
    return std::filesystem::path("/time-tracker-test/sample.json");
#endif
}

}  // namespace

TEST_CASE("unknown json members survive in the bag and do not replace known keys") {
    nlohmann::json source = {{"schemaVersion", 1}, {"extra", "kept"}};
    JsonObjectBag bag;
    absorbUnknown(bag, source, {"schemaVersion"});
    nlohmann::json emitted = {{"schemaVersion", 1}};
    emitUnknown(emitted, bag);
    CHECK(emitted.at("extra") == "kept");
    CHECK(emitted.at("schemaVersion") == 1);

    nlohmann::json occupied = {{"schemaVersion", 2}, {"extra", "original"}};
    emitUnknown(occupied, bag);
    CHECK(occupied.at("extra") == "original");

    JsonObjectBag copy = bag;
    CHECK(copy == bag);
}

TEST_CASE("a config file round-trips with a trailing newline") {
    test::MemoryFilesystem files;
    ConfigFile file;
    file.schemaVersion = kConfigSchema;
    file.dataDirectory = "/data/TimeTracker";
    const auto path = samplePath();
    auto stored = storeJson(files, path, file);
    REQUIRE(stored.succeeded());
    const auto& text = files.files.at(path.generic_string());
    CHECK(text.ends_with('\n'));
    CHECK(text.find("\n  \"") != std::string::npos);
    CHECK(text.find("\"schemaVersion\"") != std::string::npos);
    CHECK(text.find("\"dataDirectory\"") != std::string::npos);
    CHECK(text.find("/data/TimeTracker") != std::string::npos);

    auto loaded = loadJson<ConfigFile>(files, path);
    REQUIRE(loaded.succeeded());
    CHECK(loaded.value() == file);
}

TEST_CASE("a config file without schemaVersion loads as version 1") {
    test::MemoryFilesystem files;
    const auto path = samplePath();
    files.files[path.generic_string()] = "{\n  \"dataDirectory\": \"/data/TimeTracker\"\n}\n";
    auto loaded = loadJson<ConfigFile>(files, path);
    REQUIRE(loaded.succeeded());
    CHECK(loaded.value().schemaVersion == kConfigSchema);
    CHECK(loaded.value().dataDirectory == "/data/TimeTracker");
}

TEST_CASE("an unsupported schemaVersion is a type error") {
    test::MemoryFilesystem files;
    const auto path = samplePath();
    files.files[path.generic_string()] = "{\"schemaVersion\": 9, \"dataDirectory\": \"/data\"}\n";
    auto loaded = loadJson<ConfigFile>(files, path);
    CHECK_FALSE(loaded.succeeded());
    CHECK(loaded.error().kind == JsonFileErrc::Type);
    CHECK(loaded.error().message.find("unsupported schemaVersion") != std::string::npos);
}

TEST_CASE("a config file rejects an unknown key") {
    test::MemoryFilesystem files;
    const auto path = samplePath();
    files.files[path.generic_string()] =
        "{\"schemaVersion\": 1, \"dataDirectory\": \"/data\", \"typoTheme\": \"Dark\"}\n";
    auto loaded = loadJson<ConfigFile>(files, path);
    CHECK_FALSE(loaded.succeeded());
    CHECK(loaded.error().kind == JsonFileErrc::Type);
    CHECK(loaded.error().message.find("unknown key 'typoTheme'") != std::string::npos);
}

TEST_CASE("the wrong json type for dataDirectory is a type error") {
    test::MemoryFilesystem files;
    const auto path = samplePath();
    files.files[path.generic_string()] = "{\"schemaVersion\": 1, \"dataDirectory\": 12}\n";
    auto loaded = loadJson<ConfigFile>(files, path);
    CHECK_FALSE(loaded.succeeded());
    CHECK(loaded.error().kind == JsonFileErrc::Type);
}

TEST_CASE("bytes that are not json are malformed") {
    test::MemoryFilesystem files;
    const auto path = samplePath();
    files.files[path.generic_string()] = "{";
    auto loaded = loadJson<ConfigFile>(files, path);
    CHECK_FALSE(loaded.succeeded());
    CHECK(loaded.error().kind == JsonFileErrc::Malformed);
}

TEST_CASE("a missing required json key is MissingKey") {
    test::MemoryFilesystem files;
    const auto path = samplePath();
    files.files[path.generic_string()] = "{}\n";
    auto loaded = loadJson<Probe>(files, path);
    CHECK_FALSE(loaded.succeeded());
    CHECK(loaded.error().kind == JsonFileErrc::MissingKey);
    CHECK(loaded.error().message.find("mode") != std::string::npos);
}

TEST_CASE("json exception kinds stay distinct") {
    test::MemoryFilesystem files;
    const auto path = samplePath();
    files.files[path.generic_string()] = "{\"mode\": 1}\n";
    auto typed = loadJson<Probe>(files, path);
    CHECK_FALSE(typed.succeeded());
    CHECK(typed.error().kind == JsonFileErrc::Type);

    files.files[path.generic_string()] = "{\"mode\": \"range\"}\n";
    auto ranged = loadJson<Probe>(files, path);
    CHECK_FALSE(ranged.succeeded());
    CHECK(ranged.error().kind == JsonFileErrc::Type);

    files.files[path.generic_string()] = "{\"mode\": \"argument\"}\n";
    auto argument = loadJson<Probe>(files, path);
    CHECK_FALSE(argument.succeeded());
    CHECK(argument.error().kind == JsonFileErrc::Type);
    CHECK(argument.error().message.find("Unknown Probe value: ") == 0);

    files.files[path.generic_string()] = "{\"mode\": \"bug\"}\n";
    auto bug = loadJson<Probe>(files, path);
    CHECK_FALSE(bug.succeeded());
    CHECK(bug.error().kind == JsonFileErrc::Malformed);
}

TEST_CASE("a failed replace leaves the previous json bytes") {
    test::MemoryFilesystem files;
    const auto path = samplePath();
    ConfigFile original;
    original.dataDirectory = "/data/old";
    REQUIRE(storeJson(files, path, original).succeeded());
    const auto previous = files.files.at(path.generic_string());
    files.failReplaceRemaining = 1;
    ConfigFile replacement;
    replacement.dataDirectory = "/data/new";
    auto stored = storeJson(files, path, replacement);
    CHECK_FALSE(stored.succeeded());
    CHECK(stored.error().kind == JsonFileErrc::Io);
    CHECK(files.files.at(path.generic_string()) == previous);
    CHECK_FALSE(files.files.contains(path.generic_string() + ".tmp"));
}

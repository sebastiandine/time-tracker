#include "timetracker/config/ConfigFile.hpp"

#include <nlohmann/json.hpp>

#include <stdexcept>

namespace timetracker {
namespace {

[[nodiscard]] nlohmann::json migrateConfig(nlohmann::json document) {
    if (!document.is_object()) {
        throw std::invalid_argument("config root must be a JSON object");
    }
    int version = 1;
    if (document.contains("schemaVersion")) {
        const auto& stored = document.at("schemaVersion");
        if (!stored.is_number_integer()) {
            throw std::invalid_argument("unsupported schemaVersion");
        }
        version = stored.get<int>();
    }
    if (version < 1 || version > kConfigSchema) {
        throw std::invalid_argument("unsupported schemaVersion " + std::to_string(version) +
                                    " (this build supports 1.." + std::to_string(kConfigSchema) +
                                    ")");
    }
    document["schemaVersion"] = kConfigSchema;
    return document;
}

[[nodiscard]] bool knownConfigKey(const std::string& key) {
    return key == "schemaVersion" || key == "dataDirectory";
}

}  // namespace

void to_json(nlohmann::json& json, const ConfigFile& file) {
    json = nlohmann::json::object();
    json["schemaVersion"] = file.schemaVersion;
    if (file.dataDirectory.has_value()) {
        json["dataDirectory"] = *file.dataDirectory;
    }
}

void from_json(const nlohmann::json& json, ConfigFile& file) {
    const auto migrated = migrateConfig(json);
    for (auto it = migrated.begin(); it != migrated.end(); ++it) {
        if (!knownConfigKey(it.key())) {
            throw std::invalid_argument("unknown key '" + it.key() + "'");
        }
    }
    file.schemaVersion = kConfigSchema;
    file.dataDirectory.reset();
    if (migrated.contains("dataDirectory")) {
        file.dataDirectory = migrated.at("dataDirectory").get<std::string>();
    }
}

}  // namespace timetracker

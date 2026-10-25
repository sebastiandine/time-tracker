#pragma once

#include <nlohmann/json_fwd.hpp>

#include <optional>
#include <string>

namespace timetracker {

/**
 * @brief Schema version written into @c config.json.
 *
 * Missing @c schemaVersion loads as version 1. A higher number is rejected.
 */
inline constexpr int kConfigSchema = 1;

/**
 * @brief On-disk configuration document.
 *
 * Cascade: defaults, then @c config.json in the system, user, and executable
 * directories, then @c .env in that same order, then process environment, then
 * the command line. @c ConfigService applies that cascade. This type is only
 * the file.
 *
 *   dataDirectory  string  default per-user TimeTracker directory
 *                  env TIME_TRACKER_DATA_DIRECTORY  flag --data-directory
 *
 * @code
 * { "dataDirectory": "/data/TimeTracker", "schemaVersion": 1 }
 * @endcode
 *
 * Unknown keys fail and are not written back. There are no secret fields.
 */
struct ConfigFile {
    int schemaVersion{kConfigSchema};                 /**< Document version. */
    std::optional<std::string> dataDirectory;         /**< Absolute data directory, when set. */

    /**
     * @brief Compare both members.
     * @param left First document.
     * @param right Second document.
     * @return True when version and directory match.
     */
    friend bool operator==(const ConfigFile& left, const ConfigFile& right) = default;
};

/**
 * @brief Write @p file as a JSON object.
 * @param json Destination object.
 * @param file Document to store. Omitted directory stays omitted.
 */
void to_json(nlohmann::json& json, const ConfigFile& file);

/**
 * @brief Read @p file from a JSON object.
 * @param json Source object. Unknown keys and a future schema version throw.
 * @param file Destination. A missing @c dataDirectory leaves the optional empty.
 * @throws std::invalid_argument Unknown key or unsupported @c schemaVersion.
 */
void from_json(const nlohmann::json& json, ConfigFile& file);

}  // namespace timetracker

#pragma once

#include <string>

namespace timetracker {

/**
 * @brief Why a JSON file could not be loaded or stored.
 */
enum class JsonFileErrc {
    Io,         /**< Filesystem: missing file, read, write, fsync, or replace. */
    Malformed,  /**< The bytes are not a JSON document. */
    Type,       /**< Wrong JSON type, unknown value, or unsupported schemaVersion. */
    MissingKey, /**< A required key is absent. */
};

/**
 * @brief Classified JSON file failure.
 *
 * Aggregate. A successful result default-constructs this value; callers read
 * it only after failure.
 */
struct JsonFileError {
    JsonFileErrc kind{}; /**< Classification. */
    std::string message; /**< Text safe to show. Includes the path for I/O. */
};

}  // namespace timetracker

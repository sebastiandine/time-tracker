#pragma once

#include "timetracker/util/Result.hpp"

#include <filesystem>

namespace timetracker {

/**
 * @brief Opens the application database.
 *
 * Implementations own the connection. Not thread-safe. No SQLite types cross
 * this port.
 */
class IDatabaseSession {
public:
    virtual ~IDatabaseSession() = default;

    /**
     * @brief Open or create a database file.
     * @param dbPath Filesystem path. The parent directory must already exist.
     * @return Success, or an error when the file cannot be opened or migrated.
     */
    [[nodiscard]] virtual Result<void> open(const std::filesystem::path& dbPath) = 0;
};

}  // namespace timetracker

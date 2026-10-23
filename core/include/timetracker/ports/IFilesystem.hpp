#pragma once

#include "timetracker/util/Result.hpp"

#include <filesystem>

namespace timetracker {

/**
 * @brief Creates directories for the application data root.
 *
 * Not thread-safe.
 */
class IFilesystem {
public:
    virtual ~IFilesystem() = default;

    /**
     * @brief Create @p directory and any missing parents.
     * @param directory Absolute or relative directory. Existing directories succeed.
     * @return Success, or an error when the directory cannot be created.
     */
    [[nodiscard]] virtual Result<void> ensureDirectory(
        const std::filesystem::path& directory) = 0;
};

}  // namespace timetracker

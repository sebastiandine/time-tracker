#pragma once

#include "timetracker/util/Result.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace timetracker {

/**
 * @brief Creates directories and reads or replaces whole text files.
 *
 * Not thread-safe. Text methods read and write the full file. They do not
 * follow a path out of a caller-chosen location; confinement is the caller's job.
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

    /**
     * @brief Report whether a file or directory is present.
     * @param path File or directory path.
     * @return True when the path exists.
     */
    [[nodiscard]] virtual bool exists(const std::filesystem::path& path) const = 0;

    /**
     * @brief Read an entire file as bytes interpreted as text.
     * @param path File path. A missing file is an error, not an empty string.
     * @return The file bytes, or an error that includes the path.
     */
    [[nodiscard]] virtual Result<std::string> readText(const std::filesystem::path& path) = 0;

    /**
     * @brief Replace @p destination with @p contents.
     *
     * Writes a sibling temp file, flushes it, then replaces the destination.
     * On failure before the replace, the previous destination bytes stay.
     * The temp path does not remain after success or after a failed replace.
     * @param destination Final path. Parent directories may be created.
     * @param contents Full new contents. Not appended.
     * @return Success, or an error that includes the path. A directory fsync
     *         failure after a POSIX replace says @c after replace.
     */
    [[nodiscard]] virtual Result<void> replaceText(const std::filesystem::path& destination,
                                                   std::string_view contents) = 0;
};

}  // namespace timetracker

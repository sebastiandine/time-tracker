#pragma once

#include "timetracker/util/Result.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace timetracker {

/**
 * @brief Report whether @p path exists.
 * @param path File or directory. Errors from the filesystem count as absent.
 * @return True when the path is present.
 */
[[nodiscard]] bool hostPathExists(const std::filesystem::path& path);

/**
 * @brief Read a whole file from disk.
 * @param path File path.
 * @return The bytes, or an error naming the path when the file is missing or unreadable.
 */
[[nodiscard]] Result<std::string> readHostText(const std::filesystem::path& path);

/**
 * @brief Atomically replace a text file on disk.
 *
 * See @c IFilesystem::replaceText. Windows uses @c MoveFileExW. POSIX renames
 * and then fsyncs the parent directory.
 * @param destination Final path.
 * @param contents Full new contents.
 * @return Success, or an error naming the path.
 */
[[nodiscard]] Result<void> replaceHostText(const std::filesystem::path& destination,
                                           std::string_view contents);

}  // namespace timetracker

#pragma once

#include "timetracker/ports/IDatabaseSession.hpp"
#include "timetracker/ports/IEnvironment.hpp"
#include "timetracker/ports/IFilesystem.hpp"
#include "timetracker/util/Result.hpp"

#include <filesystem>
#include <string_view>

namespace timetracker {

/**
 * @brief Directory name under the operating-system user-data root.
 */
inline constexpr std::string_view kDataDirectoryName = "TimeTracker";

/**
 * @brief Database file name inside the user-data directory.
 */
inline constexpr std::string_view kDatabaseFileName = "time_tracker.db";

/**
 * @brief Report whether @p relative stays inside @p root.
 *
 * Each component of @p relative must be a single name. @c .., an absolute
 * path, and a symlink or junction that resolves outside @p root are rejected.
 * @p root should already exist. A final component that does not exist yet is
 * still accepted when its parent resolves inside the root.
 * @param root Existing directory that anchors the check.
 * @param relative Path joined under @p root. Not a user-chosen absolute path.
 * @return True when the join cannot leave @p root.
 */
[[nodiscard]] bool isConfinedPath(const std::filesystem::path& root,
                                  const std::filesystem::path& relative);

/**
 * @brief Resolve the per-user Time Tracker directory.
 *
 * Windows uses @c LOCALAPPDATA. Other systems use @c XDG_DATA_HOME, or
 * @c HOME/.local/share when that variable is unset. The directory is not created.
 * @param environment Process environment. Tests pass a fake.
 * @return @c .../TimeTracker, or an error when the required variable is missing
 *         or the join would leave the root.
 */
[[nodiscard]] Result<std::filesystem::path> userDataDirectory(const IEnvironment& environment);

/**
 * @brief Join the database file name under @p dataDirectory.
 * @param dataDirectory Directory returned by @c userDataDirectory, or a test double.
 * @return The database path, or an error when the file name would leave the directory.
 */
[[nodiscard]] Result<std::filesystem::path> databasePath(
    const std::filesystem::path& dataDirectory);

/**
 * @brief Create the user-data directory and open the database.
 * @param environment Source of the per-user root.
 * @param files Creates the directory.
 * @param session Receives the database path.
 * @return Success, or the first error from the directory lookup, create, or open.
 */
[[nodiscard]] Result<void> openUserDatabase(const IEnvironment& environment, IFilesystem& files,
                                            IDatabaseSession& session);

/**
 * @brief Create @p directory if needed and open @c time_tracker.db inside it.
 *
 * A missing file is created. Migration builds empty tables and does not insert
 * categories, topics, or time entries.
 * @param directory Directory chosen by configuration or the Data menu. The user's grant.
 * @param files Creates the directory.
 * @param session Receives the database file. A previous connection is closed by the session.
 * @return Success, or the first error from create or open.
 */
[[nodiscard]] Result<void> openDatabaseAt(const std::filesystem::path& directory, IFilesystem& files,
                                          IDatabaseSession& session);

}  // namespace timetracker

#pragma once

#include "timetracker/ports/IEnvironment.hpp"
#include "timetracker/ports/IFilesystem.hpp"
#include "timetracker/util/Result.hpp"

#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

namespace timetracker {

/**
 * @brief Which cascade layer supplied the effective data directory.
 */
enum class ConfigLayer {
    Default,
    SystemFile,
    UserFile,
    ExeFile,
    SystemDotenv,
    UserDotenv,
    ExeDotenv,
    ProcessEnv,
    Cli,
};

/**
 * @brief Directories searched for @c config.json and @c .env.
 *
 * System, then user, then the executable directory. Not the database directory.
 */
struct ConfigLocations {
    std::filesystem::path system;      /**< `%ProgramData%/Time Tracker` or `/etc/time-tracker`. */
    std::filesystem::path user;        /**< `%APPDATA%/Time Tracker` or the XDG config directory. */
    std::filesystem::path executable;  /**< Directory that contains the executable. */
};

/**
 * @brief Name a cascade layer for logs and dialogs.
 * @param layer Winning layer.
 * @return A stable token such as @c exe-file or @c process-env.
 */
[[nodiscard]] const char* configLayerName(ConfigLayer layer);

/**
 * @brief Resolve the three configuration directories.
 * @param environment Process environment. Blank variables count as missing.
 * @param executableDirectory Directory of the executable, not the executable file.
 * @return The three directories, or an error when a required variable is missing.
 */
[[nodiscard]] Result<ConfigLocations> configLocations(
    const IEnvironment& environment, const std::filesystem::path& executableDirectory);

/**
 * @brief Loads and stores the data-directory setting.
 *
 * Load once at startup. External edits apply on the next process start.
 * @c storeDataDirectory writes the file layer only, then reapplies dotenv,
 * process environment, and the command line. Not thread-safe.
 *
 *   dataDirectory  string  default per-user TimeTracker directory
 *                  env TIME_TRACKER_DATA_DIRECTORY  flag --data-directory
 */
class ConfigService {
public:
    /**
     * @brief Load the cascade and log the effective directory.
     * @param environment Source of the default directory and process variables.
     * @param files Reads and, when no config file exists, writes the default file.
     * @param locations System, user, and executable directories.
     * @param arguments Arguments after the program name. Qt options are already removed.
     * @param log Receives one @c config dataDirectory=... line. Not closed.
     * @return Success, or an error that names the key and the layer.
     */
    [[nodiscard]] Result<void> load(const IEnvironment& environment, IFilesystem& files,
                                    const ConfigLocations& locations,
                                    const std::vector<std::string>& arguments, std::ostream& log);

    /**
     * @brief Save a data directory into the file layer.
     * @param files Writes @c config.json beside the executable, or in the user directory.
     * @param directory Absolute directory. A relative path is rejected.
     * @return Success, or an error when the path is relative or the file cannot be written.
     * @pre @c load succeeded.
     */
    [[nodiscard]] Result<void> storeDataDirectory(IFilesystem& files,
                                                  const std::filesystem::path& directory);

    /**
     * @brief Effective data directory after load or store.
     * @return The directory the database should use.
     */
    [[nodiscard]] const std::filesystem::path& dataDirectory() const { return dataDirectory_; }

    /**
     * @brief Layer that won for @c dataDirectory.
     * @return The winning layer.
     */
    [[nodiscard]] ConfigLayer layer() const { return layer_; }

    /**
     * @brief Predict the effective directory if the file layer became @p directory.
     * @param directory Candidate file-layer value. Not written.
     * @param winning Receives the layer that would win.
     * @return The directory that would be effective.
     */
    [[nodiscard]] std::filesystem::path effectiveIfStored(const std::filesystem::path& directory,
                                                          ConfigLayer& winning) const;

private:
    void reapplyOverlays();

    ConfigLocations locations_{};
    std::optional<std::string> systemDotenv_;
    std::optional<std::string> userDotenv_;
    std::optional<std::string> exeDotenv_;
    std::optional<std::string> processDirectory_;
    std::optional<std::string> cliDirectory_;
    std::string fileDirectory_;
    ConfigLayer fileLayer_{ConfigLayer::Default};
    std::filesystem::path dataDirectory_;
    ConfigLayer layer_{ConfigLayer::Default};
};

}  // namespace timetracker

#pragma once

#include "timetracker/config/ConfigService.hpp"
#include "timetracker/ports/IDatabaseSession.hpp"
#include "timetracker/ports/IFilesystem.hpp"
#include "timetracker/util/Result.hpp"

#include <filesystem>
#include <string>

namespace timetracker {

/**
 * @brief Result of choosing a data directory from the Data menu.
 *
 * @c switched means the open database is the chosen directory.
 * @c overridden means a dotenv file, the process environment, or the command
 * line kept the previous database open.
 */
struct DataLocationChoice {
    bool switched{false};     /**< The open database changed. */
    bool overridden{false};   /**< A higher layer ignored the chosen directory. */
    std::string notice;       /**< Why the open database did not change, when overridden. */
};

/**
 * @brief Points the open database at a directory and remembers that choice.
 *
 * The config service, filesystem, and database session must outlive this
 * object. Not thread-safe. A failed open reopens the previous database file.
 */
class DataLocationService {
public:
    /**
     * @brief Bind the service to the open application.
     * @param config Loaded configuration. Not owned.
     * @param files Creates the chosen directory and writes config.
     * @param session Database already opened at @c config's directory. Not owned.
     */
    DataLocationService(ConfigService& config, IFilesystem& files, IDatabaseSession& session);

    /**
     * @brief Directory of the database that is open, or that a higher layer forces.
     * @return Effective data directory.
     */
    [[nodiscard]] const std::filesystem::path& directory() const;

    /**
     * @brief Use @p directory as the data location.
     *
     * An empty, relative, or already open directory does not change the database.
     * When a higher layer would ignore @p directory, the file layer is still saved
     * and the open database stays put. Otherwise the new file is opened first and
     * the choice is saved only after that succeeds.
     * @param directory Absolute directory from the file dialog. The user's grant.
     * @return What changed, or an error when the path is relative, the new database
     *         cannot be opened, or the choice cannot be saved.
     */
    [[nodiscard]] Result<DataLocationChoice> selectDirectory(const std::filesystem::path& directory);

private:
    [[nodiscard]] Result<DataLocationChoice> keepPrevious(const std::filesystem::path& previousFile,
                                                          const std::string& reason);

    ConfigService& config_;
    IFilesystem& files_;
    IDatabaseSession& session_;
};

}  // namespace timetracker
